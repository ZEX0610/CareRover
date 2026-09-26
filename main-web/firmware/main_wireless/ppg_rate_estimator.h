#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace carerover {

struct PulsePeriodEstimate {
  bool valid = false;
  int32_t bpm = 0;
  float correlation = 0.0f;
  float lagSamples = 0.0f;
};

// Estimate pulse period in the time domain. A slow pressure/baseline change can
// dominate the lowest Goertzel bin and look like a high-SNR 40 BPM pulse. A
// real pulse must instead create a local autocorrelation maximum inside the
// configured physiological interval.
inline PulsePeriodEstimate estimatePulsePeriod(const float* samples, size_t count,
                                               float sampleHz,
                                               float minBpm = 45.0f,
                                               float maxBpm = 150.0f,
                                               float minCorrelation = 0.30f) {
  PulsePeriodEstimate result;
  if (!samples || count < 16 || sampleHz <= 0.0f || minBpm <= 0.0f ||
      maxBpm <= minBpm) {
    return result;
  }

  int minLag = static_cast<int>(ceilf(sampleHz * 60.0f / maxBpm));
  int maxLag = static_cast<int>(floorf(sampleHz * 60.0f / minBpm));
  if (minLag < 2) minLag = 2;
  if (maxLag > static_cast<int>(count / 2)) maxLag = static_cast<int>(count / 2);
  constexpr size_t MAX_LAG_BINS = 64;
  const int lagBins = maxLag - minLag + 1;
  if (lagBins < 3 || lagBins > static_cast<int>(MAX_LAG_BINS)) return result;

  float correlations[MAX_LAG_BINS] = {};
  for (int offset = 0; offset < lagBins; ++offset) {
    const int lag = minLag + offset;
    double dot = 0.0;
    double firstEnergy = 0.0;
    double secondEnergy = 0.0;
    for (size_t i = 0; i + lag < count; ++i) {
      const double first = samples[i];
      const double second = samples[i + lag];
      dot += first * second;
      firstEnergy += first * first;
      secondEnergy += second * second;
    }
    const double denominator = sqrt(firstEnergy * secondEnergy);
    correlations[offset] = denominator > 0.0 ? static_cast<float>(dot / denominator) : -1.0f;
  }

  int bestOffset = -1;
  float bestCorrelation = -1.0f;
  // Endpoints are deliberately excluded: a trend whose period lies outside
  // the pulse range otherwise becomes a false peak at the 45/150 BPM boundary.
  for (int offset = 1; offset + 1 < lagBins; ++offset) {
    const float current = correlations[offset];
    if (current >= correlations[offset - 1] && current >= correlations[offset + 1] &&
        current > bestCorrelation) {
      bestCorrelation = current;
      bestOffset = offset;
    }
  }
  result.correlation = bestCorrelation;
  if (bestOffset < 0 || bestCorrelation < minCorrelation) return result;

  float lag = static_cast<float>(minLag + bestOffset);
  const float left = correlations[bestOffset - 1];
  const float middle = correlations[bestOffset];
  const float right = correlations[bestOffset + 1];
  const float denominator = left - 2.0f * middle + right;
  if (fabsf(denominator) > 1.0e-6f) {
    float offset = 0.5f * (left - right) / denominator;
    if (offset < -0.5f) offset = -0.5f;
    if (offset > 0.5f) offset = 0.5f;
    lag += offset;
  }

  const float bpm = sampleHz * 60.0f / lag;
  if (bpm < minBpm || bpm > maxBpm) return result;
  result.valid = true;
  result.bpm = static_cast<int32_t>(lroundf(bpm));
  result.lagSamples = lag;
  return result;
}

}  // namespace carerover
