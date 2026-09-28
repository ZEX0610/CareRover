#pragma once
#include <cstdint>

namespace carerover {
// Sensor OUT is HIGH when the downward-facing detector no longer sees the table.
enum CliffCorner : uint8_t {
  CliffFrontLeft = 1u << 0,
  CliffFrontRight = 1u << 1,
  CliffRearRight = 1u << 2,
  CliffRearLeft = 1u << 3,
};

struct CliffSnapshot {
  uint8_t edgeMask = 0x0f; // Before the first read, treat all directions as unsafe.
  uint8_t installedMask = 0x0f;
  bool sampled = false;
};

struct CliffVelocity { double vx, vy, wz; };

class CliffGuard {
 public:
  static constexpr uint64_t ClearDelayMs = 30;

  void configure(uint8_t installedMask) {
    state_.installedMask = installedMask & 0x0f;
    state_.edgeMask = state_.installedMask;
    state_.sampled = false;
    for (auto& since : lowSince_) since = 0;
  }

  // A HIGH blocks immediately. A return to LOW must remain stable before release.
  void sample(uint8_t highMask, uint64_t now) {
    highMask &= state_.installedMask;
    state_.sampled = true;
    for (unsigned i = 0; i < 4; ++i) {
      const uint8_t bit = uint8_t(1u << i);
      if (!(state_.installedMask & bit)) continue;
      if (highMask & bit) {
        state_.edgeMask |= bit;
        lowSince_[i] = 0;
      } else if (state_.edgeMask & bit) {
        if (!lowSince_[i]) lowSince_[i] = now ? now : 1;
        if (now >= lowSince_[i] && now - lowSince_[i] >= ClearDelayMs) {
          state_.edgeMask &= uint8_t(~bit);
          lowSince_[i] = 0;
        }
      }
    }
  }

  CliffSnapshot snapshot() const { return state_; }

  CliffVelocity filterManual(CliffVelocity v) const {
    const uint8_t edges = state_.edgeMask;
    if ((edges & CliffFrontLeft) && v.vy < 0) v.vy = 0;  // left
    if ((edges & CliffFrontRight) && v.vx > 0) v.vx = 0; // forward
    if ((edges & CliffRearRight) && v.vy > 0) v.vy = 0; // right
    if ((edges & CliffRearLeft) && v.vx < 0) v.vx = 0;  // backward
    if (edges) v.wz = 0; // either rotation at any exposed corner
    return v;
  }

 private:
  CliffSnapshot state_;
  uint64_t lowSince_[4] = {};
};
} // namespace carerover
