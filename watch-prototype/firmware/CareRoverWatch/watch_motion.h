#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace carerover_watch {

template <typename T> inline T limit(T v, T lo, T hi) {
  return std::max(lo, std::min(v, hi));
}

// Sensor mounting contract: +X toward fingertips, +Y toward the right-hand
// thumb, +Z away from the skin. Verify the printed MPU axes before soldering.
// Chassis contract: +vx forward, +vy right, +wz clockwise.
struct MotionCommand {
  float vx = 0;
  float vy = 0;
  float wz = 0;
  float pitch_deg = 0;
  float relative_yaw_deg = 0;
  bool calibrated = false;
  bool rolling = false;
};

class WristMotion {
 public:
  explicit WristMotion(int roll_sign = 1) : roll_sign_(roll_sign < 0 ? -1 : 1) {}

  void reset() { *this = WristMotion(roll_sign_); }

  // Call with acceleration in g and angular rate in degrees/s at about 50 Hz.
  void update(float ax, float ay, float az, float gx, float gy, float gz,
              uint32_t now_ms) {
    if (!std::isfinite(ax + ay + az + gx + gy + gz)) return;
    const float norm = std::sqrt(ax * ax + ay * ay + az * az);
    if (norm < 0.5f || norm > 1.5f) return;  // Dynamic acceleration: retain last pose.
    const float pitch = std::asin(limit(ax / norm, -1.0f, 1.0f)) * kRadToDeg;
    const uint32_t gap = last_ms_ ? now_ms - last_ms_ : 20;
    last_ms_ = now_ms;
    if (gap > 200) { command_ = {}; return; }

    if (!calibrated_) {
      const bool still = std::fabs(gx) < 6 && std::fabs(gy) < 6 &&
                         std::fabs(gz) < 6 && norm > 0.85f && norm < 1.15f;
      if (!still) { neutral_samples_ = 0; neutral_sum_ = 0; return; }
      neutral_sum_ += pitch;
      if (++neutral_samples_ >= 40) {
        neutral_pitch_ = neutral_sum_ / neutral_samples_;
        pitch_filtered_ = 0;
        yaw_deg_ = 0;
        calibrated_ = true;
      }
      command_.calibrated = calibrated_;
      return;
    }

    absolute_pitch_ = pitch;
    const float dt = limit(gap / 1000.0f, 0.0f, 0.08f);
    const bool still = std::fabs(gx) < 2 && std::fabs(gy) < 2 &&
                       std::fabs(gz) < 2 && norm > 0.9f && norm < 1.1f;
    if (still) {
      bias_x_ += 0.005f * (gx - bias_x_);
      bias_y_ += 0.005f * (gy - bias_y_);
      bias_z_ += 0.005f * (gz - bias_z_);
    }
    const float roll_rate = gx - bias_x_;
    const float yaw_rate = (roll_rate * ax + (gy - bias_y_) * ay +
                            (gz - bias_z_) * az) / norm;
    // A wrist roll can have a vertical component when the arm is raised.
    // Suppress that component from the horizontal-arm estimate.
    if (std::fabs(roll_rate) < 20) {
      yaw_deg_ = limit(yaw_deg_ + yaw_rate * dt, -65.0f, 65.0f);
    }
    pitch_filtered_ += 0.25f * ((pitch - neutral_pitch_) - pitch_filtered_);

    if (std::fabs(roll_rate) >= 35) {
      rolling_ = true;
      roll_direction_ = roll_rate > 0 ? 1 : -1;
      last_roll_ms_ = now_ms;
    } else if (rolling_ && std::fabs(roll_rate) >= 18) {
      last_roll_ms_ = now_ms;
    } else if (rolling_ && now_ms - last_roll_ms_ >= 100) {
      rolling_ = false;
    }

    MotionCommand next;
    next.calibrated = true;
    next.pitch_deg = pitch_filtered_;
    next.relative_yaw_deg = yaw_deg_;
    next.rolling = rolling_;
    if (rolling_) {
      next.wz = 0.35f * roll_sign_ * roll_direction_;
    } else {
      next.vx = signedAxis(pitch_filtered_, 7.0f, 35.0f, 0.45f);
      // Positive yaw is a leftward shoulder sweep; +vy means car right.
      next.vy = -signedAxis(yaw_deg_, 9.0f, 40.0f, 0.45f);
    }
    command_ = next;
  }

  void recenter() {
    if (!calibrated_) return;
    neutral_pitch_ = absolute_pitch_;
    pitch_filtered_ = yaw_deg_ = 0;
    rolling_ = false;
    command_ = {};
    command_.calibrated = true;
  }

  MotionCommand command(uint32_t now_ms) const {
    if (!calibrated_ || !last_ms_ || now_ms - last_ms_ > 180) return {};
    return command_;
  }

 private:
  static constexpr float kRadToDeg = 57.2957795f;
  static float signedAxis(float angle, float deadzone, float full, float max) {
    const float magnitude = std::fabs(angle);
    if (magnitude <= deadzone) return 0;
    return std::copysign(max * limit((magnitude - deadzone) /
                                           (full - deadzone), 0.0f, 1.0f), angle);
  }

  int roll_sign_ = 1;
  bool calibrated_ = false, rolling_ = false;
  uint32_t last_ms_ = 0, last_roll_ms_ = 0;
  unsigned neutral_samples_ = 0;
  float neutral_sum_ = 0, neutral_pitch_ = 0, absolute_pitch_ = 0;
  float pitch_filtered_ = 0, yaw_deg_ = 0;
  float bias_x_ = 0, bias_y_ = 0, bias_z_ = 0;
  int roll_direction_ = 1;
  MotionCommand command_;
};

}  // namespace carerover_watch
