#include "safety_controller.h"
#include <cassert>
#include <iostream>
using namespace carerover;

int main() {
  CliffGuard guard;
  auto v = guard.filterManual({1,-1,1});
  assert(v.vx==0 && v.vy==0 && v.wz==0); // No sample yet.
  guard.sample(0,100);
  guard.sample(0,130);
  assert(guard.snapshot().edgeMask==0 && guard.snapshot().sampled);
  v=guard.filterManual({1,-1,1});
  assert(v.vx==1 && v.vy==-1 && v.wz==1);

  guard.sample(CliffFrontLeft,140);
  v=guard.filterManual({1,-1,1});
  assert(v.vx==1 && v.vy==0 && v.wz==0);
  v=guard.filterManual({-1,1,-1});
  assert(v.vx==-1 && v.vy==1 && v.wz==0);
  guard.sample(0,145);
  guard.sample(0,174);
  assert(guard.snapshot().edgeMask==CliffFrontLeft);
  guard.sample(0,175);
  assert(guard.snapshot().edgeMask==0);

  guard.sample(CliffFrontRight,180);
  v=guard.filterManual({1,-1,-1});
  assert(v.vx==0 && v.vy==-1 && v.wz==0);
  v=guard.filterManual({-1,1,0});
  assert(v.vx==-1 && v.vy==1 && v.wz==0);
  guard.sample(0,181); guard.sample(0,211);
  guard.sample(CliffRearRight,212);
  v=guard.filterManual({1,1,1});
  assert(v.vx==1 && v.vy==0 && v.wz==0);
  guard.sample(0,213); guard.sample(0,243);
  guard.sample(CliffRearLeft,244);
  v=guard.filterManual({-1,-1,-1});
  assert(v.vx==0 && v.vy==-1 && v.wz==0);
  guard.sample(CliffFrontLeft|CliffFrontRight|CliffRearRight|CliffRearLeft,245);
  v=guard.filterManual({1,1,-1});
  assert(v.vx==0 && v.vy==0 && v.wz==0);

  // During staged installation, unconnected corners must not read as edges.
  CliffGuard frontRightOnly;
  frontRightOnly.configure(CliffFrontRight);
  assert(frontRightOnly.snapshot().installedMask==CliffFrontRight);
  frontRightOnly.sample(0,10); frontRightOnly.sample(0,40);
  assert(frontRightOnly.snapshot().edgeMask==0);
  frontRightOnly.sample(CliffFrontLeft|CliffFrontRight|CliffRearRight|CliffRearLeft,41);
  assert(frontRightOnly.snapshot().edgeMask==CliffFrontRight);
  v=frontRightOnly.filterManual({1,-1,1});
  assert(v.vx==0 && v.vy==-1 && v.wz==0);

  // Existing manual target is filtered at snapshot/output time, not just on receipt.
  SafetyController safety;
  safety.cliffSample(0,1); safety.cliffSample(0,31);
  assert(!safety.setMode(1,Mode::Manual,40));
  assert(!safety.velocity(1,0,-.5,.3,40));
  auto target=safety.snapshot(40).target;
  assert(target.vy==-.5 && target.wz==.3);
  safety.cliffSample(CliffFrontLeft,41);
  target=safety.snapshot(41).target;
  assert(target.vx==0 && target.vy==0 && target.wz==0);
  assert(safety.snapshot(41).mode==Mode::Manual);
  safety.cliffSample(0,42); safety.cliffSample(0,72);
  target=safety.snapshot(72).target;
  assert(target.vy==-.5 && target.wz==.3);
  std::cout << "Cliff guard: corner directions, rotation, immediate block, delayed clear PASS\n";
}
