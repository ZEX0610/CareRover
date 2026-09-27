#include <Arduino.h>
#include <ESP_I2S.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/i2s_std.h>
#include <esp_random.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>
#include <atomic>
#include <cmath>
#include "audio_frame.h"

#if __has_include("audio_config.local.h")
#include "audio_config.local.h"
#else
#include "audio_config.example.h"
#endif

// ISOLATED TEST SKETCH. It does not include CareRover motion, health or CAM code.
// See README.md before powering the MAX98357. Never flash over the live car
// without a full backup and explicit decision to replace its present firmware.
namespace {
using namespace carerover_audio;
// The existing INMP441 already occupies GPIO1/2. The amp gets its own clocks
// on I2S1; no mic/amp clock signals are tied together.
// GPIO39/42 cannot simultaneously be used for external JTAG debugging.
constexpr int kMicBclk = 1;
constexpr int kMicWs = 2;
constexpr int kMicSd = 16;
constexpr int kAmpBclk = 39;
constexpr int kAmpWs = 42;
constexpr int kAmpDin = 21;
constexpr int kAmpEnable = 38;  // Via 1 kOhm to breakout SD pin; LOW = shutdown.
constexpr size_t kStereoSlots = kSamples * 2;
constexpr size_t kStereoBytes = kStereoSlots * sizeof(int32_t);
constexpr char kWsGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

enum class Mode : uint8_t { Idle, Meter, Loop, Tone, Remote, Record, Replay };
struct AudioFrame { uint8_t bytes[kPacketBytes]; };
constexpr uint32_t kRecordFrameCount = 150; // 3 seconds at 50 frames/s.
I2SClass micI2s(I2S_NUM_0);
I2SClass ampI2s(I2S_NUM_1);
QueueHandle_t uplink = nullptr;
QueueHandle_t downlink = nullptr;
AudioFrame* recording = nullptr; // Allocated in PSRAM once, never on the stack.
std::atomic<Mode> mode{Mode::Idle};
std::atomic<uint32_t> micRms{0}, micFrames{0}, txDrops{0}, rxDrops{0};
std::atomic<uint32_t> playRms{0}, playFrames{0}, i2sTxErrors{0};
std::atomic<uint32_t> recordedFrames{0}, replayIndex{0};
std::atomic<bool> linkUp{false};
uint32_t captureSeq = 0;

bool configured() {
  return AUDIO_WIFI_SSID[0] && AUDIO_RELAY_HOST[0] &&
         AUDIO_DEVICE_TOKEN[0] && AUDIO_RELAY_ROOT_CA[0];
}

void queueNewest(QueueHandle_t q, const AudioFrame& frame, std::atomic<uint32_t>& drops) {
  if (xQueueSend(q, &frame, 0) == pdPASS) return;
  AudioFrame old;
  xQueueReceive(q, &old, 0);
  drops.fetch_add(1);
  xQueueSend(q, &frame, 0);
}

void captureTask(void*) {
  alignas(4) int32_t stereo[kStereoSlots];
  AudioFrame packet{};
  for (;;) {
    size_t n = 0;
    const esp_err_t err = i2s_channel_read(micI2s.rxChan(), stereo, kStereoBytes,
                                           &n, pdMS_TO_TICKS(80));
    if (err != ESP_OK || n != kStereoBytes) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }
    int64_t squareSum = 0;
    put_header(packet.bytes, captureSeq++);
    for (size_t i = 0; i < kSamples; ++i) {
      const int16_t sample = mic32_to_pcm16(stereo[2 * i]); // L/R tied LOW: left slot.
      packet.bytes[kHeaderBytes + 2 * i] = static_cast<uint8_t>(sample);
      packet.bytes[kHeaderBytes + 2 * i + 1] = static_cast<uint8_t>(sample >> 8);
      squareSum += int32_t(sample) * int32_t(sample);
    }
    micRms.store(static_cast<uint32_t>(sqrt(double(squareSum) / kSamples)));
    micFrames.fetch_add(1);
    const Mode m = mode.load();
    if (m == Mode::Loop) queueNewest(downlink, packet, rxDrops);
    else if (m == Mode::Remote && linkUp.load()) queueNewest(uplink, packet, txDrops);
    else if (m == Mode::Record && recording) {
      const uint32_t index = recordedFrames.load();
      if (index < kRecordFrameCount) {
        recording[index] = packet;
        recordedFrames.store(index + 1);
        if (index + 1 == kRecordFrameCount) mode.store(Mode::Idle);
      }
    }
  }
}

