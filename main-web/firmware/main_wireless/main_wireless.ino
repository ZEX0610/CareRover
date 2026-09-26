#include <Arduino.h>
#include <Wire.h>
#include <esp_arduino_version.h>
#include <math.h>
#include "MAX30105.h"
#include "demo_tuning.h"
#include "demo_signal_filters.h"
#include "demo_ppg.h"
#include "gesture_actions.h"
#include "health_quality.h"
#include "oled_ui.h"
#include "ppg_rate_estimator.h"
#include "signal_state_filters.h"
#include "wireless_runtime.h"

// CareRover main ESP32-S3 pin allocation.
// CAM UART remains available while the local SensorBus is used by MAX30102.
static constexpr int CAM_RX_PIN = 18;
static constexpr int CAM_TX_PIN = 17;
static constexpr int SENSOR_SDA_PIN = 8;
static constexpr int SENSOR_SCL_PIN = 9;
static constexpr int OLED_SDA_PIN = 4;
static constexpr int OLED_SCL_PIN = 5;
static constexpr uint8_t MAX30102_ADDRESS = 0x57;
static constexpr uint8_t EXPECTED_PART_ID = 0x15;

static constexpr uint32_t PC_BAUD = 115200;
static constexpr uint32_t CAM_BAUD = 115200;
static constexpr uint32_t SENSOR_I2C_HZ = 400000;
static constexpr uint32_t OLED_I2C_HZ = 400000;
static constexpr uint32_t LINK_TIMEOUT_MS = carerover::tuning::CameraSafetyMs;
static constexpr uint32_t HEALTH_REPORT_MS = 1000;
static constexpr uint32_t SENSOR_RETRY_MS = 3000;

static constexpr size_t RX_LINE_CAPACITY = 256;
static constexpr size_t PPG_WINDOW_SIZE = 200;
static constexpr size_t PPG_STEP_SIZE = 25;
static constexpr size_t FILTER_HALF_WIDTH = 25;
static constexpr float PI_F = 3.14159265358979323846f;
static constexpr uint32_t FINGER_IR_THRESHOLD = 50000;
static constexpr uint8_t FINGER_LOST_SAMPLES = 5;
static constexpr uint8_t INITIAL_LED_POWER = 30;
static constexpr uint32_t IR_TARGET_LOW = 90000;
static constexpr uint32_t IR_TARGET_HIGH = 190000;
static constexpr uint16_t CAM_SOURCE_ACCEPT_SCORE_MILLI = 450;
static constexpr uint16_t GESTURE_ENTER_SCORE_MILLI = carerover::tuning::GestureDisplayEnter;
static constexpr uint16_t GESTURE_HOLD_SCORE_MILLI = carerover::tuning::GestureDisplayHold;
static constexpr uint8_t GESTURE_ENTER_FRAMES = 2;
static constexpr uint8_t GESTURE_SWITCH_FRAMES = 2;
static constexpr uint8_t GESTURE_CLASSIFY_GRACE_FRAMES = 7;
static constexpr uint8_t GESTURE_NO_HAND_GRACE_FRAMES = 3;
static constexpr float PPG_MIN_AC_DC = 0.00002f;
static constexpr float PPG_MAX_AC_DC = 0.030f;
static constexpr float PPG_MIN_SPECTRAL_SNR = 2.0f;
static constexpr float SPO2_MIN_RED_IR_CORRELATION = 0.30f;
static constexpr uint8_t HEALTH_GOOD_WINDOWS = 2;
static constexpr uint8_t HR_GRACE_WINDOWS = 30;
static constexpr uint8_t SPO2_GRACE_WINDOWS = 8;
static constexpr int32_t HR_MAX_WINDOW_DELTA = 12;
static constexpr int32_t SPO2_MAX_WINDOW_DELTA = 6;
static constexpr int32_t HR_MAX_OUTPUT_STEP = 3;
static constexpr int32_t SPO2_MAX_OUTPUT_STEP = 1;

HardwareSerial CamLink(1);
TwoWire OledWire(1);
MAX30105 particleSensor;
OledUi oledUi;
carerover::GestureDisplay gestureFilter(GESTURE_ENTER_SCORE_MILLI, GESTURE_HOLD_SCORE_MILLI,
                                GESTURE_ENTER_FRAMES, GESTURE_SWITCH_FRAMES,
                                GESTURE_CLASSIFY_GRACE_FRAMES,
                                GESTURE_NO_HAND_GRACE_FRAMES);
StableMetric heartRateFilter(HEALTH_GOOD_WINDOWS, HR_GRACE_WINDOWS,
                             HR_MAX_WINDOW_DELTA, HR_MAX_OUTPUT_STEP);
StableMetric spo2Filter(HEALTH_GOOD_WINDOWS, SPO2_GRACE_WINDOWS,
                        SPO2_MAX_WINDOW_DELTA, SPO2_MAX_OUTPUT_STEP);
