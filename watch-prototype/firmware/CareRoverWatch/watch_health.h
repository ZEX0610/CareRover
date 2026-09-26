#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace carerover_watch {

struct HealthReading {
  int hr_bpm = 0;
  int spo2_pct = 0;  // Demonstration estimate; wrist calibration is pending.
  int signal_quality = 0;
  bool contact = false;
  bool hr_valid = false, spo2_valid = false;
  bool hr_held = false, spo2_held = false;
  uint32_t hr_age_ms = 0, spo2_age_ms = 0;
};

// Small, self-contained relative-PPG estimator. The car's current estimator
// inspired the 25 Hz, autocorrelation and ratio-of-ratios approach, but wrist
// placement requires its own physical validation and calibration.
class WristHealth {
 public:
  void reset() { *this = WristHealth(); }

  void sample(uint32_t red, uint32_t ir, uint32_t now_ms) {
    if (last_sample_ms_ && now_ms - last_sample_ms_ > 250) reset();
    last_sample_ms_ = now_ms;
    if (ir < 8000) {
      if (++low_ir_count_ >= 5) { reset(); last_sample_ms_ = now_ms; }
      return;
    }
    low_ir_count_ = 0;
    contact_ = true;
    const unsigned index = total_ % kCapacity;
    red_[index] = red;
    ir_[index] = ir;
    times_[index] = now_ms;
    ++total_;
    if (count_ < kCapacity) ++count_;
    if (count_ >= 150 && total_ % 25 == 0) evaluate(now_ms);
  }

  HealthReading reading(uint32_t now_ms) const {
    HealthReading out;
    out.contact = contact_;
    out.signal_quality = quality_;
    if (hr_valid_ && now_ms - hr_fresh_ms_ < 30000) {
      out.hr_bpm = hr_;
      out.hr_valid = true;
      out.hr_age_ms = now_ms - hr_fresh_ms_;
      out.hr_held = out.hr_age_ms > 1500;
    }
    if (spo2_valid_ && now_ms - spo2_fresh_ms_ < 12000) {
      out.spo2_pct = spo2_;
      out.spo2_valid = true;
      out.spo2_age_ms = now_ms - spo2_fresh_ms_;
      out.spo2_held = out.spo2_age_ms > 1500;
    }
    return out;
  }

 private:
  static constexpr unsigned kCapacity = 200;
  static float correlationAt(const float* x, unsigned n, unsigned lag) {
    double dot = 0, left = 0, right = 0;
    for (unsigned i = 0; i + lag < n; ++i) {
      dot += x[i] * x[i + lag];
      left += x[i] * x[i];
      right += x[i + lag] * x[i + lag];
    }
    return left > 0 && right > 0 ?
        float(dot / std::sqrt(left * right)) : -1.0f;
  }

  void evaluate(uint32_t now_ms) {
    const unsigned n = count_ >= 200 ? 200 : 150;
    const unsigned first = (total_ - n) % kCapacity;
    const unsigned last = (total_ - 1) % kCapacity;
    const uint32_t span = times_[last] - times_[first];
    const float hz = span ? (n - 1) * 1000.0f / span : 0;
    if (hz < 20 || hz > 30) return;

    float x[kCapacity], y[kCapacity];
    double mi = 0, mr = 0;
    for (unsigned i = 0; i < n; ++i) {
      const unsigned index = (first + i) % kCapacity;
      mi += ir_[index];
      mr += red_[index];
    }
    mi /= n; mr /= n;
    if (mi < 8000 || mr < 4000) return;
    // Remove the linear baseline between the first and last quarter means.
    double i_start = 0, i_end = 0, r_start = 0, r_end = 0;
    for (unsigned i = 0; i < n / 4; ++i) {
      i_start += ir_[(first + i) % kCapacity];
      i_end += ir_[(first + n - n / 4 + i) % kCapacity];
      r_start += red_[(first + i) % kCapacity];
      r_end += red_[(first + n - n / 4 + i) % kCapacity];
    }
    const float i_slope = float((i_end - i_start) / (n / 4)) / (n * 0.75f);
    const float r_slope = float((r_end - r_start) / (n / 4)) / (n * 0.75f);
    double ei = 0, er = 0, cross = 0;
    for (unsigned i = 0; i < n; ++i) {
      const float center = float(i) - float(n - 1) / 2;
      x[i] = float(ir_[(first + i) % kCapacity] - mi) - i_slope * center;
      y[i] = float(red_[(first + i) % kCapacity] - mr) - r_slope * center;
      ei += x[i] * x[i]; er += y[i] * y[i]; cross += x[i] * y[i];
    }
    if (ei <= 0 || er <= 0) return;
    const float ac_dc = std::sqrt(ei / n) / mi;
    const float red_ac_dc = std::sqrt(er / n) / mr;
    const float red_ir_corr = cross / std::sqrt(ei * er);
    if (ac_dc < 0.00001f || ac_dc > 0.08f) return;

    const unsigned min_lag = unsigned(std::ceil(hz * 60 / 170));
    const unsigned max_lag = unsigned(std::floor(hz * 60 / 45));
    float best = -1; unsigned best_lag = 0;
    for (unsigned lag = min_lag + 1; lag < max_lag; ++lag) {
      const float c = correlationAt(x, n, lag);
      if (c > 0.22f && c >= correlationAt(x, n, lag - 1) &&
          c >= correlationAt(x, n, lag + 1) && c > best) {
        best = c; best_lag = lag;
      }
    }
    quality_ = int(std::lround(100 * std::max(0.0f, std::min(1.0f,
        0.65f * std::max(best, 0.0f) +
        0.35f * std::max(red_ir_corr, 0.0f)))));
    if (best_lag) {
      const int candidate = int(std::lround(60 * hz / best_lag));
      if (candidate >= 45 && candidate <= 170 &&
          (!hr_valid_ || std::abs(candidate - hr_) <= 18)) {
        hr_ = hr_valid_ ? int(std::lround(0.7f * hr_ + 0.3f * candidate)) : candidate;
        hr_valid_ = true; hr_fresh_ms_ = now_ms;
      }
    }
    const float ratio = red_ac_dc / ac_dc;
    const int oxygen = int(std::lround(110 - 25 * ratio));
    if (n >= 200 && red_ir_corr >= 0.20f && ratio >= 0.15f && ratio <= 1.8f &&
        oxygen >= 65 && oxygen <= 100 &&
        (!spo2_valid_ || std::abs(oxygen - spo2_) <= 8)) {
      spo2_ = spo2_valid_ ? int(std::lround(0.75f * spo2_ + 0.25f * oxygen)) : oxygen;
      spo2_valid_ = true; spo2_fresh_ms_ = now_ms;
    }
  }

  uint32_t red_[kCapacity] = {}, ir_[kCapacity] = {}, times_[kCapacity] = {};
  uint32_t last_sample_ms_ = 0, hr_fresh_ms_ = 0, spo2_fresh_ms_ = 0;
  unsigned total_ = 0, count_ = 0, low_ir_count_ = 0;
  int hr_ = 0, spo2_ = 0, quality_ = 0;
  bool contact_ = false, hr_valid_ = false, spo2_valid_ = false;
};

}  // namespace carerover_watch
