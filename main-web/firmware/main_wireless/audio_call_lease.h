#pragma once
#include <atomic>
#include <cstdint>

namespace carerover {
// Relay refreshes this once per second. A lost relay must not leave the sonar off.
class AudioCallLease {
 public:
  static constexpr uint32_t TimeoutMs = 3000;
  void set(bool active, uint32_t nowMs) {
    lastMs_.store(nowMs);
    active_.store(active);
  }
  void reset() { active_.store(false); }
  bool active(uint32_t nowMs) const {
    return active_.load() && uint32_t(nowMs - lastMs_.load()) < TimeoutMs;
  }
 private:
  std::atomic<uint32_t> lastMs_{0};
  std::atomic<bool> active_{false};
};
}