Median5Filter heartRateMedian;
Median5Filter spo2Median;
carerover::DemoPpg demoPpg;
uint8_t fingerEnterSamples=0;
uint64_t hrFreshMs=0,spo2FreshMs=0;
int32_t currentHr(){return carerover::tuning::balanced?demoPpg.hr.output():heartRateFilter.output();}
int32_t currentSpo2(){return carerover::tuning::balanced?demoPpg.spo2.output():spo2Filter.output();}
bool hrHeld(){return carerover::tuning::balanced?demoPpg.hr.held():heartRateFilter.held();}
bool spo2Held(){return carerover::tuning::balanced?demoPpg.spo2.held():spo2Filter.held();}

char camRxLine[RX_LINE_CAPACITY];
size_t camRxLength = 0;
char pcCommand[24];
size_t pcCommandLength = 0;
bool rawStreaming = false;
uint32_t lastValidPacketMs = 0;
uint32_t validPackets = 0;
uint32_t badPackets = 0;
bool timeoutReported = false;

uint32_t irBuffer[PPG_WINDOW_SIZE];
uint32_t redBuffer[PPG_WINDOW_SIZE];
uint32_t sampleMsBuffer[PPG_WINDOW_SIZE];
float filterScratch[PPG_WINDOW_SIZE];
float irFiltered[PPG_WINDOW_SIZE];
float redFiltered[PPG_WINDOW_SIZE];
float spectralPowers[128];
float ppgWindowWeights[PPG_WINDOW_SIZE];
size_t ppgSamples = 0;
uint32_t totalSamples = 0;
uint32_t samplesSinceReport = 0;
uint32_t lastSampleMs = 0;
uint64_t wirelessSampleMs = 0, wirelessReportMs = 0;
uint32_t lastHealthReportMs = 0;
uint32_t lastSensorAttemptMs = 0;
uint32_t lastIr = 0;
uint32_t lastRed = 0;
uint32_t fifoOverflowTotal = 0;
uint8_t noFingerSamples = FINGER_LOST_SAMPLES;
uint8_t ledPower = INITIAL_LED_POWER;
bool sensorReady = false;
bool fingerPresent = false;
bool measurementValid = false;
bool heartRateValid = false;
bool spo2Valid = false;
int32_t heartRate = 0;
int32_t spo2 = 0;
uint8_t sqi = 0;
float effectiveSampleHz = 0.0f;
float windowIrMean = 0.0f;
float windowAcDc = 0.0f;
float windowCorrelation = 0.0f;
float windowSpectralSnr = 0.0f;
float windowPulseCorrelation = 0.0f;
float windowRatioR = 0.0f;
float windowSampleHz = 0.0f;
bool algorithmHrValid = false;
bool algorithmSpo2Valid = false;
const char *healthState = "sensor_missing";

uint8_t crc8(const char *data, size_t length) {
  uint8_t crc = 0;
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint8_t>(data[i]);
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80U) ? static_cast<uint8_t>((crc << 1U) ^ 0x07U)
                          : static_cast<uint8_t>(crc << 1U);
    }
  }
  return crc;
}

bool parseHexByte(const char *text, uint8_t &value) {
  if (text == nullptr || text[0] == '\0' || text[1] == '\0') return false;
  char *end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 16);
  if (end != text + 2 || parsed > 0xFFUL) return false;
  value = static_cast<uint8_t>(parsed);
  return true;
}

void printProtocolError(const char *reason, const char *line) {
  ++badPackets;
  Serial.printf("{\"type\":\"protocol_error\",\"reason\":\"%s\",\"bad_packets\":%lu,\"raw\":\"%s\"}\r\n",
                reason, static_cast<unsigned long>(badPackets), line);
}

