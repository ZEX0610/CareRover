#include "continuous_servo_drive.h"
#include <cassert>
#include <limits>
#include <iostream>
uint32_t fakeNow=10;int attachCount=0,failAttach=0;uint32_t duties[64]={};
using namespace carerover;
int main(){
  const uint8_t pins[4]={10,11,12,13};ServoCalibration c={{1490,1500,1510,1520},{1,-1,1,-1},300,{100,200,300,400}};
  ContinuousServoDrive d;assert(d.begin(pins,c));uint32_t neutral[4];for(int i=0;i<4;++i)neutral[i]=duties[pins[i]];
  assert(d.commandChassis(.2f,0,0,240));fakeNow+=100;d.tick();
  assert(duties[pins[FRONT_LEFT]]>neutral[FRONT_LEFT]);
  assert(duties[pins[FRONT_RIGHT]]==neutral[FRONT_RIGHT]);
  assert(duties[pins[REAR_LEFT]]==neutral[REAR_LEFT]);
  assert(duties[pins[REAR_RIGHT]]<neutral[REAR_RIGHT]);
  WheelSpeeds invalid={{-.8f,0,std::numeric_limits<float>::quiet_NaN(),0}};
  auto before=d.target();assert(!d.commandWheels(invalid,100));for(int i=0;i<4;++i)assert(before.value[i]==d.target().value[i]);
  d.stopNow();for(int i=0;i<4;++i)assert(duties[pins[i]]==neutral[i]);
  d.commandChassis(1,0,0,100);fakeNow+=100;d.tick();assert(!d.moving());
  attachCount=0;failAttach=3;ContinuousServoDrive broken;assert(!broken.begin(pins,c));assert(!broken.ready());
  // Physical CareRover layout is two orthogonal active wheel pairs, not an
  // X-drive. Longitudinal motion must leave the lateral pair passive; lateral
  // motion must leave the longitudinal pair passive.
  auto forward=mixOrthogonalOmni(.4f,0,0);
  assert(forward.value[FRONT_LEFT]==.4f&&forward.value[FRONT_RIGHT]==0);
  assert(forward.value[REAR_LEFT]==0&&forward.value[REAR_RIGHT]==.4f);
  auto right=mixOrthogonalOmni(0,.4f,0);
  assert(right.value[FRONT_LEFT]==0&&right.value[FRONT_RIGHT]==.4f);
  assert(right.value[REAR_LEFT]==.4f&&right.value[REAR_RIGHT]==0);
  auto clockwise=mixOrthogonalOmni(0,0,.4f);
  // Field observation: the previous sign produced counter-clockwise motion.
  // For the public +wz=clockwise contract, the front pair must be positive
  // and the rear pair negative with the calibrated servo polarity applied.
  assert(clockwise.value[FRONT_LEFT]==.4f&&clockwise.value[FRONT_RIGHT]==.4f);
  assert(clockwise.value[REAR_LEFT]==-.4f&&clockwise.value[REAR_RIGHT]==-.4f);
  auto diagonal=mixOrthogonalOmni(.4f,.3f,0);
  assert(diagonal.value[FRONT_LEFT]==.4f&&diagonal.value[FRONT_RIGHT]==.3f);
  assert(diagonal.value[REAR_LEFT]==.3f&&diagonal.value[REAR_RIGHT]==.4f);
  auto saturated=mixOrthogonalOmni(1,1,1);
  assert(saturated.value[FRONT_LEFT]==1&&saturated.value[FRONT_RIGHT]==1);
  assert(saturated.value[REAR_LEFT]==0&&saturated.value[REAR_RIGHT]==0);
  std::cout<<"Orthogonal omni mixing, PWM calibration, rejection, deadline and initialization failure passed\n";
}
