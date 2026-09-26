#pragma once
#include <cstdint>

namespace carerover {
enum class SeatNotice { None, Seated, Vacant };

// Only report stable transitions observed while the robot is stationary in IDLE.
// The first valid reading establishes a baseline; it is not a new arrival.
class SeatNotificationGate {
 public:
  static constexpr uint64_t DwellMs = 800;
  static constexpr uint64_t QuietMs = 5000;

  SeatNotice sample(bool idle, bool valid, bool seated, uint64_t now) {
    if (!idle) { armed_ = false; candidate_ = false; return SeatNotice::None; }
    if (!valid) { candidate_ = false; return SeatNotice::None; }
    if (!armed_) {
      armed_ = true; baseline_ = seated; candidate_ = false;
      return SeatNotice::None;
    }
    if (seated == baseline_) { candidate_ = false; return SeatNotice::None; }
    if (!candidate_ || candidateSeat_ != seated) {
      candidate_ = true; candidateSeat_ = seated; candidateSince_ = now;
      return SeatNotice::None;
    }
    if (now < candidateSince_ || now - candidateSince_ < DwellMs) return SeatNotice::None;
    baseline_ = seated; candidate_ = false;
    // State still advances during the quiet period, so a later reversal never
    // produces a delayed, misleading notification about the old state.
    if (sent_ && now - lastSent_ < QuietMs) return SeatNotice::None;
    sent_ = true; lastSent_ = now;
    return seated ? SeatNotice::Seated : SeatNotice::Vacant;
  }

 private:
  bool armed_ = false, baseline_ = false, candidate_ = false;
  bool candidateSeat_ = false, sent_ = false;
  uint64_t candidateSince_ = 0, lastSent_ = 0;
};
}
