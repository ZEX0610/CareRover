#pragma once
#include <cstdint>
struct WirelessHealth {
  bool ready = false, finger = false, hrValid = false, spo2Valid = false;
  bool hrHeld=false,spo2Held=false;
  uint64_t hrFreshMs=0,spo2FreshMs=0;
  float quality=0;
  int32_t hr = 0, spo2 = 0;
  uint8_t sqi = 0;
  uint64_t sampleMs = 0, reportMs = 0;
  char state[24] = "sensor_missing";
};
void wirelessBegin();
void wirelessGesture(const char* label, float score, bool accepted, bool actionEligible, bool held, uint64_t ageMs,
                     uint32_t inferMs);
void wirelessHealth(const WirelessHealth& health);
void wirelessPpg(uint32_t ir);
void wirelessStatus();
uint64_t wirelessNowMs();

#include "vision_protocol.h"
void wirelessVision(const carerover::VisionPacket& packet, bool resync);

void wirelessDiagnostics();

#include "front_guard.h"
carerover::FrontSnapshot wirelessFront();
const char* wirelessStopReason();
