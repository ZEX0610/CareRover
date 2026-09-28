#include "watch_link.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <cJSON.h>
#include <cmath>
#include <cstring>

namespace {
constexpr uint16_t kPort = 45670;
constexpr size_t kPacketCapacity = 384;
portMUX_TYPE watchMux = portMUX_INITIALIZER_UNLOCKED;
carerover::WatchLinkState watchState;

const cJSON* field(const cJSON* root, const char* name) {
  return cJSON_GetObjectItemCaseSensitive(root, name);
}

bool unsignedField(const cJSON* root, const char* name, uint32_t& out) {
  const cJSON* item = field(root, name);
  if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
      item->valuedouble < 0 || item->valuedouble > UINT32_MAX ||
      std::floor(item->valuedouble) != item->valuedouble) return false;
  out = static_cast<uint32_t>(item->valuedouble);
  return true;
}

bool flagField(const cJSON* root, const char* name, bool& out) {
  uint32_t value = 0;
  if (!unsignedField(root, name, value) || value > 1) return false;
  out = value != 0;
  return true;
}

bool floatField(const cJSON* root, const char* name, float& out) {
  const cJSON* item = field(root, name);
  if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble)) return false;
  out = static_cast<float>(item->valuedouble);
  return std::isfinite(out);
}

bool uniqueFields(const cJSON* root) {
  for (const cJSON* a = root->child; a; a = a->next)
    for (const cJSON* b = a->next; b; b = b->next)
      if (a->string && b->string && strcmp(a->string, b->string) == 0) return false;
  return true;
}

bool parseWatch(const char* text, carerover::WatchLinkPacket& out) {
  cJSON* root = cJSON_Parse(text);
  if (!root) return false;
  bool ok = false;
  do {
    if (!cJSON_IsObject(root) || cJSON_GetArraySize(root) > 20 || !uniqueFields(root)) break;
    const cJSON* type = field(root, "type");
    if (!cJSON_IsString(type) || strcmp(type->valuestring, "watch_v1") != 0) break;
    uint32_t hr = 0, spo2 = 0, sqi = 0;
    if (!unsignedField(root, "boot", out.boot) ||
        !unsignedField(root, "seq", out.seq) ||
        !unsignedField(root, "ms", out.watchMs) ||
        !flagField(root, "cal", out.calibrated) ||
        !flagField(root, "roll", out.rolling) ||
        !floatField(root, "vx", out.vx) ||
        !floatField(root, "vy", out.vy) ||
        !floatField(root, "wz", out.wz) ||
        !flagField(root, "contact", out.contact) ||
        !unsignedField(root, "hr", hr) ||
        !flagField(root, "hr_valid", out.hrValid) ||
        !flagField(root, "hr_held", out.hrHeld) ||
        !unsignedField(root, "hr_age", out.hrAgeMs) ||
        !unsignedField(root, "spo2", spo2) ||
        !flagField(root, "spo2_valid", out.spo2Valid) ||
        !flagField(root, "spo2_held", out.spo2Held) ||
        !unsignedField(root, "spo2_age", out.spo2AgeMs) ||
        !unsignedField(root, "sqi", sqi)) break;
    if (hr > 240 || spo2 > 100 || sqi > 100) break;
    out.hr = hr; out.spo2 = spo2; out.sqi = sqi;
    ok = carerover::validWatchPacket(out);
  } while (false);
  cJSON_Delete(root);
  return ok;
}

void watchTask(void*) {
  WiFiUDP udp;
  bool listening = false;
  uint32_t lastReport = 0;
  for (;;) {
    if (!listening && WiFi.softAPIP()[0] == 192) listening = udp.begin(kPort);
    if (listening) {
      const int length = udp.parsePacket();
      if (length > 0) {
        char text[kPacketCapacity];
        const int n = udp.read(text, sizeof(text) - 1);
        udp.flush();
        const IPAddress ip = udp.remoteIP();
        if (n > 0 && length < static_cast<int>(sizeof(text)) &&
            ip[0] == 192 && ip[1] == 168 && ip[2] == 4 && ip[3] > 1) {
          text[n] = '\0';
          carerover::WatchLinkPacket packet;
          if (parseWatch(text, packet)) {
            portENTER_CRITICAL(&watchMux);
            watchState.accept(packet, millis());
            portEXIT_CRITICAL(&watchMux);
          }
        }
      }
    }
    const uint32_t now = millis();
    if (now - lastReport >= 1000) {
      lastReport = now;
      const auto state = watchLinkSnapshot();
      const bool online = state.online(now);
      const auto& p = state.packet;
      Serial.printf("{\"type\":\"watch_status\",\"online\":%s,\"seq\":%lu,\"age_ms\":%lu,\"cal\":%d,\"vx\":%.3f,\"vy\":%.3f,\"wz\":%.3f,\"hr_valid\":%d,\"hr\":%d,\"spo2_valid\":%d,\"spo2\":%d}\n",
                    online ? "true" : "false", static_cast<unsigned long>(p.seq),
                    static_cast<unsigned long>(state.seen ? now - state.receivedMs : 0),
                    online && p.calibrated, online ? p.vx : 0, online ? p.vy : 0,
                    online ? p.wz : 0, online && p.hrValid, online && p.hrValid ? p.hr : 0,
                    online && p.spo2Valid, online && p.spo2Valid ? p.spo2 : 0);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
}  // namespace

bool watchLinkBegin() {
  return xTaskCreate(watchTask, "watch_rx", 5120, nullptr, 1, nullptr) == pdPASS;
}

carerover::WatchLinkSnapshot watchLinkSnapshot() {
  portENTER_CRITICAL(&watchMux);
  const auto snapshot = watchState.snapshot();
  portEXIT_CRITICAL(&watchMux);
  return snapshot;
}
