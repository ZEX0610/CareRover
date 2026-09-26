#pragma once

#include <Arduino.h>
#include <Wire.h>

struct OledUiSnapshot {
  bool heartRateHeld=false,spo2Held=false;
  bool frontEnabled=false,frontValid=false;
  float frontCm=0;
  const char *frontStatus="",*frontPhase="",*stopReason="";
  bool camConnected;
  bool gestureValid;
  const char *gestureLabel;
  uint16_t gestureScoreMilli;
  bool heartRateValid;
  int32_t heartRateBpm;
  bool spo2Valid;
  int32_t spo2Percent;
};

class OledUi {
 public:
  bool begin(TwoWire &wire);
  void service(uint32_t nowMs, const OledUiSnapshot &snapshot);
  bool ready() const { return ready_; }

 private:
  static constexpr uint8_t ADDRESS = 0x3C;
  static constexpr uint16_t WIDTH = 128;
  static constexpr uint16_t HEIGHT = 64;
  static constexpr uint32_t RETRY_MS = 3000;
  static constexpr uint32_t DATA_REFRESH_MS = 250;
  static constexpr uint32_t IDLE_DELAY_MS = 1500;
  static constexpr uint32_t ANIMATION_FRAME_MS = 125;

  bool initializeDisplay();
  bool sendCommand(uint8_t command);
  bool flush();
  void clear();
  void drawPixel(int16_t x, int16_t y, bool on = true);
  void drawHorizontalLine(int16_t y);
  void drawChar(int16_t x, int16_t y, char character, uint8_t scale = 1);
  void drawText(int16_t x, int16_t y, const char *text, uint8_t scale = 1);
  void drawPackedBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                        uint8_t width, uint8_t height);
  void renderData(const OledUiSnapshot &snapshot);
  void renderNoResult();
  void renderAnimation();

  TwoWire *wire_ = nullptr;
  uint8_t framebuffer_[WIDTH * HEIGHT / 8] = {};
  bool ready_ = false;
  bool idleActive_ = false;
  bool lastHadResult_ = false;
  uint8_t animationChoice_ = 0;
  uint8_t animationFrame_ = 0;
  uint32_t lastRetryMs_ = 0;
  uint32_t noResultSinceMs_ = 0;
  uint32_t lastRefreshMs_ = 0;
};
