#pragma once
#include <Arduino.h>
using gpio_num_t=int;
inline int gpio_get_level(int p){return levels[p];}