void processCamPacket(const carerover::VisionPacket& p, bool resync) {
  wirelessVision(p, resync);
  if (p.kind == 'P') {
    ++validPackets; lastValidPacketMs=millis(); timeoutReported=false;
    Serial.printf("{\"type\":\"person\",\"seq\":%lu,\"found\":%s,\"confidence\":%.3f,\"infer_ms\":%lu}\n",
      (unsigned long)p.seq,p.found?"true":"false",p.score/1000.0f,(unsigned long)p.inferMs);
    return;
  }
  const auto seq=p.seq, camMs=p.camMs, inferMs=p.inferMs;
  const int handDetected=p.found, scoreMilli=p.score, x0=p.x0,y0=p.y0,x1=p.x1,y1=p.y1;
  const char* label=p.label;
  ++validPackets;
  lastValidPacketMs = millis();
  timeoutReported = false;
  const bool sourceAccepted = handDetected != 0 && scoreMilli >= CAM_SOURCE_ACCEPT_SCORE_MILLI &&
                              strcmp(label, "no_gesture") != 0 && strcmp(label, "no_hand") != 0;
  const uint16_t boundedScore = static_cast<uint16_t>(constrain(scoreMilli, 0, 1000));
  gestureFilter.update(handDetected != 0, label, boundedScore, p.receivedMs);
  const bool actionEligible = boundedScore >= carerover::tuning::GestureActionScore && gestureFilter.accepted() && !gestureFilter.holding() &&
                               carerover::gestureActionBoxValid(label, x0, y0, x1, y1);
  wirelessGesture(gestureFilter.label(), gestureFilter.scoreMilli() / 1000.0f,
                  gestureFilter.accepted(), actionEligible, gestureFilter.holding(), gestureFilter.age(p.receivedMs), inferMs);

  Serial.printf(
      "{\"type\":\"gesture\",\"seq\":%lu,\"cam_ms\":%lu,\"hand\":%s,"
      "\"label\":\"%s\",\"score\":%.3f,\"accepted\":%s,\"holding\":%s,"
      "\"source_label\":\"%s\",\"source_score\":%.3f,\"source_accepted\":%s,"
      "\"box\":[%d,%d,%d,%d],\"infer_ms\":%lu,\"valid_packets\":%lu}\r\n",
      static_cast<unsigned long>(seq), static_cast<unsigned long>(camMs),
      handDetected ? "true" : "false", gestureFilter.label(),
      gestureFilter.scoreMilli() / 1000.0f, gestureFilter.accepted() ? "true" : "false",
      gestureFilter.holding() ? "true" : "false", label, scoreMilli / 1000.0f,
      sourceAccepted ? "true" : "false", x0, y0, x1, y1,
      static_cast<unsigned long>(inferMs), static_cast<unsigned long>(validPackets));
}

carerover::CamVisionAdapter camAdapter;
void readCamLink() {
  // Bound each loop so a noisy serial source cannot starve health sampling.
  for (int budget=0; budget<512 && CamLink.available()>0; ++budget) {
    carerover::VisionPacket packet;
    if(camAdapter.feed(char(CamLink.read()),wirelessNowMs(),packet)) processCamPacket(packet,camAdapter.resynchronized);
  }
  badPackets=camAdapter.bad+camAdapter.stale;
  static uint32_t reportMs=0;
  if(millis()-reportMs>=1000) {
    reportMs=millis();
    Serial.printf("{\"type\":\"vision_link\",\"valid\":%lu,\"bad\":%lu,\"stale\":%lu,\"resync\":%lu}\n",(unsigned long)camAdapter.valid,(unsigned long)camAdapter.bad,(unsigned long)camAdapter.stale,(unsigned long)camAdapter.resets);
  }
}

void processPcCommand() {
  pcCommand[pcCommandLength] = '\0';
  if (strcmp(pcCommand, "RAW_ON") == 0) {
    rawStreaming = true;
    Serial.println("{\"type\":\"command_ack\",\"command\":\"RAW_ON\",\"ok\":true}");
  } else if (strcmp(pcCommand, "RAW_OFF") == 0) {
    rawStreaming = false;
    Serial.println("{\"type\":\"command_ack\",\"command\":\"RAW_OFF\",\"ok\":true}");
  } else if (strcmp(pcCommand, "STATUS") == 0) {
    wirelessStatus();
    const uint32_t now = millis();
    const bool camConnected = validPackets > 0 && !timeoutReported &&
                              now - lastValidPacketMs <= LINK_TIMEOUT_MS;
    Serial.printf("{\"type\":\"system_status\",\"oled_ready\":%s,\"max30102_ready\":%s,"
                  "\"cam_connected\":%s,\"oled_sda_gpio\":%d,\"oled_scl_gpio\":%d,"
                  "\"sensor_sda_gpio\":%d,\"sensor_scl_gpio\":%d}\r\n",
                  oledUi.ready() ? "true" : "false", sensorReady ? "true" : "false",
                  camConnected ? "true" : "false", OLED_SDA_PIN, OLED_SCL_PIN,
                  SENSOR_SDA_PIN, SENSOR_SCL_PIN);
  } else if (pcCommandLength > 0) {
    Serial.printf("{\"type\":\"command_ack\",\"command\":\"%s\",\"ok\":false}\r\n", pcCommand);
  }
  pcCommandLength = 0;
}

void readPcCommands() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      processPcCommand();
      continue;
    }
    if (pcCommandLength + 1 < sizeof(pcCommand)) {
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
      pcCommand[pcCommandLength++] = c;
    } else {
      pcCommandLength = 0;
    }
  }
}

bool i2cAddressPresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void resetPpgState(const char *state) {
  ppgSamples = 0;
  demoPpg.reset();hrFreshMs=spo2FreshMs=0;
  heartRateFilter.reset();
  spo2Filter.reset();
  heartRateMedian.reset();
  spo2Median.reset();
  algorithmHrValid = false;
  algorithmSpo2Valid = false;
  windowIrMean = 0.0f;
  windowAcDc = 0.0f;
  windowCorrelation = 0.0f;
  windowSpectralSnr = 0.0f;
  windowPulseCorrelation = 0.0f;
  windowRatioR = 0.0f;
  windowSampleHz = 0.0f;
  measurementValid = false;
  heartRateValid = false;
  spo2Valid = false;
  heartRate = 0;
  spo2 = 0;
  sqi = 0;
  healthState = state;
}

