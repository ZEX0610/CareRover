#pragma once
#include "vision_protocol.h"
#include "box_track.h"
#include "demo_tuning.h"
#include <algorithm>
namespace carerover {
struct FollowVelocity { double vx=0, vy=0, wz=0; };
struct FollowConfig {
  float confidence=0.40f, ema=0.30f, centerDeadzone=0.04f, distanceDeadzone=0.05f;
  float distanceGain=0.40f, turnGain=0.50f;
  float maxVx=0.15f, maxWz=0.20f, matchIou=0.03f;
  // P results arrive at roughly 3.1-3.3 Hz in the combined stream.  Keep one
  // transient miss inside the 490 ms source deadline while commanding zero;
  // two consecutive misses still exceed this grace and latch target loss.
  uint64_t lossGraceMs=1100, lookaheadMs=300;
};
class PersonFollowController {
 public:
  explicit PersonFollowController(FollowConfig cfg={}) : cfg_(cfg) {}
  void reset() { measurementAccepted_=false;track_.reset();centerActive_=distanceActive_=false;lastOutputMs_=0; initialized_=false; have_=false; lost_=false; reference_=ex_=area_=0;lastValidMs_=0;output_={}; }
  void update(const VisionPacket& p) {
    if(have_&&p.seq==previous_.seq) return;
    measurementAccepted_=false;
    if(!p.found||p.score<float(1000)*cfg_.confidence) {
      output_={};
      if(initialized_&&lastValidMs_&&p.receivedMs>=lastValidMs_&&p.receivedMs-lastValidMs_>cfg_.lossGraceMs) lost_=true;
      return;
    }
    if(have_&&(tuning::balanced?track_.cost(p,p.receivedMs)>=1000:boxIou(previous_,p)<cfg_.matchIou)) {
      output_={};
      if(lastValidMs_&&p.receivedMs>=lastValidMs_&&p.receivedMs-lastValidMs_>cfg_.lossGraceMs) lost_=true;
      return;
    }
    if(tuning::balanced&&!track_.update(p)){output_={};return;}
    measurementAccepted_=true;previous_=p; have_=true;
    lastValidMs_=p.receivedMs; lost_=false;
    const auto measured=tuning::balanced?track_.view(p.receivedMs+cfg_.lookaheadMs):p;
    const float area=float((measured.x1-measured.x0)*(measured.y1-measured.y0));
    const float ex=(float(measured.x0+measured.x1)*0.5f-160.0f)/160.0f;
    if(!initialized_) {
      reference_=area; ex_=ex; area_=area; initialized_=true;
    } else { ex_+=cfg_.ema*(ex-ex_); area_+=cfg_.ema*(area-area_); }
    const float distanceError=1-std::sqrt(area_/reference_);
    const auto previousOutput=output_;
    output_={limit(cfg_.distanceGain*dead(distanceError,cfg_.distanceDeadzone),cfg_.maxVx),
             0,
             limit(cfg_.turnGain*dead(ex_,cfg_.centerDeadzone),cfg_.maxWz)};
    if(tuning::balanced){
      centerActive_=std::fabs(ex_)>(centerActive_?.025f:.04f);
      distanceActive_=std::fabs(distanceError)>(distanceActive_?.03f:.05f);
      // Decouple turn from translation: turn in place first, translate only
      // once centered, so a turn never cancels a wheel against forward motion.
      if(centerActive_) {
        output_.vx=0;
        output_.wz=limit(cfg_.turnGain*dead(ex_,.025f),cfg_.maxWz);
      } else {
        output_.wz=0;
        output_.vx=distanceActive_?limit(cfg_.distanceGain*dead(distanceError,.03f),cfg_.maxVx):0;
      }
      const float dt=lastOutputMs_?std::min(.5f,float(p.receivedMs-lastOutputMs_)/1000):.1f;
      if(output_.vx)output_.vx=std::clamp(output_.vx,previousOutput.vx-.15f*dt,previousOutput.vx+.15f*dt);
      if(output_.wz)output_.wz=std::clamp(output_.wz,previousOutput.wz-.20f*dt,previousOutput.wz+.20f*dt);
      lastOutputMs_=p.receivedMs;
    }
  }
  FollowVelocity output() const { return lost_?FollowVelocity{}:output_; }
  bool measurementAccepted() const { return measurementAccepted_; }
  bool ready() const { return initialized_&&!lost_; }
  bool lost() const { return lost_; }
  float referenceArea() const { return reference_; }
 private:
  static float dead(float x,float zone) { return std::fabs(x)<=zone?0:x-std::copysign(zone,x); }
  static float limit(float x,float max) { return std::max(-max,std::min(max,x)); }
  BoxTrack track_;bool centerActive_=false,distanceActive_=false;uint64_t lastOutputMs_=0;
  FollowConfig cfg_; VisionPacket previous_;
  bool measurementAccepted_=false,initialized_=false, have_=false, lost_=false;
  uint64_t lastValidMs_=0;
  float ex_=0, area_=0, reference_=0; FollowVelocity output_;
};
}
