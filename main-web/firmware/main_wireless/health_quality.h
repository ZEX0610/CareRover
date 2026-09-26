#pragma once

#include <cstdint>

namespace carerover {

struct PpgWindowQuality {
  float sampleHz = 0.0f;
  float acDc = 0.0f;
  float correlation = 0.0f;
  float spectralSnr = 0.0f;
  float ratioR = 0.0f;
  int32_t heartRate = 0;
  int32_t spo2 = 0;
};

// Field policy: output measured candidates when sampling is sane and the
// waveform contains a modest periodic component. Stability across windows is
// enforced separately by StableMetric. These are demo/screening values, not a
// medical-device validation policy.
inline bool heartRateCandidateValid(const PpgWindowQuality& q) {
  return q.sampleHz >= 20.0f && q.sampleHz <= 30.0f &&
         q.heartRate >= 40 && q.heartRate <= 200 &&
         q.spectralSnr >= 2.0f && q.acDc >= 0.00002f && q.acDc <= 0.030f;
}

inline bool spo2CandidateValid(const PpgWindowQuality& q) {
  return q.sampleHz >= 20.0f && q.sampleHz <= 30.0f &&
         q.spectralSnr >= 2.0f && q.acDc >= 0.00002f && q.acDc <= 0.030f &&
         q.correlation >= 0.30f && q.ratioR >= 0.20f && q.ratioR <= 1.80f &&
         q.spo2 >= 65 && q.spo2 <= 100;
}

}  // namespace carerover
