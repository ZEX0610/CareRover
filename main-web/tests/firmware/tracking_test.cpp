#include "vision_protocol.h"
#include "person_follow.h"
#include "safety_controller.h"
#include "imu_filter.h"
#include "latest_frame.h"
#include "video_work_queue.h"
#include <cassert>
#include <string>
#include <iostream>
using namespace carerover;
VisionPacket face(uint32_t seq,uint64_t now,int x=100,int width=80) {
  VisionPacket p;p.seq=seq;p.camMs=uint32_t(now);p.receivedMs=now;p.found=true;p.score=900;p.x0=x;p.x1=x+width;p.y0=60;p.y1=140;return p;
}
std::string encoded(VisionPacket p) {char b[256];assert(encodeVision(p,b,sizeof(b)));return b;}
std::string checked(std::string body) {char tail[8];snprintf(tail,sizeof(tail),"*%02X",visionCrc(body.data(),body.size()));return "@"+body+tail;}
void protocolTests() {
  auto original=face(42,123456);auto s=encoded(original);VisionPacket p;
  assert(parseVision(s.c_str(),p)&&p.seq==42&&p.x1==180);
  for(size_t i=1;i<s.size()-3;++i){auto bad=s;bad[i]^=1;assert(!parseVision(bad.c_str(),p));}
  for(auto body:{"P,1,1,1,900,1,1,2,2,3,4","P,1,1,1,900,1,,2,2,3","P,1,1,1,900,1,1,321,2,3","P,1,1,0,900,1,1,2,2,3","P,-1,1,1,900,1,1,2,2,3","P,4294967296,1,1,900,1,1,2,2,3"})assert(!parseVision(checked(body).c_str(),p));
  CamVisionAdapter adapter;int accepted=0;
  auto feed=[&](const std::string& line,uint64_t now){for(char c:line)if(adapter.feed(c,now,p))++accepted;};
  feed(s+"\r\n",10);assert(accepted==1);
  feed(s+"\n",20);assert(accepted==1&&adapter.stale==1);
  auto g=face(43,123457);g.kind='G';strcpy(g.label,"palm");feed(encoded(g)+"\n",30);assert(accepted==2);
  feed(std::string(300,'x')+s+"\n",40);assert(accepted==2&&adapter.bad==1);
  feed(encoded(face(44,123458))+"\n",50);assert(accepted==3);
  feed(encoded(face(0,1))+"\n",600);assert(accepted==4&&adapter.resets==1);
}
void followTests() {
  PersonFollowController f;f.reset();f.update(face(1,10));assert(f.ready()&&f.referenceArea()==6400);
  f.update(face(4,310,120,60));assert(f.output().vx>=0);assert(f.output().vx<=.250001&&fabs(f.output().vy)<=.200001);
  VisionPacket no;no.seq=5;no.receivedMs=700;f.update(no);assert(!f.lost());assert(f.output().vx==0);
  VisionPacket noAgain;noAgain.seq=6;noAgain.receivedMs=1600;f.update(noAgain);assert(f.lost());assert(f.output().vx==0);
  PersonFollowController transient;
  transient.update(face(1,100));transient.update(face(2,200));transient.update(face(3,300));
  VisionPacket oneMiss;oneMiss.seq=4;oneMiss.receivedMs=620;transient.update(oneMiss);
  assert(!transient.lost()&&transient.output().vx==0&&transient.output().wz==0);
  transient.update(face(5,940));assert(!transient.lost()&&transient.ready());
  VisionPacket firstMiss;firstMiss.seq=6;firstMiss.receivedMs=1260;transient.update(firstMiss);assert(!transient.lost());
  VisionPacket secondMiss;secondMiss.seq=7;secondMiss.receivedMs=2200;transient.update(secondMiss);assert(transient.lost());
  f.reset();for(int i=1;i<=3;++i)f.update(face(i,i*100,230));assert(f.output().vx==0&&f.output().vy==0&&f.output().wz>0&&f.output().wz<=.300001);
}
void displayTrackTests() {
  BoxTrack track;
  assert(track.updateDisplay(face(1,100,30)));
  assert(track.updateDisplay(face(2,300,45)));
  const auto fast=face(3,700,230,70);
  assert(!track.update(fast)); // Ordinary association rejects a large jump.
  assert(track.updateDisplay(fast)); // CAM already accepted it: UI re-anchors.
  const auto start=track.view(700,450);
  const auto middle=track.view(810,450);
  const auto anchored=track.view(940,450);
  assert(start.found&&middle.found&&anchored.found);
  assert(start.x0<middle.x0&&middle.x0<anchored.x0); // No teleport.
  assert(anchored.x0>=225&&anchored.x0<=235);
  assert(!track.updateDisplay(face(3,700,10))); // Replay cannot re-anchor.
  assert(track.updateDisplay(face(4,900,210,70)));
  const auto bounded=track.view(1900,450);
  const auto unbounded=track.view(1900);
  assert(bounded.found&&unbounded.found&&bounded.x0>unbounded.x0);
  const float shortAge=displayPredictionSeconds(350,450);
  const float longerAge=displayPredictionSeconds(700,450);
  const float staleAge=displayPredictionSeconds(1000,450);
  assert(shortAge<longerAge&&longerAge<staleAge&&staleAge<.45f); // Smooth taper, not a hard freeze.
}
SafetyController followReady() {
  SafetyController s;s.network(true,10);s.cameraPacket(10);s.person(face(1,10));assert(!s.setMode(1,Mode::Follow,10));return s;
}
void safetyTests() {
  {
    SafetyController manual;manual.network(true,10);manual.cameraPacket(10);
    assert(!manual.setMode(1,Mode::Manual,10));assert(!manual.velocity(1,.2,0,0,20));
    manual.velocity(1,0,0,0,30);manual.velocity(1,0,0,0,80);
    assert(manual.snapshot(80).stoppedAtMs==30);
    assert(!strcmp(manual.snapshot(80).stopReason,"release"));
  }
  auto s=followReady();
  for(int i=2;i<=4;++i){auto p=face(i,10+(i-1)*100);s.cameraPacket(p.receivedMs);s.heartbeat(1,p.receivedMs);s.person(p);s.computeFollow(p.receivedMs);}
  assert(s.followingReady());
  s.heartbeat(2,500);s.computeFollow(549);assert(s.snapshot(549).mode==Mode::Follow);
  s.computeFollow(700);assert(s.snapshot(700).mode==Mode::Follow); // Follow no longer drops on stale owner heartbeat.
  s=followReady();s.heartbeat(1,200);s.tick(510);assert(s.snapshot(510).mode==Mode::Follow); // Ping does not drop follow.
  s=followReady();for(int t=110;t<900;t+=100){s.cameraPacket(t);s.heartbeat(1,t);s.computeFollow(t);}s.tick(2100);assert(s.snapshot(2100).mode==Mode::Follow); // person timeout no longer drops follow.
  s=followReady();assert(s.velocity(1,.1,0,0,20));s.velocity(1,0,0,0,21);assert(s.snapshot(21).mode==Mode::Idle);
  s=followReady();s.emergency(20);assert(s.snapshot(20).estop);s.clear(1,21);assert(s.snapshot(21).mode==Mode::Idle);
  s=followReady();s.configureHardware(true,true);s.imu(true,true,false,20);s.tick(250);assert(s.snapshot(250).mode==Mode::Follow);s.tick(320);assert(s.snapshot(320).mode==Mode::Follow); // IMU timeout no longer drops follow.
  { // Person leaving the frame holds follow still; a 30 s re-identify grace applies.
    auto g=followReady();
    for(int i=2;i<=4;++i){auto p=face(i,10+(i-1)*100);g.cameraPacket(p.receivedMs);g.person(p);}
    assert(g.followingReady());
    for(uint64_t t=400;t<2000;t+=100){VisionPacket no;no.seq=uint32_t(t);no.receivedMs=t;no.found=false;g.cameraPacket(t);g.person(no);}
    assert(g.snapshot(1990).mode==Mode::Follow); // lost target holds follow, does not exit
    for(uint64_t t=2000;t<=34000;t+=100){VisionPacket no;no.seq=uint32_t(t);no.receivedMs=t;no.found=false;g.cameraPacket(t);g.person(no);}
    assert(g.snapshot(34000).mode==Mode::Idle); // 30 s re-identify grace expired
  }
}
void imuTests() {
  ImuFilter f;for(int t=10;t<=5100;t+=10)f.update(1,0,0,.1f,.2f,.3f,t);
  assert(f.state().calibrated&&f.state().valid);assert(fabs(f.state().biasX-.1f)<.001);
  for(int t=5110;t<=5320;t+=10)f.update(.707f,.707f,0,0,0,0,t);
  assert(!f.state().tiltFault);
  for(int t=5330;t<=5900;t+=10)f.update(.5f,.866f,0,0,0,0,t);
  assert(f.state().tiltFault);
  for(int t=5330;t<7350;t+=10)f.update(1,0,0,0,0,0,t);
  assert(!f.state().tiltFault);f.missing();assert(!f.state().valid);
}
void frameTests() {
  LatestFrame f;assert(f.acquire(0)==-1);assert(!f.publish(0,0));assert(!f.publish(0,LatestFrame::Capacity+1));
  assert(f.publish(0,100));auto held=f.acquire(0);assert(held==0);auto seq=f.sequence();assert(!f.publish(held,50));
  for(int i=0;i<100;++i){int slot=f.writable();assert(slot!=held);assert(f.publish(slot,200));}
  assert(f.length(held)==100);f.release(held);assert(f.acquire(seq)>=0);
}
void videoWorkQueueTests() {
  VideoWorkQueue q;
  assert(q.beginRead()==-1);
  const int first=q.beginWrite();assert(first>=0);
  assert(q.beginRead()==-1); // An unfinished RGB copy is not visible.
  assert(q.finishWrite(first));
  const int second=q.beginWrite();assert(second>=0&&second!=first);
  assert(q.finishWrite(second));
  assert(q.replaced()==1); // Slow encoder consumes the newest frame.
  const int held=q.beginRead();assert(held==second);
  const int spare=q.beginWrite();assert(spare==first);
  assert(q.beginRead()==-1); // Reader lease cannot be overwritten.
  assert(q.finishWrite(spare));
  q.endRead(held);
  assert(q.beginRead()==spare);
  q.endRead(spare);
  assert(q.beginWrite()>=0);
}
int main(){protocolTests();followTests();displayTrackTests();safetyTests();imuTests();frameTests();videoWorkQueueTests();std::cout<<"Tracking protocol, follow, safety, IMU and JPEG leases passed\n";}
