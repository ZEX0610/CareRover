#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace carerover {
// All physical thresholds/times must come from the installed chassis calibration.
struct FrontConfig {
  bool enabled=false, verified=false, bypassVerified=false;
  double stopCm=0, slowCm=0, warnCm=0, releaseCm=0;
  double lateralSpeed=0, forwardSpeed=0;
  uint32_t settleMs=0, marginMs=0, passMs=0, lateralTimeoutMs=0;
  bool protectionReady() const {
    return enabled&&verified&&std::isfinite(stopCm)&&std::isfinite(slowCm)&&
      std::isfinite(warnCm)&&std::isfinite(releaseCm)&&stopCm>=2&&
      stopCm<slowCm&&slowCm<=warnCm&&slowCm<releaseCm&&releaseCm<=400&&warnCm<=400;
  }
  bool bypassReady() const {
    return protectionReady()&&bypassVerified&&std::isfinite(lateralSpeed)&&std::isfinite(forwardSpeed)&&
      lateralSpeed>0&&lateralSpeed<=.25&&forwardSpeed>0&&forwardSpeed<=.25&&
      settleMs>=70&&settleMs<=5000&&marginMs>0&&passMs>0&&
      lateralTimeoutMs>marginMs&&lateralTimeoutMs<=30000&&passMs<=30000;
  }
};
// Bypass is now MANUAL-only and automatic: Halt -> Right -> Margin -> Pass -> done.
enum class BypassPhase { None, Halt, Right, Margin, Pass };
inline const char* phaseName(BypassPhase p) {
  switch(p) { case BypassPhase::Halt:return "HALT";case BypassPhase::Right:return "RIGHT";
    case BypassPhase::Margin:return "MARGIN";case BypassPhase::Pass:return "PASS";default:return "NONE"; }
}
struct FrontSnapshot {
  bool enabled=false, ready=false, valid=false, held=false;
  double distanceCm=0, rawCm=0; uint64_t ageMs=0;
  BypassPhase phase=BypassPhase::None;
  const char* status="DISABLED";
};
struct FrontVelocity { double vx=0,vy=0,wz=0; };

// Seating detection (IDLE mode only): distance < 30 cm counts as "seated",
// otherwise "vacant"; N consecutive readings confirm a transition. Reused from
// the previous desktop-pet HC-SR04 seating logic.
class SeatingDetector {
 public:
  static constexpr double ThresholdCm = 30.0;
  static constexpr uint8_t ConfirmCount = 3;
  void reset() { *this = SeatingDetector{}; }
  void sample(double cm, bool valid) {
    if (!valid || !std::isfinite(cm) || cm <= 0) { seatedCnt_ = vacantCnt_ = 0; return; }
    if (cm < ThresholdCm) {
      if (seatedCnt_ < UINT8_MAX) ++seatedCnt_;
      vacantCnt_ = 0;
      if (seatedCnt_ >= ConfirmCount) seated_ = true;
    } else {
      if (vacantCnt_ < UINT8_MAX) ++vacantCnt_;
      seatedCnt_ = 0;
      if (vacantCnt_ >= ConfirmCount) seated_ = false;
    }
  }
  bool seated() const { return seated_; }
 private:
  bool seated_ = false; uint8_t seatedCnt_ = 0, vacantCnt_ = 0;
};

