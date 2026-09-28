#include "oled_ui.h"

#include <esp_system.h>
#include <pgmspace.h>
#include <stdio.h>
#include <string.h>

#include "dog_animation_assets.h"

namespace {

// 5x7 column font: space, A-Z, 0-9, colon, percent, dash, slash and dot.
const uint8_t FONT_5X7[][5] PROGMEM = {
    {0x00, 0x00, 0x00, 0x00, 0x00},
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x41, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x40, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x00, 0x36, 0x36, 0x00, 0x00}, {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x08, 0x08, 0x08, 0x08, 0x08}, {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x00, 0x60, 0x60, 0x00, 0x00},
};

uint8_t glyphIndex(char character) {
  if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
  if (character >= 'A' && character <= 'Z') return 1 + character - 'A';
  if (character >= '0' && character <= '9') return 27 + character - '0';
  if (character == ':') return 37;
  if (character == '%') return 38;
  if (character == '-') return 39;
  if (character == '/') return 40;
  if (character == '.') return 41;
  return 0;
}

}  // namespace

bool OledUi::begin(TwoWire &wire) {
  wire_ = &wire;
  return initializeDisplay();
}

bool OledUi::sendCommand(uint8_t command) {
  wire_->beginTransmission(ADDRESS);
  wire_->write(0x00);
  wire_->write(command);
  return wire_->endTransmission() == 0;
}

bool OledUi::initializeDisplay() {
  if (wire_ == nullptr) return false;
  lastRetryMs_ = millis();
  wire_->beginTransmission(ADDRESS);
  if (wire_->endTransmission() != 0) {
    ready_ = false;
    return false;
  }

  const uint8_t commands[] = {
      0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
      0x8D, 0x14, 0x20, 0x02, 0xA1, 0xC8, 0xDA, 0x12,
      0x81, 0x7F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6,
      0xAF,
  };
  for (const uint8_t command : commands) {
    if (!sendCommand(command)) {
      ready_ = false;
      return false;
    }
  }
  delay(100);
  ready_ = true;
  idleActive_ = false;
  lastHadResult_ = false;
  noResultSinceMs_ = millis();
  clear();
  renderNoResult();
  return true;
}

void OledUi::clear() { memset(framebuffer_, 0, sizeof(framebuffer_)); }

void OledUi::drawPixel(int16_t x, int16_t y, bool on) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
  const uint16_t index = static_cast<uint16_t>(x) + (static_cast<uint16_t>(y) / 8U) * WIDTH;
  const uint8_t mask = 1U << (y & 7);
  if (on) framebuffer_[index] |= mask;
  else framebuffer_[index] &= static_cast<uint8_t>(~mask);
}

void OledUi::drawHorizontalLine(int16_t y) {
  for (int16_t x = 0; x < WIDTH; ++x) drawPixel(x, y);
}

void OledUi::drawChar(int16_t x, int16_t y, char character, uint8_t scale) {
  const uint8_t index = glyphIndex(character);
  for (uint8_t column = 0; column < 5; ++column) {
    const uint8_t bits = pgm_read_byte(&FONT_5X7[index][column]);
    for (uint8_t row = 0; row < 7; ++row) {
      if ((bits & (1U << row)) == 0) continue;
      for (uint8_t dx = 0; dx < scale; ++dx) {
        for (uint8_t dy = 0; dy < scale; ++dy) {
          drawPixel(x + column * scale + dx, y + row * scale + dy);
        }
      }
    }
  }
}

void OledUi::drawText(int16_t x, int16_t y, const char *text, uint8_t scale) {
  if (text == nullptr) return;
  while (*text != '\0') {
    drawChar(x, y, *text++, scale);
    x += 6 * scale;
    if (x >= WIDTH) break;
  }
}

void OledUi::drawPackedBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                              uint8_t width, uint8_t height) {
  const uint8_t rowBytes = width / 8;
  for (uint8_t row = 0; row < height; ++row) {
    for (uint8_t byteIndex = 0; byteIndex < rowBytes; ++byteIndex) {
      const uint8_t value = pgm_read_byte(bitmap + row * rowBytes + byteIndex);
      for (uint8_t bit = 0; bit < 8; ++bit) {
        if (value & (0x80U >> bit)) drawPixel(x + byteIndex * 8 + bit, y + row);
      }
    }
  }
}