bool initializeMax30102() {
  lastSensorAttemptMs = millis();
  if (!i2cAddressPresent(MAX30102_ADDRESS)) {
    sensorReady = false;
    resetPpgState("sensor_missing");
    Serial.printf("{\"type\":\"sensor_status\",\"sensor\":\"MAX30102\",\"state\":\"not_found\",\"address\":\"0x57\",\"sda_gpio\":%d,\"scl_gpio\":%d}\r\n",
                  SENSOR_SDA_PIN, SENSOR_SCL_PIN);
    return false;
  }

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST, MAX30102_ADDRESS)) {
    sensorReady = false;
    resetPpgState("sensor_error");
    Serial.println("{\"type\":\"sensor_status\",\"sensor\":\"MAX30102\",\"state\":\"begin_failed\"}");
    return false;
  }

  const uint8_t partId = particleSensor.readPartID();
  const uint8_t revisionId = particleSensor.getRevisionID();
  if (partId != EXPECTED_PART_ID) {
    sensorReady = false;
    resetPpgState("wrong_part");
    Serial.printf("{\"type\":\"sensor_status\",\"sensor\":\"MAX30102\",\"state\":\"wrong_part\",\"part_id\":\"0x%02X\",\"expected\":\"0x15\"}\r\n",
                  partId);
    return false;
  }

  // SparkFun/Maxim reference configuration: 100 conversions/s averaged by 4
  // to approximately 25 FIFO samples/s, Red + IR, 411 us, 4096 nA range.
  ledPower = INITIAL_LED_POWER;
  particleSensor.setup(ledPower, 4, 2, 100, 411, 4096);
  particleSensor.clearFIFO();
  sensorReady = true;
  noFingerSamples = FINGER_LOST_SAMPLES;
  resetPpgState("no_finger");
  Serial.printf("{\"type\":\"sensor_status\",\"sensor\":\"MAX30102\",\"state\":\"ready\",\"address\":\"0x57\",\"part_id\":\"0x%02X\",\"revision_id\":\"0x%02X\",\"i2c_hz\":%lu,\"nominal_fifo_hz\":25,\"led_power\":%u}\r\n",
                partId, revisionId, static_cast<unsigned long>(SENSOR_I2C_HZ), ledPower);
  return true;
}

void filterPpgChannel(const uint32_t *input, float *output) {
  for (size_t i = 0; i < PPG_WINDOW_SIZE; ++i) {
    const size_t first = i > FILTER_HALF_WIDTH ? i - FILTER_HALF_WIDTH : 0;
    const size_t last = min(PPG_WINDOW_SIZE - 1, i + FILTER_HALF_WIDTH);
    double sum = 0.0;
    for (size_t j = first; j <= last; ++j) sum += input[j];
    filterScratch[i] = static_cast<float>(input[i] - sum / (last - first + 1));
  }

  static constexpr float smoothWeights[5] = {1.0f / 9.0f, 2.0f / 9.0f, 3.0f / 9.0f,
                                               2.0f / 9.0f, 1.0f / 9.0f};
  for (size_t i = 0; i < PPG_WINDOW_SIZE; ++i) {
    float filtered = 0.0f;
    for (int offset = -2; offset <= 2; ++offset) {
      int index = static_cast<int>(i) + offset;
      if (index < 0) index = 0;
      if (index >= static_cast<int>(PPG_WINDOW_SIZE)) index = PPG_WINDOW_SIZE - 1;
      filtered += smoothWeights[offset + 2] * filterScratch[index];
    }
    output[i] = filtered;
  }
}

float goertzelPower(const float *samples, float frequencyHz, float sampleHz) {
  const float omega = 2.0f * PI_F * frequencyHz / sampleHz;
  const float coefficient = 2.0f * cosf(omega);
  float q1 = 0.0f;
  float q2 = 0.0f;
  for (size_t i = 0; i < PPG_WINDOW_SIZE; ++i) {
    const float q0 = samples[i] * ppgWindowWeights[i] + coefficient * q1 - q2;
    q2 = q1;
    q1 = q0;
  }
  return q1 * q1 + q2 * q2 - coefficient * q1 * q2;
}

