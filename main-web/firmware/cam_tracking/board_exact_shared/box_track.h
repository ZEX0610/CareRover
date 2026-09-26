#pragma once
#include "vision_protocol.h"
#include "demo_tuning.h"
#include <algorithm>
namespace carerover {
// Constant-velocity Kalman tracking for display and association only.  The
// filtered box is never used as a motion source; wheel commands still require
// a fresh accepted detector measurement.
struct ScalarKalman {
  bool initialized = false;
  float x = 0, v = 0;
  float p00 = 25, p01 = 0, p11 = 100;

  void reset() { *this = ScalarKalman{}; }
  void init(float value) { initialized = true; x = value; v = 0; p00 = 25; p01 = 0; p11 = 100; }
  void predict(float dt, float processNoise) {
    if (!initialized) return;
    dt = std::clamp(dt, 0.001f, 1.0f);
    x += v * dt;
    const float oldP01 = p01;
    p00 += dt * (2.0f * oldP01 + dt * p11) + processNoise * dt * dt;
    p01 += dt * p11;
    p11 += processNoise;
  }
  void update(float measurement, float measurementNoise) {
    if (!initialized) { init(measurement); return; }
    const float innovation = measurement - x;
    const float s = std::max(0.001f, p00 + measurementNoise);
    const float k0 = p00 / s;
    const float k1 = p01 / s;
    x += k0 * innovation;
    v += k1 * innovation;
    const float oldP01 = p01;
    p00 = std::max(0.0001f, (1.0f - k0) * p00);
    p01 = (1.0f - k0) * oldP01;
    p11 = std::max(0.0001f, p11 - k1 * oldP01);
  }
};

class BoxTrack {
 public:
  void reset(){*this=BoxTrack{};}
  bool ready(uint64_t now)const{return have_&&now>=last_.receivedMs&&now-last_.receivedMs<=tuning::PersonDisplayMs;}
  float cost(const VisionPacket& p,uint64_t now)const{
    if(!p.found||p.x1<=p.x0||p.y1<=p.y0)return 1000;
    if(!ready(now))return 0;
    const auto predicted=view(now);const float area=float((p.x1-p.x0)*(p.y1-p.y0));
    const float oldArea=float((predicted.x1-predicted.x0)*(predicted.y1-predicted.y0));
    const float ratio=area/std::max(1.f,oldArea),iou=boxIou(predicted,p);
    const float dx=(float(p.x0+p.x1)-float(predicted.x0+predicted.x1))/640.f;
    const float dy=(float(p.y0+p.y1)-float(predicted.y0+predicted.y1))/480.f;
    const float distance=std::sqrt(dx*dx+dy*dy);
    if(iou<.02f&&(distance>.22f||ratio<.45f||ratio>2.2f))return 1000;
    return .55f*(1-iou)+.30f*distance+.15f*std::fabs(std::log(ratio));
  }
  bool update(const VisionPacket& p){
    if(!p.found||p.x1<=p.x0||p.y1<=p.y0)return false;
    if(have_&&(p.seq==last_.seq||p.receivedMs<=last_.receivedMs))return false;
    if(cost(p,p.receivedMs)>=1000)return false;
    const float x=(p.x0+p.x1)*.5f,y=(p.y0+p.y1)*.5f,a=std::log(float((p.x1-p.x0)*(p.y1-p.y0)));
    if(!ready(p.receivedMs)){cx_.init(x);cy_.init(y);area_.init(a);}
    else {
      const float dt=std::max(.01f,float(p.receivedMs-last_.receivedMs)/1000);
      cx_.predict(dt,180.f); cy_.predict(dt,120.f); area_.predict(dt,1.5f);
      const float noise=p.score>=450?4.f:16.f;
      cx_.update(x,noise); cy_.update(y,noise); area_.update(a,p.score>=450?.04f:.16f);
    }
    last_=p;have_=true;return true;
  }
  VisionPacket view(uint64_t now)const{
    VisionPacket p=last_;if(!ready(now)){p.found=false;return p;}
    const float dt=std::min(1.0f,float(now-last_.receivedMs)/1000);
    const float ratio=float(last_.x1-last_.x0)/float(last_.y1-last_.y0);
    auto xFilter=cx_, yFilter=cy_, aFilter=area_;
    xFilter.predict(dt,180.f); yFilter.predict(dt,120.f); aFilter.predict(dt,1.5f);
    const float a=std::exp(std::clamp(aFilter.x,0.f,11.25f));
    const float w=std::min(320.f,std::sqrt(a*ratio)),h=std::min(240.f,std::sqrt(a/ratio));
    const float x=std::clamp(xFilter.x,w/2,320-w/2),y=std::clamp(yFilter.x,h/2,240-h/2);
    p.x0=uint16_t(x-w/2);p.x1=uint16_t(x+w/2);p.y0=uint16_t(y-h/2);p.y1=uint16_t(y+h/2);return p;
  }
 private:
  VisionPacket last_;bool have_=false;ScalarKalman cx_,cy_,area_;
};
}
