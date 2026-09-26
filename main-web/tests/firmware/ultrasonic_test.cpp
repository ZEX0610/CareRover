#include "hcsr04.h"
#include <cassert>
#include <iostream>
using namespace carerover;
int main(){
  Hcsr04 sensor;FrontInstallation install;assert(!sensor.begin(install));install.trig=14;install.echo=15;assert(sensor.begin(install));
  double cm=0;bool valid=false;
  assert(!sensor.service(0,cm,valid));assert(highPulses==1&&levels[14]==LOW);
  echoEdge(15,HIGH,100);echoEdge(15,LOW,5900);
  assert(sensor.service(6000,cm,valid)&&valid&&cm==100);
  assert(!sensor.service(69999,cm,valid)&&highPulses==1);
  assert(!sensor.service(70000,cm,valid)&&highPulses==2);
  assert(!sensor.service(99999,cm,valid));assert(sensor.service(100000,cm,valid)&&!valid); // no echo
  levels[15]=HIGH;assert(sensor.service(140000,cm,valid)&&!valid&&highPulses==2);levels[15]=LOW;
  assert(!sensor.service(210000,cm,valid));echoEdge(15,HIGH,210100);echoEdge(15,LOW,210150);
  assert(sensor.service(210200,cm,valid)&&!valid); // short glitch / blind zone
  Hcsr04 wrapped;assert(wrapped.begin(install));assert(!wrapped.service(0xfffff000U,cm,valid));
  echoEdge(15,HIGH,0xfffff100U);echoEdge(15,LOW,uint32_t(0xfffff100U+5800));
  assert(wrapped.service(uint32_t(0xfffff100U+5900),cm,valid)&&valid&&cm==100);
  std::cout<<"HC-SR04 driver: pulse spacing, edge capture, timeout, stuck high, invalid pulse and timer wrap passed\n";
}