void estimatePpgWindow() {
  uint64_t irSum = 0;
  uint64_t redSum = 0;
  uint32_t irMin = UINT32_MAX;
  uint32_t irMax = 0;
  for (size_t i = 0; i < PPG_WINDOW_SIZE; ++i) {
    irSum += irBuffer[i];
    redSum += redBuffer[i];
    if (irBuffer[i] < irMin) irMin = irBuffer[i];
    if (irBuffer[i] > irMax) irMax = irBuffer[i];
  }
  const float irMean = static_cast<float>(static_cast<double>(irSum) / PPG_WINDOW_SIZE);
  const float redMean = static_cast<float>(static_cast<double>(redSum) / PPG_WINDOW_SIZE);

  filterPpgChannel(irBuffer, irFiltered);
  filterPpgChannel(redBuffer, redFiltered);

  double covariance = 0.0;
  double irEnergy = 0.0;
  double redEnergy = 0.0;
  const size_t metricFirst = FILTER_HALF_WIDTH;
  const size_t metricLast = PPG_WINDOW_SIZE - FILTER_HALF_WIDTH;
  for (size_t i = metricFirst; i < metricLast; ++i) {
    covariance += static_cast<double>(irFiltered[i]) * redFiltered[i];
    irEnergy += static_cast<double>(irFiltered[i]) * irFiltered[i];
    redEnergy += static_cast<double>(redFiltered[i]) * redFiltered[i];
  }
  const double metricCount = metricLast - metricFirst;
  const float irRms = static_cast<float>(sqrt(irEnergy / metricCount));
  const float redRms = static_cast<float>(sqrt(redEnergy / metricCount));
  const double denominator = sqrt(irEnergy * redEnergy);

  windowIrMean = irMean;
  windowAcDc = irMean > 0.0f ? irRms / irMean : 0.0f;
  windowCorrelation = denominator > 0.0 ? static_cast<float>(covariance / denominator) : 0.0f;
  const float redAcDc = redMean > 0.0f ? redRms / redMean : 0.0f;
  windowRatioR = windowAcDc > 0.0f ? redAcDc / windowAcDc : 0.0f;

  const uint32_t durationMs = sampleMsBuffer[PPG_WINDOW_SIZE - 1] - sampleMsBuffer[0];
  windowSampleHz = durationMs > 0
                       ? (PPG_WINDOW_SIZE - 1) * 1000.0f / durationMs
                       : 25.0f;

  static constexpr float minFrequencyHz = 0.67f;
  static constexpr float maxFrequencyHz = 3.00f;
  static constexpr float frequencyStepHz = 0.02f;
  size_t binCount = 0;
  size_t bestIndex = 0;
  float bestPower = -1.0f;
  float totalPower = 0.0f;
  for (float frequency = minFrequencyHz;
       frequency <= maxFrequencyHz + 0.001f && binCount < 128;
       frequency += frequencyStepHz, ++binCount) {
    const float power = goertzelPower(irFiltered, frequency, windowSampleHz);
    spectralPowers[binCount] = power;
    totalPower += power;
    if (power > bestPower) {
      bestPower = power;
      bestIndex = binCount;
    }
  }

  size_t selectedIndex = bestIndex;
  float selectedFrequency = minFrequencyHz + bestIndex * frequencyStepHz;
  if (selectedFrequency > 1.50f) {
    const float halfFrequency = selectedFrequency * 0.5f;
    const int halfIndex = lroundf((halfFrequency - minFrequencyHz) / frequencyStepHz);
    if (halfIndex >= 0 && halfIndex < static_cast<int>(binCount) &&
        spectralPowers[halfIndex] > 0.25f * bestPower) {
      selectedIndex = static_cast<size_t>(halfIndex);
      selectedFrequency = minFrequencyHz + selectedIndex * frequencyStepHz;
    }
  }

  const float selectedPower = spectralPowers[selectedIndex];
  windowSpectralSnr = totalPower > 0.0f ? selectedPower / (totalPower / binCount) : 0.0f;
  const carerover::PulsePeriodEstimate pulse =
      carerover::estimatePulsePeriod(irFiltered, PPG_WINDOW_SIZE, windowSampleHz);
  windowPulseCorrelation = pulse.correlation;
  heartRate = pulse.valid ? pulse.bpm : 0;
  spo2 = lroundf(110.0f - 25.0f * windowRatioR);

  const carerover::PpgWindowQuality quality = {
      windowSampleHz, windowAcDc, windowCorrelation, windowSpectralSnr,
      windowRatioR, heartRate, spo2};
  algorithmHrValid = pulse.valid && carerover::heartRateCandidateValid(quality);
  algorithmSpo2Valid = carerover::spo2CandidateValid(quality);

  sqi = 0;
  if (irMean >= FINGER_IR_THRESHOLD && irMean <= 210000.0) sqi += 20;
  if (irMax < 245000U && irMin > 1000U) sqi += 20;
  if (windowAcDc >= PPG_MIN_AC_DC && windowAcDc <= PPG_MAX_AC_DC) sqi += 20;
  if (windowCorrelation >= SPO2_MIN_RED_IR_CORRELATION) sqi += 20;
  if (windowSpectralSnr >= PPG_MIN_SPECTRAL_SNR) sqi += 20;
}

