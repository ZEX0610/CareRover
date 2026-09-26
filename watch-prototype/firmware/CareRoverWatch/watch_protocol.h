#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "watch_motion.h"
#include "watch_health.h"

namespace carerover_watch {

// One human-readable v1 datagram at 20 Hz. Values are desired chassis targets,
// not direct PWM. The car must ignore them outside its future WATCH mode.
inline bool encodeWatchPacket(char* dst, size_t capacity, uint32_t boot_id,
                              uint32_t seq, uint32_t now_ms,
                              const MotionCommand& motion,
                              const HealthReading& health) {
  if (!dst || capacity < 2 || !std::isfinite(motion.vx + motion.vy + motion.wz))
    return false;
  const int written = std::snprintf(
      dst, capacity,
      "{\"type\":\"watch_v1\",\"boot\":%lu,\"seq\":%lu,\"ms\":%lu,"
      "\"cal\":%d,\"roll\":%d,\"vx\":%.3f,\"vy\":%.3f,\"wz\":%.3f,"
      "\"contact\":%d,\"hr\":%d,\"hr_valid\":%d,\"hr_held\":%d,\"hr_age\":%lu,"
      "\"spo2\":%d,\"spo2_valid\":%d,\"spo2_held\":%d,\"spo2_age\":%lu,\"sqi\":%d}",
      static_cast<unsigned long>(boot_id), static_cast<unsigned long>(seq),
      static_cast<unsigned long>(now_ms), int(motion.calibrated), int(motion.rolling),
      double(motion.vx), double(motion.vy), double(motion.wz), int(health.contact),
      health.hr_bpm, int(health.hr_valid), int(health.hr_held),
      static_cast<unsigned long>(health.hr_age_ms), health.spo2_pct,
      int(health.spo2_valid), int(health.spo2_held),
      static_cast<unsigned long>(health.spo2_age_ms), health.signal_quality);
  return written > 0 && static_cast<size_t>(written) < capacity;
}

}  // namespace carerover_watch
