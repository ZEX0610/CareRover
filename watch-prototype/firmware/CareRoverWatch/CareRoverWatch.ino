#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <MAX30105.h>
#include <cstring>

#include "watch_motion.h"
#include "watch_health.h"
#include "watch_protocol.h"
#if __has_include("watch_secrets.h")
#include "watch_secrets.h"
#else
#define WATCH_WIFI_SSID ""
#define WATCH_WIFI_PASSWORD ""
#endif

using namespace carerover_watch;

// Only wire after measuring the MAX module's SDA/SCL idle voltage <= 3.3 V.
constexpr int kSda = 4, kScl = 5, kBootButton = 0;
constexpr uint8_t kMpu = 0x68;
constexpr uint16_t kUdpPort = 45670;
constexpr uint32_t kMotionPeriodMs = 20, kPacketPeriodMs = 50;
IPAddress carIp(192, 168, 4, 1);
MAX30105 ppg;
WiFiUDP udp;
WristMotion motion;
WristHealth health;
bool mpuReady = false, ppgReady = false;
uint32_t bootId = 0, seq = 0, lastMotionMs = 0, lastPacketMs = 0;
uint32_t lastWifiAttemptMs = 0, nextPpgMs = 0;
bool buttonWasDown = false;

bool writeMpu(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kMpu);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readMpu(float &ax, float &ay, float &az,
             float &gx, float &gy, float &gz) {
  Wire.beginTransmission(kMpu);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(kMpu, uint8_t(14), uint8_t(true)) != 14) return false;
  int16_t raw[7];
  for (int i = 0; i < 7; ++i) {
    const uint16_t hi = Wire.read();
    raw[i] = int16_t((hi << 8) | uint16_t(Wire.read()));
  }
  ax = raw[0] / 16384.0f; ay = raw[1] / 16384.0f; az = raw[2] / 16384.0f;
  gx = raw[4] / 65.5f; gy = raw[5] / 65.5f; gz = raw[6] / 65.5f;
  return true;
}

bool startMpu() {
  Wire.beginTransmission(kMpu);
  Wire.write(0x75);  // WHO_AM_I
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom(kMpu, uint8_t(1), uint8_t(true)) != 1) return false;
  const uint8_t id = Wire.read();
  if (id != 0x68 && id != 0x70) return false;
  return writeMpu(0x6B, 0x01) &&  // Wake, PLL X gyro.
         writeMpu(0x1A, 0x03) &&  // ~44 Hz low-pass.
         writeMpu(0x1B, 0x08) &&  // Gyro +/-500 deg/s.
         writeMpu(0x1C, 0x00);    // Accel +/-2 g.
}

void startPpg() {
  ppgReady = ppg.begin(Wire, I2C_SPEED_FAST);
  if (!ppgReady) { Serial.println("PPG missing"); return; }
  ppg.setup(30, 4, 2, 100, 411, 4096); // 100 Hz / 4 FIFO average = 25 Hz.
  ppg.clearFIFO();
  nextPpgMs = millis();
  Serial.println("PPG ready");
}

void serviceWifi(uint32_t now) {
  if (!WATCH_WIFI_SSID[0]) return;
  if (WiFi.status() == WL_CONNECTED) return;
  if (lastWifiAttemptMs && now - lastWifiAttemptMs < 8000) return;
  lastWifiAttemptMs = now;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WATCH_WIFI_SSID, WATCH_WIFI_PASSWORD);
}

void servicePpg(uint32_t now) {
  if (!ppgReady) return;
  ppg.check();
  int consumed = 0;
  while (ppg.available() && consumed++ < 8) {
    const uint32_t red = ppg.getFIFORed(), ir = ppg.getFIFOIR();
    ppg.nextSample();
    // FIFO averages four 100-Hz ADC samples. Preserve that cadence even if
    // several FIFO entries are drained in one loop iteration.
    if (now - nextPpgMs > 250) nextPpgMs = now;
    health.sample(red, ir, nextPpgMs);
    nextPpgMs += 40;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(kBootButton, INPUT_PULLUP);
  bootId = esp_random();
  Wire.begin(kSda, kScl, 400000);
  Wire.setTimeOut(20);
  mpuReady = startMpu();
  Serial.printf("watch_v1 boot=%lu MPU=%d\n", (unsigned long)bootId, mpuReady);
  startPpg();
  serviceWifi(millis());
}

void loop() {
  const uint32_t now = millis();
  const bool buttonDown = digitalRead(kBootButton) == LOW;
  if (buttonWasDown && !buttonDown) { motion.recenter(); Serial.println("RECENTER"); }
  buttonWasDown = buttonDown;
  if (now - lastMotionMs >= kMotionPeriodMs) {
    lastMotionMs = now;
    float ax, ay, az, gx, gy, gz;
    if (mpuReady && readMpu(ax, ay, az, gx, gy, gz))
      motion.update(ax, ay, az, gx, gy, gz, now);
  }
  servicePpg(now);
  serviceWifi(now);
  if (now - lastPacketMs >= kPacketPeriodMs) {
    lastPacketMs = now;
    char packet[320];
    if (encodeWatchPacket(packet, sizeof(packet), bootId, seq++, now,
                          motion.command(now), health.reading(now))) {
      Serial.println(packet);
      if (WiFi.status() == WL_CONNECTED && udp.beginPacket(carIp, kUdpPort)) {
        udp.write(reinterpret_cast<const uint8_t*>(packet), strlen(packet));
        udp.endPacket();
      }
    }
  }
  delay(1);
}
