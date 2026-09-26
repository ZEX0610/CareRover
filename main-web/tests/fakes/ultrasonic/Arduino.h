#pragma once
#include <cstdint>
#define ARDUINO_ISR_ATTR
#define OUTPUT 1
#define INPUT 0
#define HIGH 1
#define LOW 0
#define CHANGE 3
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void portENTER_CRITICAL(int*){}
inline void portEXIT_CRITICAL(int*){}
inline void portENTER_CRITICAL_ISR(int*){}
inline void portEXIT_CRITICAL_ISR(int*){}
inline uint32_t clockUs=0;
inline int levels[49]={},highPulses=0;
inline void (*callback)(void*)=nullptr;
inline void* callbackArg=nullptr;
inline void pinMode(int,int){}
inline int digitalRead(int p){return levels[p];}
inline void digitalWrite(int p,int value){levels[p]=value;if(value)++highPulses;}
inline void delayMicroseconds(uint32_t us){clockUs+=us;}
inline uint32_t micros(){return clockUs;}
inline void attachInterruptArg(int,void (*fn)(void*),void* arg,int){callback=fn;callbackArg=arg;}
inline void echoEdge(int pin,int level,uint32_t time){levels[pin]=level;clockUs=time;callback(callbackArg);}
