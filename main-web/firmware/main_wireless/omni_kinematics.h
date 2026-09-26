#pragma once

#include <math.h>
#include <stdint.h>

namespace carerover {

enum WheelIndex : uint8_t {
  FRONT_LEFT = 0,
  FRONT_RIGHT = 1,
  REAR_LEFT = 2,
  REAR_RIGHT = 3,
  WHEEL_COUNT = 4
};

struct WheelSpeeds {
  float value[WHEEL_COUNT];
};

inline float clampUnit(float value) {
  if (value > 1.0f) return 1.0f;
  if (value < -1.0f) return -1.0f;
  return value;
}

// Coordinate contract shared with the CareRover webpage:
//   vx > 0: forward, vy > 0: move right, wz > 0: rotate clockwise.
//
// Physical drive-axis contract (top of chassis is FRONT):
//
//   FL / GPIO10: longitudinal       FR / GPIO12: lateral
//            passive lateral rollers / passive longitudinal rollers
//
//   RL / GPIO13: lateral            RR / GPIO11: longitudinal
//
// This is two orthogonal active wheel pairs. It is not an X-drive. During
// forward motion GPIO12/GPIO13 stay neutral and roll passively; during lateral
// motion GPIO10/GPIO11 stay neutral and roll passively.
//
// Positive logical values mean forward for the longitudinal pair and right for
// the lateral pair. Per-servo electrical inversion is handled below this layer.
inline WheelSpeeds mixOrthogonalOmni(float vx, float vy, float wz) {
  vx = clampUnit(vx);
  vy = clampUnit(vy);
  wz = clampUnit(wz);

  WheelSpeeds output = {{
    vx + wz,  // front-left / GPIO10: forward pair; CW requires forward
    vy + wz,  // front-right / GPIO12: right pair; CW requires right
    vy - wz,  // rear-left / GPIO13: right pair; CW requires left
    vx - wz   // rear-right / GPIO11: forward pair; CW requires backward
  }};

  float peak = 1.0f;
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    const float magnitude = fabsf(output.value[i]);
    if (magnitude > peak) peak = magnitude;
  }
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    output.value[i] /= peak;
  }
  return output;
}

}  // namespace carerover