bool adjustLedPowerIfNeeded() {
  uint8_t newPower = ledPower;
  if (windowIrMean > IR_TARGET_HIGH && ledPower > 8) {
    newPower = ledPower > 12 ? ledPower - 4 : 8;
  } else if (windowIrMean < IR_TARGET_LOW && ledPower < 60) {
    newPower = ledPower <= 56 ? ledPower + 4 : 60;
  }
  if (newPower == ledPower) return false;

  const uint8_t oldPower = ledPower;
  ledPower = newPower;
  particleSensor.setPulseAmplitudeRed(ledPower);
  particleSensor.setPulseAmplitudeIR(ledPower);
  particleSensor.clearFIFO();
  Serial.printf("{\"type\":\"sensor_status\",\"sensor\":\"MAX30102\",\"state\":\"led_adjust\",\"old_power\":%u,\"new_power\":%u,\"ir_mean\":%.0f}\r\n",
                oldPower, ledPower, windowIrMean);
  resetPpgState("acquiring");
  return true;
}

void evaluatePpgWindow() {
  estimatePpgWindow();
  if (adjustLedPowerIfNeeded()) return;

  if (algorithmHrValid) heartRate = heartRateMedian.update(heartRate);
  if (algorithmSpo2Valid) spo2 = spo2Median.update(spo2);
  heartRateFilter.update(algorithmHrValid, heartRate);
  spo2Filter.update(algorithmSpo2Valid, spo2);
  heartRateValid = heartRateFilter.valid();
  spo2Valid = spo2Filter.valid();
  measurementValid = heartRateValid || spo2Valid;
  if(heartRateValid&&!hrHeld())hrFreshMs=wirelessNowMs();
  if(spo2Valid&&!spo2Held())spo2FreshMs=wirelessNowMs();

  if (heartRateValid && spo2Valid) {
    healthState = hrHeld() || spo2Held() ? "holding" : "stable";
  } else if (heartRateValid) {
    healthState = hrHeld() ? "hr_holding" : "hr_stable";
  } else if (spo2Valid) {
    healthState = spo2Held() ? "spo2_holding" : "spo2_stable";
  } else if (heartRateFilter.goodWindows() > 0 || spo2Filter.goodWindows() > 0) {
    healthState = "acquiring";
  } else {
    healthState = "poor_signal";
  }

  memmove(redBuffer, redBuffer + PPG_STEP_SIZE,
          (PPG_WINDOW_SIZE - PPG_STEP_SIZE) * sizeof(redBuffer[0]));
  memmove(irBuffer, irBuffer + PPG_STEP_SIZE,
          (PPG_WINDOW_SIZE - PPG_STEP_SIZE) * sizeof(irBuffer[0]));
  memmove(sampleMsBuffer, sampleMsBuffer + PPG_STEP_SIZE,
          (PPG_WINDOW_SIZE - PPG_STEP_SIZE) * sizeof(sampleMsBuffer[0]));
  ppgSamples = PPG_WINDOW_SIZE - PPG_STEP_SIZE;
}

void acceptPpgSample(uint32_t red, uint32_t ir) {
  lastRed = red;
  lastIr = ir;
  lastSampleMs = millis();
  wirelessSampleMs = wirelessNowMs();
  wirelessPpg(ir);
  ++totalSamples;
  ++samplesSinceReport;
  if (rawStreaming) {
    Serial.printf("{\"type\":\"ppg_raw\",\"ms\":%lu,\"ir\":%lu,\"red\":%lu}\r\n",
                  static_cast<unsigned long>(lastSampleMs),
                  static_cast<unsigned long>(ir), static_cast<unsigned long>(red));
  }

  if(carerover::tuning::balanced){
    if(!fingerPresent){
      fingerEnterSamples=ir>=35000?uint8_t(fingerEnterSamples+1):0;
      if(fingerEnterSamples<3)return;
      fingerPresent=true;fingerEnterSamples=0;resetPpgState("acquiring");
    }
    noFingerSamples=ir<20000?uint8_t(noFingerSamples+1):0;
    if(noFingerSamples>=8){fingerPresent=false;fingerEnterSamples=0;resetPpgState("no_finger");return;}
    if(demoPpg.sample(red,ir,wirelessNowMs())){
      const auto& q=demoPpg.result;windowIrMean=q.irMean;windowSampleHz=q.hz;windowAcDc=q.acDc;
      windowCorrelation=q.corr;windowSpectralSnr=q.snr;windowPulseCorrelation=q.pulseCorr;windowRatioR=q.ratio;
      algorithmHrValid=q.hrCandidate;algorithmSpo2Valid=q.spo2Candidate;heartRate=q.hr;spo2=q.spo2;sqi=uint8_t(q.quality*100);
      heartRateValid=demoPpg.hr.valid();spo2Valid=demoPpg.spo2.valid();measurementValid=heartRateValid||spo2Valid;
      healthState=measurementValid?(hrHeld()||spo2Held()?"holding":"stable"):"acquiring";
      if(heartRateValid)hrFreshMs=wirelessNowMs()-demoPpg.hr.age(wirelessNowMs());
      if(spo2Valid)spo2FreshMs=wirelessNowMs()-demoPpg.spo2.age(wirelessNowMs());
    }
    return;
  }
  if (ir < FINGER_IR_THRESHOLD) {
    if (noFingerSamples < FINGER_LOST_SAMPLES) ++noFingerSamples;
    if (noFingerSamples >= FINGER_LOST_SAMPLES) {
      fingerPresent = false;
      resetPpgState("no_finger");
    }
    return;
  }

  noFingerSamples = 0;
  if (!fingerPresent) {
    fingerPresent = true;
    resetPpgState("acquiring");
  }

  if (ppgSamples < PPG_WINDOW_SIZE) {
    redBuffer[ppgSamples] = red;
    irBuffer[ppgSamples] = ir;
    sampleMsBuffer[ppgSamples] = lastSampleMs;
    ++ppgSamples;
  }
  if (ppgSamples == PPG_WINDOW_SIZE) evaluatePpgWindow();
}

