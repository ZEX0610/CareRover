#pragma once
#include <Arduino.h>
#include <driver/gpio.h>
#include "front_config.h"
namespace carerover {
// Interrupt captures only pulse edges; no pulseIn / wait-for-echo in any task.
// service() belongs to one 1ms task; ISR state is copied under its own short lock.
class Hcsr04 {
 public:
  bool begin(const FrontInstallation& i) {
    if(!frontPinsReady(i))return false;
    trig_=i.trig;echo_=i.echo;pinMode(trig_,OUTPUT);digitalWrite(trig_,LOW);pinMode(echo_,INPUT);
    attachInterruptArg(echo_,edge,this,CHANGE);return true;
  }
  bool service(uint32_t nowUs,double& cm,bool& valid) {
    bool completed=false;uint32_t width=0;
    portENTER_CRITICAL(&mux_);
    if(done_) { width=width_;done_=false;armed_=false;completed=true; }
    portEXIT_CRITICAL(&mux_);
    if(completed) { pending_=false;cm=width/58.0;valid=width>=116&&width<=23200;return true; }
    if(pending_&&uint32_t(nowUs-startUs_)>=30000) {
      portENTER_CRITICAL(&mux_);armed_=false;risen_=false;done_=false;portEXIT_CRITICAL(&mux_);
      pending_=false;cm=0;valid=false;return true;
    }
    if(!pending_&&(!started_||uint32_t(nowUs-startUs_)>=70000)) {
      started_=true;startUs_=nowUs;
      if(digitalRead(echo_)==HIGH) { cm=0;valid=false;return true; }
      portENTER_CRITICAL(&mux_);armed_=true;risen_=false;done_=false;portEXIT_CRITICAL(&mux_);
      pending_=true;digitalWrite(trig_,HIGH);delayMicroseconds(10);digitalWrite(trig_,LOW);
    }
    return false;
  }
 private:
  static void ARDUINO_ISR_ATTR edge(void* arg) {
    auto* self=static_cast<Hcsr04*>(arg);const auto now=micros();
    portENTER_CRITICAL_ISR(&self->mux_);
    if(self->armed_) {
      if(gpio_get_level(gpio_num_t(self->echo_))) { if(!self->risen_){self->riseUs_=now;self->risen_=true;} }
      else if(self->risen_) { self->width_=uint32_t(now-self->riseUs_);self->done_=true;self->armed_=false; }
    }
    portEXIT_CRITICAL_ISR(&self->mux_);
  }
  portMUX_TYPE mux_=portMUX_INITIALIZER_UNLOCKED;
  int trig_=-1,echo_=-1;bool pending_=false,started_=false;
  volatile bool armed_=false,risen_=false,done_=false;
  volatile uint32_t riseUs_=0,width_=0;uint32_t startUs_=0;
};
}
