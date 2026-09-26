#include "continuous_servo_drive.h"

namespace carerover {

ContinuousServoDrive::ContinuousServoDrive()
    : pins_{0, 0, 0, 0},
      calibration_{{1500, 1500, 1500, 1500}, {1, 1, 1, 1}, 300},
      target_{{0, 0, 0, 0}},
      current_{{0, 0, 0, 0}},
      ready_(false),
      commandDeadlineMs_(0),
      lastUpdateMs_(0) {}

bool ContinuousServoDrive::begin(const uint8_t pins[WHEEL_COUNT],
                                 const ServoCalibration& calibration) {
  calibration_ = calibration;
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    pins_[i] = pins[i];
    if (!ledcAttach(pins_[i], PWM_FREQUENCY_HZ, PWM_RESOLUTION_BITS)) {
      for (uint8_t attached = 0; attached < i; ++attached) {
        ledcDetach(pins_[attached]);
      }
      ready_ = false;
      return false;
    }
  }
  ready_ = true;
  lastUpdateMs_ = millis();
  stopNow();
  return true;
}

void ContinuousServoDrive::setCalibration(const ServoCalibration& calibration) {
  calibration_ = calibration;
  stopNow();
}

bool ContinuousServoDrive::commandChassis(float vx, float vy, float wz,
                                          uint32_t durationMs) {
  return commandWheels(mixOrthogonalOmni(vx, vy, wz), durationMs);
}

bool ContinuousServoDrive::commandWheels(const WheelSpeeds& speeds,
                                         uint32_t durationMs) {
  if (!ready_ || durationMs == 0 || durationMs > MAX_COMMAND_DURATION_MS) {
    return false;
  }
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    if (!isfinite(speeds.value[i])) return false;
  }
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    target_.value[i] = clampUnit(speeds.value[i]);
  }
  commandDeadlineMs_ = millis() + durationMs;
  return true;
}

void ContinuousServoDrive::tick() {
  if (!ready_) return;

  const uint32_t now = millis();
  if (commandDeadlineMs_ != 0 && static_cast<int32_t>(now - commandDeadlineMs_) >= 0) {
    stopNow();
    return;
  }
  const uint32_t elapsedMs = now - lastUpdateMs_;
  if (elapsedMs < UPDATE_INTERVAL_MS) return;
  lastUpdateMs_ = now;

  const float maxStep = SLEW_PER_SECOND * static_cast<float>(elapsedMs) / 1000.0f;
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    const float error = target_.value[i] - current_.value[i];
    if (fabsf(error) <= maxStep) {
      current_.value[i] = target_.value[i];
    } else {
      current_.value[i] += (error > 0.0f) ? maxStep : -maxStep;
    }
  }
  writeAll();
}

void ContinuousServoDrive::stopNow() {
  commandDeadlineMs_ = 0;
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    target_.value[i] = 0.0f;
    current_.value[i] = 0.0f;
  }
  if (ready_) writeAll();
}

bool ContinuousServoDrive::moving() const {
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    if (fabsf(current_.value[i]) > ZERO_EPSILON ||
        fabsf(target_.value[i]) > ZERO_EPSILON) {
      return true;
    }
  }
  return false;
}

uint32_t ContinuousServoDrive::microsToDuty(uint16_t pulseUs) {
  const uint32_t maxDuty = (1UL << PWM_RESOLUTION_BITS) - 1UL;
  return static_cast<uint32_t>(
      (static_cast<uint64_t>(pulseUs) * maxDuty + PWM_PERIOD_US / 2) /
      PWM_PERIOD_US);
}

void ContinuousServoDrive::writeWheel(uint8_t wheel, float normalizedSpeed) {
  normalizedSpeed = clampUnit(normalizedSpeed);
  int32_t pulseUs = static_cast<int32_t>(calibration_.neutralUs[wheel]);
  pulseUs += static_cast<int32_t>(lroundf(
      normalizedSpeed * calibration_.directionSign[wheel] *
      (calibration_.wheelSpanUs[wheel] ? calibration_.wheelSpanUs[wheel] : calibration_.speedSpanUs)));
  if (pulseUs < 900) pulseUs = 900;
  if (pulseUs > 2100) pulseUs = 2100;
  ledcWrite(pins_[wheel], microsToDuty(static_cast<uint16_t>(pulseUs)));
}

void ContinuousServoDrive::writeAll() {
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    writeWheel(i, current_.value[i]);
  }
}

}  // namespace carerover