void serviceMax30102() {
  if (!sensorReady) {
    if (millis() - lastSensorAttemptMs >= SENSOR_RETRY_MS) initializeMax30102();
    return;
  }

  particleSensor.check();
  uint8_t handled = 0;
  while (particleSensor.available() > 0 && handled < 8) {
    acceptPpgSample(particleSensor.getFIFORed(), particleSensor.getFIFOIR());
    particleSensor.nextSample();
    ++handled;
  }
}

void printHealthTelemetry() {
  const uint32_t now = millis();
  const uint32_t elapsed = now - lastHealthReportMs;
  if (elapsed > 0) effectiveSampleHz = samplesSinceReport * 1000.0f / elapsed;
  samplesSinceReport = 0;
  lastHealthReportMs = now;
  wirelessReportMs = wirelessNowMs();

  if (sensorReady) {
    const uint8_t overflowCount = particleSensor.readRegister8(MAX30102_ADDRESS, 0x05) & 0x1FU;
    if (overflowCount > 0) {
      fifoOverflowTotal += overflowCount;
      particleSensor.writeRegister8(MAX30102_ADDRESS, 0x05, 0);
    }
  }

  char heartRateText[16];
  char spo2Text[16];
  if (heartRateValid) {
    snprintf(heartRateText, sizeof(heartRateText), "%ld",
             static_cast<long>(currentHr()));
  } else {
    strcpy(heartRateText, "null");
  }
  if (spo2Valid) {
    snprintf(spo2Text, sizeof(spo2Text), "%ld", static_cast<long>(currentSpo2()));
  } else {
    strcpy(spo2Text, "null");
  }

  Serial.printf(
      "{\"type\":\"health\",\"state\":\"%s\",\"finger_present\":%s,\"valid\":%s,"
      "\"hr_valid\":%s,\"spo2_valid\":%s,\"hr_held\":%s,\"spo2_held\":%s,"
      "\"hr_bpm\":%s,\"spo2_pct\":%s,\"sqi\":%u,\"ir\":%lu,\"red\":%lu,"
      "\"sample_hz\":%.1f,\"fifo_overflow\":%lu,\"window_samples\":%u,\"led_power\":%u,"
      "\"ir_mean\":%.0f,\"ac_dc\":%.5f,\"red_ir_corr\":%.3f,\"spectral_snr\":%.2f,"
      "\"pulse_corr\":%.3f,"
      "\"ratio_r\":%.3f,\"window_hz\":%.2f,\"candidate_hr\":%ld,"
      "\"candidate_spo2\":%ld,\"algorithm_hr_valid\":%s,\"algorithm_spo2_valid\":%s,"
      "\"hr_good_windows\":%u,\"spo2_good_windows\":%u,"
      "\"hr_bad_windows\":%u,\"spo2_bad_windows\":%u}\r\n",
      healthState, fingerPresent ? "true" : "false", measurementValid ? "true" : "false",
      heartRateValid ? "true" : "false", spo2Valid ? "true" : "false",
      hrHeld() ? "true" : "false", spo2Held() ? "true" : "false",
      heartRateText, spo2Text, sqi, static_cast<unsigned long>(lastIr),
      static_cast<unsigned long>(lastRed), effectiveSampleHz,
      static_cast<unsigned long>(fifoOverflowTotal), static_cast<unsigned>(ppgSamples), ledPower,
      windowIrMean, windowAcDc, windowCorrelation, windowSpectralSnr,
      windowPulseCorrelation, windowRatioR,
      windowSampleHz, static_cast<long>(heartRate), static_cast<long>(spo2),
      algorithmHrValid ? "true" : "false", algorithmSpo2Valid ? "true" : "false",
      heartRateFilter.goodWindows(), spo2Filter.goodWindows(),
      heartRateFilter.badWindows(), spo2Filter.badWindows());
}

