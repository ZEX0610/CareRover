#pragma once

#include <Arduino.h>

#include "omni_kinematics.h"

namespace carerover {

struct ServoCalibration {
  uint16_t neutralUs[WHEEL_COUNT];
  int8_t directionSign[WHEEL_COUNT];
  uint16_t speedSpanUs;
  uint16_t wheelSpanUs[WHEEL_COUNT] = {}; // Zero inherits legacy global span.
};

class ContinuousServoDrive {
 public:
  static constexpr uint32_t PWM_FREQUENCY_HZ = 50;
  // ESP32-S3 LEDC supports up to 14-bit duty resolution. At 50 Hz this still
  // gives about 1.22 us per step, comfortably finer than servo calibration.
  static constexpr uint8_t PWM_RESOLUTION_BITS = 14;
  static constexpr uint32_t MAX_COMMAND_DURATION_MS = 5000;

  ContinuousServoDrive();

  bool begin(const uint8_t pins[WHEEL_COUNT], const ServoCalibration& calibration);
  void setCalibration(const ServoCalibration& calibration);

  // These commands are deliberately time-bounded. The caller must refresh them
  // before durationMs expires; otherwise the drive immediately stops.
  bool commandChassis(float vx, float vy, float wz, uint32_t durationMs);
  bool commandWheels(const WheelSpeeds& speeds, uint32_t durationMs);

  void tick();
  void stopNow();
  bool ready() const { return ready_; }
  bool moving() const;
  const WheelSpeeds& target() const { return target_; }
  const WheelSpeeds& current() const { return current_; }
  const ServoCalibration& calibration() const { return calibration_; }

 private:
  static constexpr uint32_t PWM_PERIOD_US = 1000000UL / PWM_FREQUENCY_HZ;
  static constexpr uint32_t UPDATE_INTERVAL_MS = 10;
  static constexpr float SLEW_PER_SECOND = 2.5f;
  static constexpr float ZERO_EPSILON = 0.005f;

  uint8_t pins_[WHEEL_COUNT];
  ServoCalibration calibration_;
  WheelSpeeds target_;
  WheelSpeeds current_;
  bool ready_;
  uint32_t commandDeadlineMs_;
  uint32_t lastUpdateMs_;

  static uint32_t microsToDuty(uint16_t pulseUs);
  void writeWheel(uint8_t wheel, float normalizedSpeed);
  void writeAll();
};

}  // namespace carerover