class FrontGuard {
 public:
  static constexpr uint64_t StaleMs=220;
  void configure(FrontConfig cfg) { *this=FrontGuard{};cfg_=cfg; }
  const FrontConfig& config() const { return cfg_; }
  void sample(double cm,bool valid,uint64_t now) {
    if(seen_&&now<=sampleMs_) return; // Duplicate samples never renew freshness/counts.
    seen_=true;sampleMs_=now;valid_=valid&&std::isfinite(cm)&&cm>=2&&cm<=400;
    if(!valid_) { count_=near_=clear_=0;index_=0;return; }
    raw_=cm;values_[index_++%3]=cm;count_=std::min(count_+1,3);
    double sorted[3];std::copy(values_,values_+count_,sorted);std::sort(sorted,sorted+count_);
    distance_=sorted[count_/2];
    near_=cm<=cfg_.stopCm?std::min(near_+1,3):0;
    clear_=cm>=cfg_.releaseCm?std::min(clear_+1,3):0;
    if(cm<=cfg_.stopCm)blocked_=true;else if(clear_>=3)blocked_=false;
  }
  bool fresh(uint64_t now) const { return seen_&&valid_&&now>=sampleMs_&&now-sampleMs_<StaleMs; }
  FrontSnapshot snapshot(uint64_t now) const {
    FrontSnapshot s; s.enabled=cfg_.enabled;s.ready=cfg_.protectionReady();s.valid=fresh(now);
    s.held=held_;s.phase=phase_;
    s.distanceCm=s.valid?distance_:0;s.rawCm=s.valid?raw_:0;s.ageMs=seen_&&now>=sampleMs_?now-sampleMs_:StaleMs;
    s.status=!cfg_.enabled?"DISABLED":!s.ready?"UNCONFIGURED":!s.valid?"UNKNOWN":held_?"STOPPED":
      phase_!=BypassPhase::None?"BYPASS":blocked_?"BLOCKED":
      std::min(raw_,distance_)<cfg_.slowCm?"SLOW":distance_<cfg_.warnCm?"WARN":"CLEAR";
    return s;
  }
  bool active() const { return phase_!=BypassPhase::None; }
  BypassPhase phase() const { return phase_; }
  void cancel() { phase_=BypassPhase::None; }
  void release() { held_=false; }
  void hold() { held_=true; }
  bool held() const { return held_; }
  bool blocked() const { return blocked_; }
  // MANUAL-only automatic bypass. Returns nullptr (never stops the mode on its own):
  // a failed bypass simply gives up and lets blocked()/output() stop the wheels.
  const char* advance(uint64_t now,bool following,bool /*approaching*/,bool /*targetReady*/) {
    if(!cfg_.enabled||!cfg_.bypassReady())return nullptr;
    if(following)return nullptr;
    if(phase_==BypassPhase::None) {
      if(blocked_ && fresh(now) && raw_<=cfg_.stopCm) enter(BypassPhase::Halt,now);
      return nullptr;
    }
    if(phase_==BypassPhase::Halt) {
      if(now-phaseMs_>=3000) { phase_=BypassPhase::None; return nullptr; }
      if(near_>=3&&now-phaseMs_>=cfg_.settleMs) { enter(BypassPhase::Right,now);lateralMs_=now;clear_=0; }
    } else if(phase_==BypassPhase::Right||phase_==BypassPhase::Margin) {
      if(now-lateralMs_>=cfg_.lateralTimeoutMs) { phase_=BypassPhase::None; return nullptr; }
      if(phase_==BypassPhase::Right&&clear_>=3)enter(BypassPhase::Margin,now);
      else if(phase_==BypassPhase::Margin) {
        if(raw_<cfg_.releaseCm) { enter(BypassPhase::Right,now);clear_=0; }
        else if(now-phaseMs_>=cfg_.marginMs)enter(BypassPhase::Pass,now);
      }
    } else if(phase_==BypassPhase::Pass) {
      if(raw_<cfg_.slowCm) { phase_=BypassPhase::None; return nullptr; }
      if(now-phaseMs_>=cfg_.passMs) phase_=BypassPhase::None;
    }
    return nullptr;
  }
  FrontVelocity output(FrontVelocity desired,uint64_t now) const {
    if(!cfg_.enabled)return desired;
    if(held_)return {};
    if(phase_==BypassPhase::Halt)return {};
    if(phase_==BypassPhase::Right||phase_==BypassPhase::Margin)return {0,cfg_.lateralSpeed,0};
    if(phase_==BypassPhase::Pass)return {cfg_.forwardSpeed,0,0};
    if(desired.vx>0 && fresh(now)) {  // only gate forward motion when a real echo is present
      if(blocked_)return {};
      const auto factor=std::clamp((std::min(raw_,distance_)-cfg_.stopCm)/(cfg_.slowCm-cfg_.stopCm),0.0,1.0);
      desired.vx*=factor;
    }
    return desired;
  }
 private:
  void enter(BypassPhase phase,uint64_t now) { phase_=phase;phaseMs_=now; }
  FrontConfig cfg_;bool seen_=false,valid_=false,held_=false,blocked_=false;
  uint64_t sampleMs_=0,phaseMs_=0,lateralMs_=0;double raw_=0,distance_=0,values_[3]={};
  unsigned index_=0;int count_=0,near_=0,clear_=0;BypassPhase phase_=BypassPhase::None;
};
}