void printBoardInfo() {
  Serial.println("{\"type\":\"boot\",\"board\":\"CareRover-Main\",\"protocol\":3}");
  Serial.printf("{\"type\":\"board_info\",\"arduino_core\":\"%s\",\"cpu_mhz\":%u,"
                "\"flash_bytes\":%lu,\"psram_bytes\":%lu,\"free_psram_bytes\":%lu,"
                "\"cam_rx_gpio\":%d,\"cam_tx_gpio\":%d,\"sensor_sda_gpio\":%d,\"sensor_scl_gpio\":%d,"
                "\"sensor_i2c_hz\":%lu,\"oled_sda_gpio\":%d,\"oled_scl_gpio\":%d,"
                "\"oled_i2c_hz\":%lu,\"oled_address\":\"0x3C\"}\r\n",
                ESP_ARDUINO_VERSION_STR, getCpuFrequencyMhz(),
                static_cast<unsigned long>(ESP.getFlashChipSize()),
                static_cast<unsigned long>(ESP.getPsramSize()),
                static_cast<unsigned long>(ESP.getFreePsram()), CAM_RX_PIN, CAM_TX_PIN,
                SENSOR_SDA_PIN, SENSOR_SCL_PIN,
                static_cast<unsigned long>(SENSOR_I2C_HZ), OLED_SDA_PIN, OLED_SCL_PIN,
                static_cast<unsigned long>(OLED_I2C_HZ));
}

void serviceOled(uint32_t nowMs) {
  OledUiSnapshot snapshot = {};
  snapshot.camConnected = validPackets > 0 && !timeoutReported;
  snapshot.gestureValid = snapshot.camConnected && gestureFilter.accepted();
  snapshot.gestureLabel = snapshot.gestureValid ? gestureFilter.label() : "--";
  snapshot.gestureScoreMilli = snapshot.gestureValid ? gestureFilter.scoreMilli() : 0;
  snapshot.heartRateValid = heartRateValid;
  snapshot.heartRateBpm = currentHr();snapshot.heartRateHeld=hrHeld();
  snapshot.spo2Valid = spo2Valid;
  snapshot.spo2Percent = currentSpo2();snapshot.spo2Held=spo2Held();
  const auto front=wirelessFront();snapshot.frontEnabled=front.enabled;snapshot.frontValid=front.valid;
  snapshot.frontCm=front.distanceCm;snapshot.frontStatus=front.status;snapshot.frontPhase=carerover::phaseName(front.phase);
  snapshot.stopReason=wirelessStopReason();
  oledUi.service(nowMs, snapshot);
}

void setup() {
  Serial.begin(PC_BAUD);
  delay(350);
  for (size_t i = 0; i < PPG_WINDOW_SIZE; ++i) {
    ppgWindowWeights[i] = 0.5f - 0.5f * cosf(2.0f * PI_F * i / (PPG_WINDOW_SIZE - 1));
  }
  CamLink.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);
  Wire.begin(SENSOR_SDA_PIN, SENSOR_SCL_PIN, SENSOR_I2C_HZ);
  OledWire.begin(OLED_SDA_PIN, OLED_SCL_PIN, OLED_I2C_HZ);
  printBoardInfo();
  Serial.println("{\"type\":\"link_status\",\"state\":\"waiting_for_cam\",\"baud\":115200}");
  lastHealthReportMs = millis();
  initializeMax30102();
  const bool oledReady = oledUi.begin(OledWire);
  Serial.printf("{\"type\":\"oled_status\",\"state\":\"%s\",\"address\":\"0x3C\","
                "\"sda_gpio\":%d,\"scl_gpio\":%d,\"width\":128,\"height\":64}\r\n",
                oledReady ? "ready" : "not_found", OLED_SDA_PIN, OLED_SCL_PIN);
  wirelessBegin();
}

void loop() {
  readPcCommands();
  readCamLink();
  serviceMax30102();

  const uint32_t now = millis();
  if(sensorReady&&wirelessSampleMs&&wirelessNowMs()-wirelessSampleMs>=250){
    fingerPresent=false;fingerEnterSamples=0;resetPpgState("sample_timeout");
    sensorReady=false;lastSensorAttemptMs=now;
  }
  if (!timeoutReported && now - lastValidPacketMs > LINK_TIMEOUT_MS) {
    timeoutReported = true;
    gestureFilter.reset();
    Serial.printf("{\"type\":\"link_status\",\"state\":\"timeout\",\"last_valid_ms\":%lu}\r\n",
                  static_cast<unsigned long>(lastValidPacketMs));
  }
  serviceOled(now);
  if (now - lastHealthReportMs >= HEALTH_REPORT_MS) printHealthTelemetry();
  WirelessHealth health = {};
  health.ready = sensorReady; health.finger = fingerPresent;
  health.hrValid = heartRateValid; health.spo2Valid = spo2Valid;
  health.hr = currentHr(); health.spo2 = currentSpo2(); health.sqi = sqi;
  health.hrHeld=hrHeld();health.spo2Held=spo2Held();health.hrFreshMs=hrFreshMs;health.spo2FreshMs=spo2FreshMs;
  health.quality=carerover::tuning::balanced?demoPpg.result.quality:sqi/100.f;
  health.sampleMs = wirelessSampleMs;
  health.reportMs = wirelessReportMs;
  strlcpy(health.state, healthState, sizeof(health.state));
  wirelessHealth(health);
  wirelessDiagnostics();
  delay(1);
}
