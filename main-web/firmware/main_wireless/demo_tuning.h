#pragma once
#include <cstdint>
#if defined(ARDUINO) && __has_include("build_version.h")
#include "build_version.h"
#endif
#if defined(CONFIG_CAREROVER_TUNING_PROFILE) && !defined(CAREROVER_TUNING_PROFILE)
#define CAREROVER_TUNING_PROFILE CONFIG_CAREROVER_TUNING_PROFILE
#endif
#ifndef CAREROVER_TUNING_PROFILE
#define CAREROVER_TUNING_PROFILE 0
#endif
static_assert(CAREROVER_TUNING_PROFILE>=0 && CAREROVER_TUNING_PROFILE<=2,"Invalid tuning profile");
namespace carerover::tuning {
inline constexpr bool balanced=CAREROVER_TUNING_PROFILE==1;
inline constexpr const char* name=balanced?"DEMO_BALANCED":CAREROVER_TUNING_PROFILE==2?"DIAGNOSTIC_RAW":"SAFE_BASELINE";
// Display thresholds are intentionally more permissive than action thresholds:
// a demo may keep showing a plausible result, while motion actions still use
// their independent confirmation gate.
inline constexpr uint16_t GestureDisplayEnter=balanced?300:350, GestureDisplayHold=balanced?140:180;
inline constexpr uint16_t GestureActionScore=balanced?400:450;
inline constexpr uint64_t GestureDisplayHoldMs=2600, GestureDisplayNoHandMs=1700;
// CAM alternates gesture/face frames, so a single scheduling gap can approach
// 0.5 s. Keep source freshness looser than the nominal frame period; the
// command watchdog remains the independent dead-man stop.
inline constexpr uint64_t CameraSafetyMs=1200, PersonSafetyMs=1200, ImuSafetyMs=750, CommandSafetyMs=300;
// A person leaving the frame must not immediately drop follow; hold still and
// wait for re-identification, giving up only after this grace elapses.
inline constexpr uint64_t FollowLostTimeoutMs=30000;
inline constexpr uint64_t PersonDisplayMs=1400, HrDisplayHoldMs=30000, Spo2DisplayHoldMs=balanced?12000:8000;
// A demo chassis may vibrate and briefly exceed 40 degrees in accelerometer
// projection. Require a genuinely large, sustained tilt before stopping.
inline constexpr float ImuSafetyTiltDeg=55, ImuSafetyRecoverDeg=42;
inline constexpr uint64_t ImuSafetyTiltMs=400;
}