void playbackTask(void*) {
  alignas(4) int32_t stereo[kStereoSlots];
  AudioFrame packet{};
  uint32_t tonePhase = 0;
  for (;;) {
    const Mode m = mode.load();
    // The I2S TX DMA clocks this task. Waiting an extra 20 ms for a packet
    // would silently halve playback rate when the queue is momentarily empty.
    bool got = (m == Mode::Loop || m == Mode::Remote) &&
               xQueueReceive(downlink, &packet, 0) == pdPASS;
    if (m == Mode::Replay && recording) {
      const uint32_t index = replayIndex.load();
      if (index < recordedFrames.load()) {
        packet = recording[index];
        replayIndex.store(index + 1);
        got = true;
      } else {
        mode.store(Mode::Idle);
      }
    }
    int64_t outputSquareSum = 0;
    for (size_t i = 0; i < kSamples; ++i) {
      int16_t sample = 0;
      if (m == Mode::Tone) {
        // Quiet 440 Hz functional test; not a speaker-power test.
        sample = static_cast<int16_t>(1800.0 * sin(2.0 * PI * (tonePhase++ % kSampleRate) * 440.0 / kSampleRate));
      } else if (got) {
        const uint8_t* p = packet.bytes + kHeaderBytes + 2 * i;
        sample = static_cast<int16_t>(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
        // The INMP441 loopback samples measured only tens to a few hundred
        // PCM counts on this build. Keep remote playback quiet, but make the
        // isolated loopback audible without permitting full-scale output.
        int32_t scaled = m == Mode::Loop ? int32_t(sample) * 16 :
                         m == Mode::Replay ? int32_t(sample) * 2 : int32_t(sample) / 6;
        if (scaled > 5000) scaled = 5000;
        if (scaled < -5000) scaled = -5000;
        sample = static_cast<int16_t>(scaled);
      }
      stereo[2 * i] = pcm16_to_dac32(sample);
      stereo[2 * i + 1] = stereo[2 * i]; // breakout channel strap may select either slot
      outputSquareSum += int32_t(sample) * int32_t(sample);
    }
    digitalWrite(kAmpEnable, (m == Mode::Loop || m == Mode::Tone || m == Mode::Remote || m == Mode::Replay) ? HIGH : LOW);
    size_t written = 0;
    const esp_err_t err = i2s_channel_write(ampI2s.txChan(), stereo, kStereoBytes,
                                            &written, pdMS_TO_TICKS(80));
    if (err != ESP_OK || written != kStereoBytes) {
      i2sTxErrors.fetch_add(1);
      vTaskDelay(pdMS_TO_TICKS(2));
    } else if (got) {
      playFrames.fetch_add(1);
      playRms.store(static_cast<uint32_t>(sqrt(double(outputSquareSum) / kSamples)));
    }
  }
}

// Minimal WSS client is intentionally confined to this lab sketch. TLS chain,
// HTTP upgrade and bounded RFC6455 frames are checked; no setInsecure().
bool readExact(WiFiClientSecure& client, uint8_t* dst, size_t len) {
  return client.readBytes(dst, len) == len;
}

bool wsSend(WiFiClientSecure& client, uint8_t op, const uint8_t* payload, size_t len) {
  if (!client.connected() || len > kPacketBytes) return false;
  uint8_t h[8] = {static_cast<uint8_t>(0x80 | op), 0, 0, 0, 0, 0, 0, 0};
  size_t hs = 2;
  if (len < 126) h[1] = static_cast<uint8_t>(0x80 | len);
  else { h[1] = 0xFE; h[2] = static_cast<uint8_t>(len >> 8); h[3] = static_cast<uint8_t>(len); hs = 4; }
  uint32_t mask = esp_random();
  for (size_t i = 0; i < 4; ++i) h[hs + i] = static_cast<uint8_t>(mask >> (8 * i));
  uint8_t body[kPacketBytes];
  for (size_t i = 0; i < len; ++i) body[i] = payload[i] ^ h[hs + (i % 4)];
  return client.write(h, hs + 4) == hs + 4 && client.write(body, len) == len;
}

bool wsReceive(WiFiClientSecure& client, AudioFrame& out) {
  uint8_t h[2];
  if (!readExact(client, h, 2)) return false;
  const bool fin = (h[0] & 0x80) != 0;
  const uint8_t op = h[0] & 0x0F;
  if (!fin || (h[1] & 0x80)) return false; // server frames must be unmasked
  size_t len = h[1] & 0x7F;
  if (len == 126) {
    uint8_t ext[2]; if (!readExact(client, ext, 2)) return false;
    len = (size_t(ext[0]) << 8) | ext[1];
  } else if (len == 127) return false;
  if (len > kPacketBytes || (op != 2 && op != 8 && op != 9 && op != 10)) return false;
  if (!readExact(client, out.bytes, len)) return false;
  if (op == 8) return false;
  if (op == 9) return wsSend(client, 10, out.bytes, len);
  if (op == 10) return true;
  if (valid_packet(out.bytes, len)) queueNewest(downlink, out, rxDrops);
  return true;
}

bool wsConnect(WiFiClientSecure& client) {
  client.setCACert(AUDIO_RELAY_ROOT_CA);
  client.setTimeout(250);
  if (!client.connect(AUDIO_RELAY_HOST, AUDIO_RELAY_PORT)) return false;
  uint8_t rawKey[16]; esp_fill_random(rawKey, sizeof(rawKey));
  unsigned char b64[32]{}; size_t b64n = 0;
  if (mbedtls_base64_encode(b64, sizeof(b64), &b64n, rawKey, sizeof(rawKey)) != 0) return false;
  const String key(reinterpret_cast<char*>(b64));
  const String expectedInput = key + kWsGuid;
  uint8_t digest[20];
  if (mbedtls_sha1(reinterpret_cast<const uint8_t*>(expectedInput.c_str()),
                      expectedInput.length(), digest) != 0) return false;
  unsigned char accept64[40]{}; size_t acceptN = 0;
  if (mbedtls_base64_encode(accept64, sizeof(accept64), &acceptN, digest, sizeof(digest)) != 0) return false;
  client.print(String("GET /audio HTTP/1.1\r\nHost: ") + AUDIO_RELAY_HOST +
               "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: " +
               key + "\r\nAuthorization: Bearer " + AUDIO_DEVICE_TOKEN + "\r\n\r\n");
  client.setTimeout(3000);
  String status = client.readStringUntil('\n'); status.trim();
  if (status != "HTTP/1.1 101 Switching Protocols") return false;
  bool accept = false, upgrade = false;
  size_t headerBytes = 0;
  for (int i = 0; i < 24; ++i) {
    String line = client.readStringUntil('\n');
    headerBytes += line.length();
    if (headerBytes > 2048) return false;
    line.trim();
    if (!line.length()) { client.setTimeout(250); return accept && upgrade; }
    if (line.startsWith("Sec-WebSocket-Accept:")) {
      line.remove(0, strlen("Sec-WebSocket-Accept:")); line.trim();
      accept = line == reinterpret_cast<char*>(accept64);
    }
    if (line.equalsIgnoreCase("Upgrade: websocket")) upgrade = true;
  }
  return false;
}

void networkTask(void*) {
  WiFiClientSecure client;
  AudioFrame outgoing{}, incoming{};
  for (;;) {
    if (mode.load() != Mode::Remote || !configured()) {
      linkUp.store(false); client.stop(); WiFi.disconnect(false);
      xQueueReset(uplink); xQueueReset(downlink);
      vTaskDelay(pdMS_TO_TICKS(200)); continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.mode(WIFI_STA); WiFi.begin(AUDIO_WIFI_SSID, AUDIO_WIFI_PASSWORD);
      const uint32_t deadline = millis() + 12000;
      while (WiFi.status() != WL_CONNECTED && mode.load() == Mode::Remote &&
             static_cast<int32_t>(deadline - millis()) > 0) vTaskDelay(pdMS_TO_TICKS(100));
      if (WiFi.status() != WL_CONNECTED) { WiFi.disconnect(false); vTaskDelay(pdMS_TO_TICKS(2000)); continue; }
      configTime(0, 0, "pool.ntp.org");
      const uint32_t timeDeadline = millis() + 10000;
      while (time(nullptr) < 1704067200 && mode.load() == Mode::Remote &&
             static_cast<int32_t>(timeDeadline - millis()) > 0)
        vTaskDelay(pdMS_TO_TICKS(100));
      if (time(nullptr) < 1704067200) { vTaskDelay(pdMS_TO_TICKS(2000)); continue; }
    }
    if (!wsConnect(client)) { client.stop(); vTaskDelay(pdMS_TO_TICKS(2000)); continue; }
    linkUp.store(true);
    uint32_t lastPing = millis();
    while (mode.load() == Mode::Remote && client.connected()) {
      if (client.available() && !wsReceive(client, incoming)) break;
      if (xQueueReceive(uplink, &outgoing, 0) == pdPASS &&
          !wsSend(client, 2, outgoing.bytes, kPacketBytes)) break;
      if (millis() - lastPing > 15000) {
        if (!wsSend(client, 9, nullptr, 0)) break;
        lastPing = millis();
      }
      vTaskDelay(pdMS_TO_TICKS(2));
    }
    linkUp.store(false); client.stop(); xQueueReset(uplink); xQueueReset(downlink);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setMode(const String& command) {
  if (command == "idle") mode.store(Mode::Idle);
  else if (command == "mic") mode.store(Mode::Meter);
  else if (command == "loop") mode.store(Mode::Loop);
  else if (command == "tone") mode.store(Mode::Tone);
  else if (command == "record" && recording) {
    mode.store(Mode::Idle);
    recordedFrames.store(0);
    replayIndex.store(0);
    mode.store(Mode::Record);
  }
  else if (command == "replay" && recording && recordedFrames.load()) {
    replayIndex.store(0);
    mode.store(Mode::Replay);
  }
  else if (command == "remote" && configured()) mode.store(Mode::Remote);
  else if (command == "remote") Serial.println("REMOTE_NOT_CONFIGURED");
  else Serial.println("Commands: idle mic loop tone record replay remote status");
}
} // namespace

void setup() {
  Serial.begin(115200);
  pinMode(kAmpEnable, OUTPUT); digitalWrite(kAmpEnable, LOW);
  uplink = xQueueCreate(3, sizeof(AudioFrame));
  downlink = xQueueCreate(3, sizeof(AudioFrame));
  if (!uplink || !downlink) { Serial.println("QUEUE_ALLOC_FAIL"); return; }
  recording = static_cast<AudioFrame*>(ps_malloc(sizeof(AudioFrame) * kRecordFrameCount));
  if (!recording) Serial.println("RECORD_BUFFER_ALLOC_FAIL");
  micI2s.setPins(kMicBclk, kMicWs, -1, kMicSd);
  ampI2s.setPins(kAmpBclk, kAmpWs, kAmpDin);
  if (!micI2s.begin(I2S_MODE_STD, kSampleRate, I2S_DATA_BIT_WIDTH_32BIT,
                    I2S_SLOT_MODE_STEREO)) { Serial.println("I2S_RX_INIT_FAIL"); return; }
  if (!ampI2s.begin(I2S_MODE_STD, kSampleRate, I2S_DATA_BIT_WIDTH_32BIT,
                    I2S_SLOT_MODE_STEREO)) { Serial.println("I2S_TX_INIT_FAIL"); return; }
  if (xTaskCreatePinnedToCore(captureTask, "audio_rx", 8192, nullptr, 4, nullptr, 1) != pdPASS ||
      xTaskCreatePinnedToCore(playbackTask, "audio_tx", 8192, nullptr, 4, nullptr, 1) != pdPASS ||
      xTaskCreatePinnedToCore(networkTask, "audio_net", 12288, nullptr, 1, nullptr, 0) != pdPASS) {
    Serial.println("TASK_ALLOC_FAIL");
    return;
  }
  Serial.println("AUDIO_LAB_READY 115200: idle mic loop tone record replay remote status");
}

void loop() {
  static uint32_t last = 0;
  static uint32_t frames = 0;
  static uint32_t played = 0;
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n'); command.trim(); command.toLowerCase();
    if (command == "status") last = 0;
    else setMode(command);
  }
  if (millis() - last >= 1000) {
    const uint32_t nowFrames = micFrames.load();
    const uint32_t nowPlayed = playFrames.load();
    Serial.printf("audio mode=%u mic_rms=%lu capture_fps=%lu play_fps=%lu play_rms=%lu i2s_tx_err=%lu recorded=%lu replay_index=%lu link=%u txq=%u rxq=%u tx_drop=%lu rx_drop=%lu heap=%lu\n",
                  unsigned(mode.load()), (unsigned long)micRms.load(),
                  (unsigned long)(nowFrames - frames), (unsigned long)(nowPlayed - played),
                  (unsigned long)playRms.load(), (unsigned long)i2sTxErrors.load(),
                  (unsigned long)recordedFrames.load(), (unsigned long)replayIndex.load(), unsigned(linkUp.load()),
                  unsigned(uxQueueMessagesWaiting(uplink)), unsigned(uxQueueMessagesWaiting(downlink)),
                  (unsigned long)txDrops.load(), (unsigned long)rxDrops.load(),
                  (unsigned long)ESP.getFreeHeap());
    frames = nowFrames; played = nowPlayed; last = millis();
  }
  delay(10);
}
