#pragma once
#include "demo_tuning.h"
#include "signal_state_filters.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace carerover {
// A bounded 2-of-3 display vote. The action latch receives its own raw evidence.
class GestureDisplay {
 public:
  GestureDisplay(uint16_t enter,uint16_t hold,uint8_t ef,uint8_t sf,uint8_t grace,uint8_t nohand)
    :baseline_(enter,hold,ef,sf,grace,nohand),enter_(enter),hold_(hold) {}
  void reset(){baseline_.reset();*thisState()=State{};}
  void update(bool hand,const char* label,uint16_t score,uint64_t now=0) {
    if(!tuning::balanced){baseline_.update(hand,label,score);if(baseline_.accepted()&&!baseline_.holding())s_.last=now;return;}
    const bool category=hand&&label&&std::strcmp(label,"no_hand")&&std::strcmp(label,"no_gesture");
    auto& slot=s_.votes[s_.next++%3];slot={};
    if(category&&score>=enter_){copy(slot.label,label);slot.ms=now;}
    const bool support=category&&s_.active[0]&&!std::strcmp(label,s_.active)&&score>=hold_;
    if(support){s_.last=now;s_.score=score;s_.held=false;}
    else s_.held=s_.active[0];
    if(hand)s_.noHand=false;else if(!s_.noHand){s_.noHand=true;s_.noHandSince=now;}
    unsigned votes=0;
    if(category&&score>=enter_) for(const auto& v:s_.votes)
      if(!std::strcmp(v.label,label)&&now>=v.ms&&now-v.ms<=1500)++votes;
    if(votes>=2){copy(s_.active,label);s_.last=now;s_.score=score;s_.held=false;}
    if(s_.active[0]&&(now-s_.last>=tuning::GestureDisplayHoldMs ||
       (s_.noHand&&now-s_.noHandSince>=tuning::GestureDisplayNoHandMs))){s_.active[0]=0;s_.held=false;}
  }
  bool accepted()const{return tuning::balanced?s_.active[0]!=0:baseline_.accepted();}
  bool holding()const{return tuning::balanced?s_.held:baseline_.holding();}
  const char* label()const{return tuning::balanced?(accepted()?s_.active:"no_gesture"):baseline_.label();}
  uint64_t age(uint64_t now)const{return accepted()&&now>=s_.last?now-s_.last:0;}
  uint16_t scoreMilli()const{return tuning::balanced?s_.score:baseline_.scoreMilli();}
 private:
  static void copy(char* out,const char* in){std::strncpy(out,in,19);out[19]=0;}
  struct Vote{char label[20]={};uint64_t ms=0;};
  struct State{Vote votes[3];unsigned next=0;char active[20]={};uint64_t last=0,noHandSince=0;uint16_t score=0;bool held=false,noHand=false;}s_;
  State* thisState(){return &s_;}
  GestureHysteresis baseline_;uint16_t enter_,hold_;
};
// Independent metric state with a real-time hold deadline and 2-of-3 acquisition.
class TimedMetric {
 public:
  TimedMetric(uint64_t holdMs,int delta,float rate):holdMs_(holdMs),delta_(delta),rate_(rate){}
  void reset(){valid_=held_=false;count_=next_=0;last_=updated_=0;value_=0;for(auto& c:candidates_)c={};}
  void update(bool plausible,int candidate,float quality,uint64_t now){
    auto& c=candidates_[next_++%3];c={plausible,candidate,now};if(count_<3)++count_;
    unsigned votes=0;
    if(plausible)for(unsigned i=0;i<count_;++i)if(candidates_[i].valid&&now>=candidates_[i].ms&&now-candidates_[i].ms<=3000&&std::abs(candidates_[i].value-candidate)<=delta_)++votes;
    if(votes>=2){
      if(!valid_)value_=candidate;
      else{const float dt=std::min(3.f,float(now-updated_)/1000);const float alpha=.25f+.5f*std::clamp(quality,0.f,1.f);value_+=std::clamp(alpha*(candidate-value_),-rate_*dt,rate_*dt);}
      valid_=true;held_=false;last_=updated_=now;
    }else if(valid_){held_=true;if(now-last_>=holdMs_){valid_=held_=false;value_=0;}}
  }
  bool valid()const{return valid_;} bool held()const{return held_;}
  int32_t output()const{return int32_t(std::lround(value_));}
  uint64_t age(uint64_t now)const{return valid_&&now>=last_?now-last_:0;}
 private:
  struct Candidate{bool valid=false;int value=0;uint64_t ms=0;}candidates_[3];
  unsigned count_=0,next_=0;uint64_t holdMs_,last_=0,updated_=0;int delta_;float rate_,value_=0;bool valid_=false,held_=false;
};
}
