#include "safety_controller.h"
#include "front_config.h"
#include "seat_notification_gate.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
using namespace carerover;
// Synthetic test fixtures, not physical calibration.
FrontConfig config() { FrontConfig c;c.enabled=c.verified=c.bypassVerified=true;c.stopCm=20;c.slowCm=50;c.warnCm=60;c.releaseCm=70;c.lateralSpeed=.15;c.forwardSpeed=.15;c.settleMs=140;c.marginMs=210;c.passMs=280;c.lateralTimeoutMs=1400;return c; }
VisionPacket face(uint64_t t) { VisionPacket p;p.kind='P';p.seq=t+1;p.receivedMs=t;p.found=true;p.score=900;p.x0=120;p.x1=200;p.y0=60;p.y1=160;return p; }
void zero(SafetyController& s,uint64_t t) { const auto v=s.snapshot(t).target;assert(!v.vx&&!v.vy&&!v.wz); }
void feed(SafetyController& s,uint64_t t,double cm=120) {s.imu(true,true,false,t);s.cameraPacket(t);s.heartbeat(1,t);s.person(face(t));s.frontSample(cm,true,t);s.computeFollow(t);}
int main() {
  auto cfg=config();assert(cfg.protectionReady()&&cfg.bypassReady());
  {auto c=cfg;c.stopCm=std::numeric_limits<double>::quiet_NaN();assert(!c.protectionReady());c=cfg;c.passMs=0;assert(!c.bypassReady());c=cfg;c.releaseCm=40;assert(!c.protectionReady());}
  assert(!frontPinAllowed(17)&&!frontPinAllowed(18)&&!frontPinAllowed(10)&&!frontPinAllowed(35));
  { FrontGuard g;g.configure(cfg);g.sample(120,true,1);g.sample(120,true,71);g.sample(5,true,141);
    assert(g.snapshot(141).distanceCm==120);assert(g.output({1,0,0},141).vx==0); // raw beats median
    g.sample(0,false,211);assert(!g.snapshot(211).valid);g.sample(40,true,281);assert(g.snapshot(281).distanceCm==40);
    assert(g.output({1,0,0},281).vx==0);assert(!g.fresh(501));g.sample(120,true,281);assert(!g.fresh(501));
  }
  { // Seating detection: <30cm seated, >30cm vacant, 3-read confirmation.
    SeatingDetector seat;
    seat.sample(25,true);seat.sample(25,true);assert(!seat.seated());
    seat.sample(25,true);assert(seat.seated());
    seat.sample(35,true);seat.sample(35,120);assert(seat.seated()); // second arg is bool-ish; 120 -> true
    seat.sample(35,true);assert(!seat.seated());
    seat.sample(0,false);seat.sample(25,true);seat.sample(25,true);seat.sample(25,120);assert(seat.seated());
  }
  { // Notification is baseline-aware, IDLE-only, dwell-filtered and rate-limited.
    SeatNotificationGate gate;
    assert(gate.sample(true,true,false,100)==SeatNotice::None);
    assert(gate.sample(true,true,true,200)==SeatNotice::None);
    assert(gate.sample(true,false,true,500)==SeatNotice::None); // bad echo breaks dwell
    assert(gate.sample(true,true,true,600)==SeatNotice::None);
    assert(gate.sample(true,true,true,1399)==SeatNotice::None);
    assert(gate.sample(true,true,true,1400)==SeatNotice::Seated);
    assert(gate.sample(true,true,false,1500)==SeatNotice::None);
    assert(gate.sample(true,true,false,2300)==SeatNotice::None); // quiet window
    assert(gate.sample(false,true,true,2500)==SeatNotice::None); // driving disarms
    assert(gate.sample(true,true,true,2600)==SeatNotice::None); // re-entry baseline
    assert(gate.sample(true,true,false,7000)==SeatNotice::None);
    assert(gate.sample(true,true,false,7800)==SeatNotice::Vacant);
  }
  { // MANUAL obstacle: automatic bypass enters Halt; mode stays Manual; target zeroed.
    SafetyController s;s.configureFront(cfg);s.network(true,1);s.cameraPacket(1);
    assert(!s.setMode(1,Mode::Manual,1));s.frontSample(120,true,2);assert(!s.setMode(1,Mode::Manual,2));
    assert(!s.velocity(1,.8,0,0,3));
    s.frontSample(10,true,72); // blocked -> auto-bypass Halt
    assert(s.snapshot(72).front.phase==BypassPhase::Halt);
    assert(s.snapshot(72).mode==Mode::Manual);zero(s,72);
  }
  { // MANUAL automatic bypass state machine: Halt -> Right -> Margin -> Pass -> None.
    FrontGuard g;g.configure(cfg);
    g.sample(18,true,1);g.advance(1,false,true,true);assert(g.phase()==BypassPhase::Halt);
    g.sample(18,true,71);g.sample(18,true,141);g.sample(18,true,211);
    g.advance(211,false,true,true);assert(g.phase()==BypassPhase::Right);assert(g.output({1,0,0},211).vy>0);
    g.sample(120,true,281);g.sample(120,true,351);g.sample(120,true,421);
    g.advance(421,false,true,120);assert(g.phase()==BypassPhase::Margin);
    g.sample(120,true,631);g.advance(631,false,true,120);assert(g.phase()==BypassPhase::Pass);
    assert(g.output({1,0,0},631).vx>0&&g.output({1,0,0},631).vy==0);
    g.sample(120,true,911);g.advance(911,false,true,120);assert(g.phase()==BypassPhase::None);
  }
  { // MANUAL integrated: obstacle enters Halt, mode stays Manual, target zeroed.
    SafetyController s;s.configureFront(cfg);s.network(true,1);s.cameraPacket(1);
    assert(!s.setMode(1,Mode::Manual,1));s.frontSample(120,true,2);assert(!s.velocity(1,.8,0,0,3));
    s.frontSample(18,true,4);
    assert(s.snapshot(4).front.phase==BypassPhase::Halt);
    assert(s.snapshot(4).mode==Mode::Manual);zero(s,4);
  }
  { SafetyController s;s.configureFront(cfg);s.network(true,1);feed(s,1);s.setMode(1,Mode::Manual,1);s.velocity(1,.5,0,0,1);
    s.cameraPacket(221);s.tick(221);assert(s.snapshot(221).mode==Mode::Manual); // front staleness no longer drops Manual
  }
  std::cout<<"Front protection: median, seating, release latch, manual auto-bypass passed\n";
}
