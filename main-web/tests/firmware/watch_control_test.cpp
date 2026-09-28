#include "safety_controller.h"
#include "gesture_actions.h"
#include <cassert>
#include <iostream>
using namespace carerover;

int main() {
  GestureActionLatch five(4,3,true);
  for (uint64_t ms : {100u,200u,300u})
    assert(five.update(true,"five",ms)==GestureAction::None);
  assert(five.update(true,"five",400)==GestureAction::ToggleWatch);
  assert(five.update(true,"five",500)==GestureAction::None);
  for (uint64_t ms : {510u,520u,530u,540u})
    assert(five.update(false,"five",ms)==GestureAction::None);
  for (uint64_t ms : {550u,560u,570u,580u})
    assert(five.update(true,"five",ms)==GestureAction::None);
  for (uint64_t ms : {600u,700u,800u})
    assert(five.update(false,"no_hand",ms)==GestureAction::None);
  for (uint64_t ms : {900u,1000u,1100u})
    assert(five.update(true,"five",ms)==GestureAction::None);
  assert(five.update(true,"five",1200)==GestureAction::ToggleWatch);

  SafetyController s;
  s.configureCliff(CliffFrontRight|CliffRearLeft);
  s.cliffSample(0,1);s.cliffSample(0,31);
  assert(s.toggleWatch(100,false,10)!=nullptr);
  assert(s.snapshot(100).mode==Mode::Idle);
  assert(!s.toggleWatch(101,true,10));
  assert(s.snapshot(101).mode==Mode::Watch);
  s.watchInput(.3,0,0,true,true,10,100,102);
  assert(s.snapshot(102).target.vx==0 && !s.snapshot(102).watchArmed);
  s.watchInput(0,0,0,true,true,11,105,106);
  assert(s.snapshot(106).watchArmed);
  s.watchInput(.3,0,0,true,true,12,110,111);
  assert(s.snapshot(111).target.vx==.3);
  // The held pose drives continuously while fresh, but neutral stops without exiting.
  s.watchInput(.3,0,0,true,true,12,110,150);
  assert(s.snapshot(150).target.vx==.3);
  s.watchInput(0,0,0,true,true,13,151,152);
  assert(s.snapshot(152).target.vx==0 && s.snapshot(152).mode==Mode::Watch);
  s.watchInput(-.3,0,.2,true,true,14,160,161);
  s.cliffSample(CliffRearLeft,162);
  auto target=s.snapshot(162).target;
  assert(target.vx==0 && target.wz==0);
  s.cliffSample(0,163);s.cliffSample(0,193);
  assert(s.snapshot(193).target.vx==-.3);
  s.watchInput(.3,0,.2,true,true,15,195,196);
  s.cliffSample(CliffFrontRight,197);
  target=s.snapshot(197).target;
  assert(target.vx==0 && target.wz==0);
  assert(!s.toggleWatch(198,true,15));
  assert(s.snapshot(198).mode==Mode::Idle && s.snapshot(198).target.vx==0);

  assert(!s.toggleWatch(200,true,20));
  s.watchInput(0,0,0,true,true,21,201,202);
  s.watchInput(.3,0,0,true,true,22,203,204);
  s.watchInput(.3,0,0,true,true,22,203,553);
  assert(s.snapshot(553).mode==Mode::Idle);
  assert(s.snapshot(553).target.vx==0);
  assert(!s.toggleWatch(560,true,23));
  s.emergency(561);
  assert(s.snapshot(561).mode==Mode::Idle && s.snapshot(561).estop);
  assert(s.toggleWatch(562,true,24)!=nullptr);
  std::cout << "Watch: FIVE toggles, neutral/edge/stale/estop stops PASS\n";
}
