#pragma once
#include "cliff_guard.h"

// Physical order: front-left, front-right, rear-right, rear-left.
// YL-62 OUT: LOW = table detected; HIGH = table edge/no reflection.
// Power installed modules from 3V3 and share GND with the mainboard.
namespace carerover {
constexpr int CliffFrontLeftPin = 40;
constexpr int CliffFrontRightPin = 41;
constexpr int CliffRearRightPin = 47;
constexpr int CliffRearLeftPin = 48;
// All four downward-facing sensors are now connected. A missing/held-HIGH
// output blocks its corresponding manual/watch direction until checked.
constexpr uint8_t CliffInstalledMask = CliffFrontLeft | CliffFrontRight |
                                       CliffRearRight | CliffRearLeft;
}
