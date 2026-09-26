#pragma once
#include <cmath>
#include <cstdint>
#include "person_follow.h"
#include "demo_tuning.h"
#include "front_guard.h"
namespace carerover {
enum class Mode { Idle, Manual, Health, Follow, Gesture };
inline const char* modeName(Mode m) { return m==Mode::Manual?"MANUAL":m==Mode::Health?"HEALTH_CHECK":m==Mode::Follow?"PERSON_FOLLOW":m==Mode::Gesture?"GESTURE_CONTROL":"IDLE"; }
struct Targets { double vx=0, vy=0, wz=0; };
struct SafetySnapshot {
  Mode mode=Mode::Idle; Targets target; FrontSnapshot front;
  bool estop=false, fault=false, camera=false, network=false, seated=false;
  uint32_t owner=0, stopSequence=0;
  uint64_t lastCommandMs=0, stoppedAtMs=0;
  const char* stopReason="boot";
};
class SafetyController {
 public:
  static constexpr uint64_t CommandExpiryMs=tuning::CommandSafetyMs, CameraExpiryMs=tuning::CameraSafetyMs, PersonExpiryMs=tuning::PersonSafetyMs;
  static constexpr uint16_t PersonAcceptScoreMilli=tuning::balanced?400:450;
  static constexpr uint64_t GestureTurnTimeoutMs=12000;
  SafetySnapshot snapshot(uint64_t now) const {
    auto s=state_; s.camera=cameraSeen_&&now>=lastCameraMs_&&now-lastCameraMs_<CameraExpiryMs;
    s.fault=state_.fault;
    s.front=front_.snapshot(now); s.seated=seat_.seated();
    if(state_.mode==Mode::Manual||state_.mode==Mode::Follow||state_.mode==Mode::Gesture) {
      auto v=front_.output({s.target.vx,s.target.vy,s.target.wz},now);s.target={v.vx,v.vy,v.wz};
    }
    return s;
  }
  void configureFront(FrontConfig cfg) { front_.configure(cfg); seat_.reset(); }
  void frontSample(double cm,bool valid,uint64_t now) { front_.sample(cm,valid,now);seat_.sample(cm,valid);tick(now); }
  bool seated() const { return seat_.seated(); }
  void configureHardware(bool enabled, bool calibrated) { hardware_=enabled; calibration_=calibrated; }
  void imu(bool valid, bool /*calibrated*/, bool tilt, uint64_t now, uint64_t sourceMs=0) {
    // IMU validity/calibration no longer gate motion, but a sustained large
    // chassis tilt still stops the controller immediately (tilt protection).
    if(valid && !tilt) {
      imuValid_=true;
      lastImuMs_=sourceMs?sourceMs:now;
      invalidSince_=0;
      return;
    }
    if(tilt) {
      imuValid_=false; invalidSince_=now;
      if(state_.mode==Mode::Manual||state_.mode==Mode::Follow||state_.mode==Mode::Gesture) stop(now,"tilt_fault");
      return;
    }
    if(!invalidSince_) invalidSince_=now;
    if(now-invalidSince_>=tuning::ImuSafetyMs) imuValid_=false;
  }
  void cameraPacket(uint64_t now) { cameraSeen_=true; lastCameraMs_=now; }
  void cameraReset(uint64_t /*now*/) { personSeen_=false; }
  void person(const VisionPacket& p) {
    person_=p; personSeen_=true;
    if(state_.mode==Mode::Follow) {
      follow_.update(p);
      if(!follow_.measurementAccepted()) state_.target={};
      if(follow_.lost()) {
        if(!lostSinceMs_) lostSinceMs_=p.receivedMs;
        if(p.receivedMs>=lostSinceMs_ && p.receivedMs-lostSinceMs_>=tuning::FollowLostTimeoutMs) { stop(p.receivedMs,"target_lost_timeout"); }
      } else {
        lostSinceMs_=0;
      }
      if(front_.active() && !follow_.measurementAccepted()) { stop(p.receivedMs,"target_lost"); }
    }
  }
  void heartbeat(uint32_t client,uint64_t now) {
    if(state_.mode==Mode::Follow&&client==state_.owner) heartbeatMs_=now;
  }
  // Only the controller task may refresh this output lease; ping never does.
  void computeFollow(uint64_t now) {
    tick(now); if(state_.mode!=Mode::Follow) return;
    auto v=follow_.output(); state_.target={v.vx,v.vy,v.wz}; state_.lastCommandMs=now; armed_=true;
  }
  float followReference() const { return follow_.referenceArea(); }
  bool followingReady() const { return follow_.ready(); }
  const char* autonomousFollow(uint64_t now) {
    tick(now);
    if(state_.estop) return "ESTOP_ACTIVE";
    if(state_.fault) return "FAULT_ACTIVE";
    if(state_.owner) return "CONTROL_BUSY";
    if(front_.held()) return "FRONT_RELEASE_REQUIRED";
    if(!personFresh(now)||!person_.found||person_.score<PersonAcceptScoreMilli) return "TARGET_NOT_READY";
    state_.owner=0; stop(now,"gesture_like"); state_.mode=Mode::Follow;
    state_.lastCommandMs=now; armed_=true;
    return nullptr;
  }
  const char* autonomousTurn(bool clockwise,float yaw,uint64_t now) {
    tick(now);
    if(!std::isfinite(yaw))return "IMU_NOT_READY";
    if(state_.estop) return "ESTOP_ACTIVE";
    if(state_.fault) return "FAULT_ACTIVE";
    if(state_.owner) return "CONTROL_BUSY";
    if(front_.held()) return "FRONT_RELEASE_REQUIRED";
    state_.owner=0; stop(now,clockwise?"gesture_two":"gesture_ok"); state_.mode=Mode::Gesture;
    turnActive_=true; turnClockwise_=clockwise; turnAccumDeg_=0; turnLastYaw_=yaw; turnStartMs_=now;
    state_.target={0,0,clockwise?0.28:-0.28}; state_.lastCommandMs=now; armed_=true;
    return nullptr;
  }
  void updateGestureTurn(float yaw,uint64_t now) {
    tick(now);
    if(state_.mode!=Mode::Gesture || !turnActive_) return;
    if(!std::isfinite(yaw)) { stop(now,"imu_invalid");return; }
    float delta=yaw-turnLastYaw_;
    if(delta>180) delta-=360;
    if(delta<-180) delta+=360;
    turnLastYaw_=yaw; turnAccumDeg_+=delta;
    if(std::fabs(turnAccumDeg_)>=360.0f) { state_.owner=0; stop(now,"gesture_turn_complete"); return; }
    if(now-turnStartMs_>=GestureTurnTimeoutMs) { state_.owner=0; stop(now,"gesture_turn_timeout"); return; }
    const double speed=std::fabs(turnAccumDeg_)>=300.0f?0.18:0.28;
    state_.target={0,0,turnClockwise_?speed:-speed}; state_.lastCommandMs=now; armed_=true;
  }
  void autonomousStop(uint64_t now,const char* reason="gesture_dislike") {
    state_.owner=0; stop(now,reason);
  }
  float gestureTurnDegrees() const { return std::fabs(turnAccumDeg_); }
  void network(bool online,uint64_t /*now*/) { state_.network=online; }
  void fault(bool active,uint64_t now) { state_.fault=active; if(active) stop(now,"fault"); }
  void disconnect(uint32_t client,uint64_t /*now*/) { if(client&&state_.owner==client) { state_.owner=0; } }
  void emergency(uint64_t now) { state_.estop=true; stop(now,"estop"); }
  const char* clear(uint32_t client,uint64_t now) {
    if(!client) return "INVALID_COMMAND";
    if(state_.fault) return "FAULT_ACTIVE";
    if(busy(client)) return "CONTROL_BUSY";
    state_.owner=client; state_.estop=false; stop(now,"estop_cleared"); return nullptr;
  }
  const char* setMode(uint32_t client,Mode mode,uint64_t now) {
    tick(now);
    if(!client) return "INVALID_COMMAND";
    if(state_.estop) return "ESTOP_ACTIVE";
    if(state_.fault) return "FAULT_ACTIVE";
    if(busy(client)) return "CONTROL_BUSY";
    const bool movement=mode==Mode::Manual||mode==Mode::Follow||mode==Mode::Gesture;
    if(movement&&front_.held())return "FRONT_RELEASE_REQUIRED";
    if(mode==Mode::Follow&&(!personFresh(now)||!person_.found||person_.score<PersonAcceptScoreMilli)) return "TARGET_NOT_READY";
    state_.owner=client; stop(now,"mode_changed"); state_.mode=mode;
    if(mode==Mode::Follow) { heartbeatMs_=now; state_.lastCommandMs=now; armed_=true; }
    return nullptr;
  }
  const char* velocity(uint32_t client,double vx,double vy,double wz,uint64_t now) {
    tick(now);
    if(!valid(vx)||!valid(vy)||!valid(wz)||!client) return "INVALID_COMMAND";
    if(busy(client)) return "CONTROL_BUSY";
    if(vx==0&&vy==0&&wz==0) {
      front_.release();
      const bool wasActive=armed_||state_.target.vx||state_.target.vy||state_.target.wz;
      state_.target={}; armed_=false;
      if(state_.mode==Mode::Follow) stop(now,"release");
      else if(wasActive) { state_.stoppedAtMs=now; state_.stopReason="release"; ++state_.stopSequence; }
      return nullptr;
    }
    if(state_.estop) return "ESTOP_ACTIVE";
    if(state_.fault) return "FAULT_ACTIVE";
    if(state_.mode!=Mode::Manual||state_.owner!=client) return "NOT_IN_MANUAL";
    if(front_.held()) return "FRONT_RELEASE_REQUIRED";
    state_.target={vx,vy,wz}; state_.lastCommandMs=now; armed_=true; tick(now);return nullptr;
  }
  void tick(uint64_t now) {
    const bool movingMode=state_.mode==Mode::Manual||state_.mode==Mode::Follow||state_.mode==Mode::Gesture;
    if(movingMode) {
      const auto before=front_.phase();
      auto reason=front_.advance(now,state_.mode==Mode::Follow,state_.target.vx>0,follow_.ready());
      if(before==BypassPhase::None&&front_.phase()==BypassPhase::Halt) {
        state_.stopReason="front_obstacle";state_.stoppedAtMs=now;++state_.stopSequence;
      }
      if(reason) { stop(now,reason);return; }
      if(state_.mode==Mode::Manual&&state_.target.vx>0&&front_.config().enabled&&front_.blocked()&&front_.phase()==BypassPhase::None) {
        state_.target={};armed_=false;front_.hold();state_.stopReason="front_obstacle";state_.stoppedAtMs=now;++state_.stopSequence;
      }
    }
  }
  void stop(uint64_t now,const char* reason) {
    state_.target={}; state_.mode=Mode::Idle; armed_=false; turnActive_=false; follow_.reset();front_.cancel();lostSinceMs_=0;
    state_.stoppedAtMs=now; state_.stopReason=reason; ++state_.stopSequence;
  }
 private:
  static bool valid(double v) { return std::isfinite(v)&&v>=-1&&v<=1; }
  bool busy(uint32_t client) const { return state_.owner&&state_.owner!=client; }
  bool hardwareHealthy(uint64_t now) const { return imuValid_&&now>=lastImuMs_&&now-lastImuMs_<tuning::ImuSafetyMs; }
  bool personFresh(uint64_t now) const { return personSeen_&&now>=person_.receivedMs&&now-person_.receivedMs<PersonExpiryMs; }
  SafetySnapshot state_; FrontGuard front_; SeatingDetector seat_; PersonFollowController follow_; VisionPacket person_;
  uint64_t lastCameraMs_=0,lastImuMs_=0,heartbeatMs_=0,invalidSince_=0,lostSinceMs_=0;
  bool turnActive_=false,turnClockwise_=true;
  float turnAccumDeg_=0,turnLastYaw_=0;
  uint64_t turnStartMs_=0;
  bool cameraSeen_=false,armed_=false,hardware_=false,calibration_=false,imuValid_=false,personSeen_=false;
};
}
