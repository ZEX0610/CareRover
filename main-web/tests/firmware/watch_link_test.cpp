#include "../../firmware/main_wireless/watch_link_state.h"
#include <cassert>
#include <limits>

int main() {
  carerover::WatchLinkState state;
  carerover::WatchLinkPacket p;
  p.boot = 42;
  p.seq = 1;
  p.calibrated = true;
  p.vx = 0.2f;
  assert(state.accept(p, 1000));
  assert(state.snapshot().online(1200));
  assert(!state.snapshot().online(1350));

  // A duplicate or older packet must not extend the freshness window.
  assert(!state.accept(p, 1400));
  assert(state.snapshot().receivedMs == 1000);
  p.seq = 0;
  assert(!state.accept(p, 1500));
  p.seq = 2;
  assert(state.accept(p, 1500));
  assert(state.snapshot().online(1500));

  p.vy = std::numeric_limits<float>::quiet_NaN();
  p.seq = 3;
  assert(!state.accept(p, 1600));
  p.vy = 0;
  p.hrValid = true;
  p.hr = 0;
  assert(!state.accept(p, 1600));
  p.hrValid = false;
  p.hr = 0;
  p.boot = 43;
  p.seq = 0;
  assert(state.accept(p, 1600));
  assert(state.snapshot().packet.boot == 43);

  p.seq = 1;
  p.contact = true;
  p.hrValid = true;
  p.hr = 75;
  p.spo2Valid = true;
  p.spo2 = 91;
  p.sqi = 94;
  assert(state.accept(p, 1650));
  assert(state.snapshot().packet.hr == 75);
  assert(state.snapshot().packet.spo2 == 91);
  assert(state.snapshot().packet.sqi == 94);

  p.seq = 2;
  p.contact = false;
  p.hrValid = false;
  p.hr = 0;
  p.spo2Valid = false;
  p.spo2 = 0;
  p.sqi = 0;
  assert(state.accept(p, 1660));
  assert(!state.snapshot().packet.hrValid);
  assert(!state.snapshot().packet.spo2Valid);

  p.spo2Held = true;
  assert(!state.accept(p, 1700));
  p.spo2Held = false;
  p.vx = 1.0f;
  assert(!state.accept(p, 1700));
  return 0;
}
