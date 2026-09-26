#pragma once

#include <stdint.h>

#include "omni_kinematics.h"

namespace carerover {

// Physical layout confirmed on 2026-09-13, viewed from above with the marked
// front of the chassis at the top:
//   FL GPIO10 (longitudinal) ---- GPIO12 (lateral) FR
//   RL GPIO13 (lateral)      ---- GPIO11 (longitudinal) RR
//
// Array order is the WheelIndex contract: FL, FR, RL, RR.
static constexpr uint8_t MOTION_SERVO_PINS[WHEEL_COUNT] = {10, 12, 13, 11};

// Persisted with calibration so values measured for a previous wiring layout
// can never silently enable motion on this layout.
static constexpr uint32_t MOTION_LAYOUT_ID = 0x4F4D4E32UL;  // "OMN2"

}  // namespace carerover