bool OledUi::flush() {
  if (!ready_) return false;
  for (uint8_t page = 0; page < 8; ++page) {
    if (!sendCommand(0xB0 + page) || !sendCommand(0x00) || !sendCommand(0x10)) {
      ready_ = false;
      return false;
    }
    for (uint16_t column = 0; column < WIDTH; column += 16) {
      wire_->beginTransmission(ADDRESS);
      wire_->write(0x40);
      for (uint8_t offset = 0; offset < 16; ++offset) {
        wire_->write(framebuffer_[page * WIDTH + column + offset]);
      }
      if (wire_->endTransmission() != 0) {
        ready_ = false;
        return false;
      }
    }
  }
  return true;
}

void OledUi::renderData(const OledUiSnapshot &snapshot) {
  clear();
  drawText(0, 0, "CAM:");
  drawText(24, 0, snapshot.gestureValid ? snapshot.gestureLabel : "--");
  if (snapshot.gestureValid) {
    char score[8];
    snprintf(score, sizeof(score), "%u%%", (snapshot.gestureScoreMilli + 5U) / 10U);
    drawText(128 - static_cast<int16_t>(strlen(score) * 6), 0, score);
  }
  drawHorizontalLine(9);

  char value[8];
  drawText(0, 15, "HR");
  if (snapshot.heartRateValid) snprintf(value, sizeof(value), "%ld", static_cast<long>(snapshot.heartRateBpm));
  else strcpy(value, "--");
  drawText(18, 12, value, 2);
  drawText(58, 18, "BPM");
  drawText(90, 18, snapshot.wristHealthSource ? "W" : "C");

  drawText(0, 37, "O2");
  if (snapshot.spo2Valid) snprintf(value, sizeof(value), "%ld", static_cast<long>(snapshot.spo2Percent));
  else strcpy(value, "--");
  drawText(18, 34, value, 2);
  drawText(58, 40, "%");
  drawText(90, 40, snapshot.wristHealthSource ? "W" : "C");

  if(snapshot.heartRateValid&&snapshot.heartRateHeld)drawText(108,18,"H");
  if(snapshot.spo2Valid&&snapshot.spo2Held)drawText(108,40,"H");
  // Front distance on the same page.
  drawHorizontalLine(50);
  if(snapshot.frontEnabled) {
    char range[24];
    if(snapshot.frontValid)snprintf(range,sizeof(range),"FRONT %.0f CM",snapshot.frontCm);
    else strcpy(range,"FRONT --");
    drawText(0,53,range);
  } else {
    drawText(0,53,"FRONT OFF");
  }
  flush();
}

void OledUi::renderNoResult() {
  clear();
  drawText(10, 25, "NO RESULT", 2);
  flush();
}

void OledUi::renderAnimation() {
  clear();
  const uint8_t *frame = animationChoice_ == 0
                             ? &DOG_ANIMATION_A[animationFrame_][0]
                             : &DOG_ANIMATION_B[animationFrame_][0];
  drawPackedBitmap((WIDTH - DOG_FRAME_WIDTH) / 2, (HEIGHT - DOG_FRAME_HEIGHT) / 2,
                   frame, DOG_FRAME_WIDTH, DOG_FRAME_HEIGHT);
  flush();
}

void OledUi::service(uint32_t nowMs, const OledUiSnapshot &snapshot) {
  if (!ready_) {
    if (wire_ != nullptr && nowMs - lastRetryMs_ >= RETRY_MS) initializeDisplay();
    return;
  }

  const bool hasResult = snapshot.gestureValid || snapshot.heartRateValid ||
                         snapshot.spo2Valid || snapshot.wristHealthSource;
  if (hasResult) {
    noResultSinceMs_ = 0;
    idleActive_ = false;
    if (!lastHadResult_ || nowMs - lastRefreshMs_ >= DATA_REFRESH_MS) {
      renderData(snapshot);
      lastRefreshMs_ = nowMs;
    }
  } else {
    if (lastHadResult_ || noResultSinceMs_ == 0) {
      noResultSinceMs_ = nowMs;
      idleActive_ = false;
      renderNoResult();
      lastRefreshMs_ = nowMs;
    }
    if (nowMs - noResultSinceMs_ >= IDLE_DELAY_MS) {
      if (!idleActive_) {
        idleActive_ = true;
        animationChoice_ = static_cast<uint8_t>(esp_random() & 1U);
        animationFrame_ = 0;
        lastRefreshMs_ = 0;
      }
      if (lastRefreshMs_ == 0 || nowMs - lastRefreshMs_ >= ANIMATION_FRAME_MS) {
        renderAnimation();
        animationFrame_ = (animationFrame_ + 1U) % DOG_FRAME_COUNT;
        lastRefreshMs_ = nowMs;
      }
    }
  }
  lastHadResult_ = hasResult;
}
