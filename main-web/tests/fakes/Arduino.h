#pragma once
#include <cstdint>
#include <cmath>
extern uint32_t fakeNow;
extern int attachCount,failAttach;
extern uint32_t duties[64];
inline uint32_t millis(){return fakeNow;}
inline bool ledcAttach(uint8_t,uint32_t,uint8_t){return ++attachCount!=failAttach;}
inline void ledcDetach(uint8_t){}
inline bool ledcWrite(uint8_t pin,uint32_t duty){duties[pin]=duty;return true;}
