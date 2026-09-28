#pragma once

#include <cmath>
#include <cstdint>

namespace carerover {

// Diagnostic input only. Watch data must not be passed to SafetyController or
// ContinuousServoDrive until a separately reviewed WATCH mode exists.
struct WatchLinkPacket {
  uint32_t boot = 0, seq = 0, watchMs = 0;
  bool calibrated = false, rolling = false, contact = false;
  float vx = 0, vy = 0, wz = 0;
  int hr = 0, spo2 = 0, sqi = 0;
  bool hrValid = false, hrHeld = false, spo2Valid = false, spo2Held = false;
  uint32_t hrAgeMs = 0, spo2AgeMs = 0;
};

inline bool validWatchPacket(const WatchLinkPacket& p) {
  return std::isfinite(p.vx) && std::isfinite(p.vy) && std::isfinite(p.wz) &&
         std::fabs(p.vx) <= 0.45f && std::fabs(p.vy) <= 0.45f &&
         std::fabs(p.wz) <= 0.35f && p.hr >= 0 && p.hr <= 240 &&
         p.spo2 >= 0 && p.spo2 <= 100 && p.sqi >= 0 && p.sqi <= 100 &&
         (!p.hrValid || (p.contact && p.hr >= 35 && p.hr <= 220)) &&
         (!p.spo2Valid || (p.contact && p.spo2 >= 70 && p.spo2 <= 100)) &&
         (!p.hrHeld || p.hrValid) && (!p.spo2Held || p.spo2Valid);
}

struct WatchLinkSnapshot {
  WatchLinkPacket packet;
  uint32_t receivedMs = 0;
  bool seen = false;

  bool online(uint32_t nowMs) const {
    return seen && static_cast<uint32_t>(nowMs - receivedMs) < 350;
  }
};

class WatchLinkState {
 public:
  bool accept(const WatchLinkPacket& packet, uint32_t nowMs) {
    if (!validWatchPacket(packet)) return false;
    if (snapshot_.seen && packet.boot == snapshot_.packet.boot &&
        static_cast<int32_t>(packet.seq - snapshot_.packet.seq) <= 0) return false;
    snapshot_.packet = packet;
    snapshot_.receivedMs = nowMs;
    snapshot_.seen = true;
    return true;
  }

  WatchLinkSnapshot snapshot() const { return snapshot_; }

 private:
  WatchLinkSnapshot snapshot_;
};

}  // namespace carerover
