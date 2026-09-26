#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../firmware/CareRoverWatch/watch_motion.h"
#include "../firmware/CareRoverWatch/watch_health.h"
#include "../firmware/CareRoverWatch/watch_protocol.h"

using namespace carerover_watch;

int main() {
  WristMotion m;
  uint32_t t = 1000;
  for (int i = 0; i < 45; ++i, t += 20) m.update(0, 0, 1, 0, 0, 0, t);
  assert(m.command(t).calibrated);
  for (int i = 0; i < 20; ++i, t += 20)
    m.update(0.342f, 0, 0.94f, 0, 0, 0, t);
  assert(m.command(t).vx > 0.1f);
  m.recenter();
  m.update(0.342f, 0, 0.94f, 0, 0, 0, t += 20);
  assert(std::fabs(m.command(t).vx) < 0.01f);
  for (int i = 0; i < 30; ++i, t += 20)
    m.update(0, 0, 1, 0, 0, 60, t);
  assert(m.command(t).vy < -0.1f);
  for (int i = 0; i < 10; ++i, t += 20)
    m.update(0, 0, 1, 90, 0, 0, t);
  assert(m.command(t).rolling && m.command(t).wz > 0 && m.command(t).vx == 0);
  for (int i = 0; i < 10; ++i, t += 20)
    m.update(0, 0, 1, -90, 0, 0, t);
  assert(m.command(t).rolling && m.command(t).wz < 0);
  for (int i = 0; i < 8; ++i, t += 20) m.update(0, 0, 1, 0, 0, 0, t);
  assert(!m.command(t).rolling);
  assert(!m.command(t + 200).calibrated);  // Stale IMU means zero target.

  WristHealth h;
  uint32_t ppgTime = 1000;
  for (int i = 0; i < 240; ++i, ppgTime += 40) {
    const float wave = std::sin(2 * 3.14159265f * 1.25f * i / 25.0f);
    h.sample(uint32_t(45000 + 700 * wave), uint32_t(50000 + 1000 * wave), ppgTime);
  }
  const HealthReading fresh = h.reading(ppgTime);
  std::printf("HR=%d SpO2=%d SQI=%d\n", fresh.hr_bpm,
              fresh.spo2_pct, fresh.signal_quality);
  assert(fresh.contact && fresh.hr_valid && fresh.spo2_valid);
  assert(fresh.hr_bpm >= 70 && fresh.hr_bpm <= 85);
  assert(fresh.spo2_pct >= 85 && fresh.spo2_pct <= 96);
  const HealthReading held = h.reading(ppgTime + 2000);
  assert(held.hr_valid && held.spo2_valid && held.hr_held && held.spo2_held);
  assert(!h.reading(ppgTime + 31000).hr_valid);
  for (int i = 0; i < 5; ++i) h.sample(0, 0, ppgTime += 40);
  assert(!h.reading(ppgTime).contact);

  char packet[320];
  assert(encodeWatchPacket(packet, sizeof(packet), 123, 4, t,
                           m.command(t), fresh));
  assert(std::strstr(packet, "\"type\":\"watch_v1\""));
  assert(std::strstr(packet, "\"spo2_valid\":1"));
  char tiny[10];
  assert(!encodeWatchPacket(tiny, sizeof(tiny), 123, 4, t,
                            m.command(t), fresh));
  std::puts("watch_native_test PASS");
}
