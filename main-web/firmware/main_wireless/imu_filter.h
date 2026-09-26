#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "demo_tuning.h"
namespace carerover {
struct ImuSample {
  float yaw=0,pitch=0,roll=0,gx=0,gy=0,gz=0,ax=0,ay=0,az=0;
  float biasX=0,biasY=0,biasZ=0;
  bool valid=false,calibrated=false,tiltFault=false;
  uint64_t sampleMs=0;
  uint32_t acceptedFrames=0,rejectedFrames=0;
  bool held=false,warningTilt=false;
};
class ImuFilter {
 public:
  static constexpr float TiltFaultDeg=tuning::ImuSafetyTiltDeg;
  static constexpr float TiltRecoverDeg=tuning::ImuSafetyRecoverDeg;
  static constexpr uint64_t TiltConfirmMs=tuning::ImuSafetyTiltMs;
  static constexpr uint64_t TiltRecoveryMs=1000;
  void missing() {
    state_.valid=false;
    state_.held=true;
    // Missing samples are handled by SafetyController's bounded freshness
    // grace. They are not evidence of a physical tilt by themselves.
    state_.tiltFault=false;
    tiltStart_=0; recoveryStart_=0;
  }
  const ImuSample& state() const { return state_; }
  void update(float ax,float ay,float az,float gx,float gy,float gz,uint64_t now) {
    if(!std::isfinite(ax+ay+az+gx+gy+gz)) { missing(); return; }
    const float norm=std::sqrt(ax*ax+ay*ay+az*az);
    if(tuning::balanced) {
      // Chassis acceleration and wheel vibration are expected during a demo.
      // Reject only physically impossible sensor values; do not turn ordinary
      // dynamic acceleration into an IMU fault. Large chassis tilt is handled
      // separately below using the filtered angle and a sustained timer.
      const bool implausible=norm<.12f||norm>3.5f||std::fabs(gx)>1000||std::fabs(gy)>1000||std::fabs(gz)>1000;
      const bool transient=false;
      previousNorm_=norm;haveNorm_=true;
      if(implausible||transient) {
        ++state_.rejectedFrames;state_.held=true;tiltStart_=recoveryStart_=0;
        if(!state_.sampleMs||now<state_.sampleMs||now-state_.sampleMs>=tuning::ImuSafetyMs)missing();
        return;
      }
    }
    state_.held=false;++state_.acceptedFrames;
    float dt=last_&&now>=last_?float(now-last_)/1000:0.01f;
    if(dt>0.1f) { missing(); dt=0.01f; }
    last_=now;
    state_.ax=ax;state_.ay=ay;state_.az=az;state_.gx=gx;state_.gy=gy;state_.gz=gz;
    const float gravity=std::sqrt(ax*ax+ay*ay+az*az);
    // The board is mounted with its X axis vertical (pointing up), so gravity
    // reads along +X when level. Tilt is the deviation of gravity from +X:
    // pitch in the X-Y plane (around Z), roll in the X-Z plane (around Y).
    const float pitch=std::atan2(ay,ax)*57.2957795f;
    const float roll=std::atan2(az,ax)*57.2957795f;
    if(!state_.calibrated) {
      if(std::fabs(gravity-1)>0.08f||std::fabs(gx)>3||std::fabs(gy)>3||std::fabs(gz)>3) { calibrationStart_=0;count_=0;sx_=sy_=sz_=0; }
      else {
        if(!count_) calibrationStart_=now;
        ++count_; sx_+=gx;sy_+=gy;sz_+=gz;
        if(now-calibrationStart_>=5000&&count_>=450) { state_.biasX=sx_/count_;state_.biasY=sy_/count_;state_.biasZ=sz_/count_;state_.calibrated=true; }
      }
      state_.pitch=pitch;state_.roll=roll;
    } else {
      const float tau=tuning::balanced?.5f+4.f*std::fabs(gravity-1.f):.5f;
      const float alpha=tuning::balanced&&(gravity<.70f||gravity>1.30f)?1.f:tau/(tau+dt);
      state_.roll=alpha*(state_.roll+(gy-state_.biasY)*dt)+(1-alpha)*roll;
      state_.pitch=alpha*(state_.pitch+(gz-state_.biasZ)*dt)+(1-alpha)*pitch;
      state_.yaw+=(gx-state_.biasX)*dt;
      if(state_.yaw>180) state_.yaw-=360;
      if(state_.yaw<-180) state_.yaw+=360;
    }
    state_.valid=true;state_.sampleMs=now;
    // Require a sustained large tilt. A single raw accelerometer spike is
    // expected when the chassis starts, stops, or crosses a small obstacle.
    state_.warningTilt=std::fabs(pitch)>=35||std::fabs(roll)>=35;
    const bool overTilt=std::fabs(pitch)>=TiltFaultDeg||std::fabs(roll)>=TiltFaultDeg||
                        std::fabs(state_.pitch)>=TiltFaultDeg||std::fabs(state_.roll)>=TiltFaultDeg;
    if(overTilt) {
      recoveryStart_=0;
      if(!tiltStart_) tiltStart_=now;
      if(now-tiltStart_>=TiltConfirmMs) state_.tiltFault=true;
    } else {
      tiltStart_=0;
    }
    if(state_.tiltFault) {
      if(std::fabs(pitch)<TiltRecoverDeg&&std::fabs(roll)<TiltRecoverDeg&&
         std::fabs(state_.pitch)<TiltRecoverDeg&&std::fabs(state_.roll)<TiltRecoverDeg) {
        if(!recoveryStart_) recoveryStart_=now;
        if(now-recoveryStart_>=TiltRecoveryMs) state_.tiltFault=false;
      } else recoveryStart_=0;
    }
  }
 private:
  ImuSample state_; uint64_t last_=0,calibrationStart_=0,tiltStart_=0,recoveryStart_=0;
  float previousNorm_=1;bool haveNorm_=false;
  uint32_t count_=0;float sx_=0,sy_=0,sz_=0;
};
}
