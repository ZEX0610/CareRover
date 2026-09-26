#pragma once
#include <Arduino.h>
#include "imu_filter.h"
namespace carerover {
// Dedicated GPIO6/7 bus. No global interrupt mask, no Wire controller reuse.
class Mpu6050Soft {
 public:
  explicit Mpu6050Soft(uint8_t address=0x68):address_(address) {}
  bool begin() {
    pinMode(Sda,OUTPUT_OPEN_DRAIN);pinMode(Scl,OUTPUT_OPEN_DRAIN);digitalWrite(Sda,HIGH);digitalWrite(Scl,HIGH);
    const uint8_t preferred=address_;
    const uint8_t candidates[2]={preferred,uint8_t(preferred==0x68?0x69:0x68)};
    for(const uint8_t candidate:candidates) {
      address_=candidate;uint8_t id=0;
      ready_=read(0x75,&id,1)&&(id&0x7e)==0x68&&write(0x6b,1)&&write(0x1a,3)&&write(0x19,9)&&write(0x1b,0)&&write(0x1c,0);
      if(ready_) { filter_=ImuFilter{};return true; }
    }
    address_=preferred;ready_=false;return false;
  }
  uint8_t address() const {return address_;}
  const ImuSample& sample(uint64_t now) {
    uint8_t b[14];
    if(!ready_||!read(0x3b,b,14)) { filter_.missing();ready_=false;return filter_.state(); }
    auto s=[&](int i){return int16_t(uint16_t(b[i])<<8|b[i+1]);};
    filter_.update(s(0)/16384.f,s(2)/16384.f,s(4)/16384.f,s(8)/131.f,s(10)/131.f,s(12)/131.f,now);
    return filter_.state();
  }
 private:
  static constexpr int Sda=6,Scl=7;
  uint8_t address_; bool ready_=false; uint32_t began_=0; ImuFilter filter_;
  bool expired() const {return uint32_t(micros()-began_)>4000;}
  bool high() {
    digitalWrite(Scl,HIGH); const uint32_t start=micros();
    while(!digitalRead(Scl)) if(uint32_t(micros()-start)>200||expired()) return false;
    delayMicroseconds(5);return !expired();
  }
  void low() {digitalWrite(Scl,LOW);delayMicroseconds(5);}
  bool start() {digitalWrite(Sda,HIGH);if(!high())return false;digitalWrite(Sda,LOW);delayMicroseconds(5);low();return true;}
  void stop() {digitalWrite(Sda,LOW);digitalWrite(Scl,HIGH);delayMicroseconds(5);digitalWrite(Sda,HIGH);}
  void recover() {digitalWrite(Sda,HIGH);for(int i=0;i<9;++i){low();digitalWrite(Scl,HIGH);delayMicroseconds(5);}stop();}
  bool send(uint8_t byte) {
    for(int i=7;i>=0;--i) {digitalWrite(Sda,(byte>>i)&1);if(!high())return false;low();}
    digitalWrite(Sda,HIGH);if(!high())return false;bool ack=!digitalRead(Sda);low();return ack;
  }
  bool receive(uint8_t& byte,bool ack) {
    digitalWrite(Sda,HIGH);byte=0;
    for(int i=0;i<8;++i) {if(!high())return false;byte=uint8_t((byte<<1)|digitalRead(Sda));low();}
    digitalWrite(Sda,ack?LOW:HIGH);if(!high())return false;low();digitalWrite(Sda,HIGH);return true;
  }
  bool finish(bool ok) {stop();if(!ok)recover();return ok;}
  bool write(uint8_t reg,uint8_t value) {began_=micros();return finish(start()&&send(address_<<1)&&send(reg)&&send(value));}
  bool read(uint8_t reg,uint8_t* bytes,size_t n) {
    began_=micros();bool ok=start()&&send(address_<<1)&&send(reg)&&start()&&send(uint8_t(address_<<1|1));
    for(size_t i=0;i<n&&ok;++i)ok=receive(bytes[i],i+1<n);
    return finish(ok);
  }
};
}
