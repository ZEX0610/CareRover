#include "audio_gateway.h"

#if defined(CAREROVER_AUDIO_GATEWAY)

#include <Arduino.h>
#include <ESP_I2S.h>
#include <driver/i2s_std.h>
#include <atomic>
#include <cmath>
#include <cstring>

namespace {
constexpr size_t Samples = 320;
constexpr size_t PacketBytes = 648;
constexpr size_t StereoBytes = Samples * 2 * sizeof(int32_t);
constexpr uint32_t SampleRate = 16000;
constexpr int MicBclk = 1, MicWs = 2, MicSd = 16;
constexpr int AmpBclk = 39, AmpWs = 42, AmpDin = 21, AmpEnable = 38;
struct AudioFrame { uint8_t bytes[PacketBytes]; };

I2SClass mic(I2S_NUM_0), amp(I2S_NUM_1);
QueueHandle_t uplink = nullptr, downlink = nullptr;
httpd_handle_t httpServer = nullptr;
std::atomic<int> audioFd{-1};
std::atomic<uint32_t> lastDownlinkMs{0}, captured{0}, sent{0}, played{0},
  captureDrops{0}, uplinkDrops{0}, downlinkDrops{0}, writeErrors{0};
std::atomic<bool> publishPending{false};
portMUX_TYPE publishMux = portMUX_INITIALIZER_UNLOCKED;
AudioFrame publishFrame{};
uint32_t sequence = 0;

bool valid(const AudioFrame& packet) {
  const auto* p = packet.bytes;
  return p[0] == 'C' && p[1] == 'R' && p[2] == 1 && p[3] == 1;
}
void header(AudioFrame& packet) {
  auto* p = packet.bytes;
  p[0] = 'C'; p[1] = 'R'; p[2] = 1; p[3] = 1;
  const uint32_t n = sequence++;
  for (int i = 0; i < 4; ++i) p[4 + i] = static_cast<uint8_t>(n >> (8 * i));
}
void newest(QueueHandle_t queue, const AudioFrame& packet, std::atomic<uint32_t>& drops) {
  if (xQueueSend(queue, &packet, 0) == pdPASS) return;
  AudioFrame old;
  xQueueReceive(queue, &old, 0);
  drops.fetch_add(1);
  xQueueSend(queue, &packet, 0);
}
void captureTask(void*) {
  alignas(4) int32_t stereo[Samples * 2];
  AudioFrame packet{};
  for (;;) {
    if (audioFd.load() < 0) { vTaskDelay(pdMS_TO_TICKS(40)); continue; }
    size_t n = 0;
    if (i2s_channel_read(mic.rxChan(), stereo, StereoBytes, &n, pdMS_TO_TICKS(80)) != ESP_OK || n != StereoBytes) {
      captureDrops.fetch_add(1); vTaskDelay(pdMS_TO_TICKS(2)); continue;
    }
    captured.fetch_add(1);
    // Parent push-to-talk downlink wins. This prevents the previously observed
    // acoustic feedback when the speaker and wrist-side microphone are close.
    if (millis() - lastDownlinkMs.load() < 700) continue;
    header(packet);
    for (size_t i = 0; i < Samples; ++i) {
      const int16_t sample = static_cast<int16_t>(stereo[2 * i] >> 16);
      packet.bytes[8 + 2 * i] = static_cast<uint8_t>(sample);
      packet.bytes[9 + 2 * i] = static_cast<uint8_t>(sample >> 8);
    }
    newest(uplink, packet, uplinkDrops);
  }
}
void playbackTask(void*) {
  alignas(4) int32_t stereo[Samples * 2];
  AudioFrame packet{};
  for (;;) {
    if (audioFd.load() < 0) {
      digitalWrite(AmpEnable, LOW);
      vTaskDelay(pdMS_TO_TICKS(40));
      continue;
    }
    const bool got = xQueueReceive(downlink, &packet, 0) == pdPASS;
    for (size_t i = 0; i < Samples; ++i) {
      int16_t sample = 0;
      if (got) {
        const uint8_t* p = packet.bytes + 8 + 2 * i;
        int32_t scaled = static_cast<int16_t>(uint16_t(p[0]) | uint16_t(p[1]) << 8);
        // The 3 W amplifier was harsh at the first live test. Use the quiet
        // remote level proven by the isolated sketch, with additional headroom.
        scaled /= 8;
        if (scaled > 3000) scaled = 3000;
        if (scaled < -3000) scaled = -3000;
        sample = static_cast<int16_t>(scaled);
      }
      stereo[2 * i] = int32_t(sample) * 65536;
      stereo[2 * i + 1] = stereo[2 * i];
    }
    // Keep the amp awake briefly across late packets instead of toggling its
    // shutdown input at packet rate; transmit zeroes during gaps.
    digitalWrite(AmpEnable, millis() - lastDownlinkMs.load() < 400 ? HIGH : LOW);
    size_t written = 0;
    if (i2s_channel_write(amp.txChan(), stereo, StereoBytes, &written, pdMS_TO_TICKS(80)) != ESP_OK || written != StereoBytes)
      writeErrors.fetch_add(1);
    else if (got) played.fetch_add(1);
  }
}
void publish(void*) {
  const int fd = audioFd.load();
  if (fd >= 0 && httpServer && httpd_ws_get_fd_info(httpServer, fd) == HTTPD_WS_CLIENT_WEBSOCKET) {
    AudioFrame packet;
    portENTER_CRITICAL(&publishMux); packet = publishFrame; portEXIT_CRITICAL(&publishMux);
    httpd_ws_frame_t frame{};
    frame.type = HTTPD_WS_TYPE_BINARY;
    frame.payload = packet.bytes;
    frame.len = PacketBytes;
    if (httpd_ws_send_frame_async(httpServer, fd, &frame) == ESP_OK) sent.fetch_add(1);
    else httpd_sess_trigger_close(httpServer, fd);
  }
  publishPending.store(false);
}
void senderTask(void*) {
  AudioFrame packet{};
  uint32_t lastReport = millis();
  for (;;) {
    if (xQueueReceive(uplink, &packet, pdMS_TO_TICKS(100)) == pdPASS && audioFd.load() >= 0) {
      portENTER_CRITICAL(&publishMux); publishFrame = packet; portEXIT_CRITICAL(&publishMux);
      if (!publishPending.exchange(true) && httpd_queue_work(httpServer, publish, nullptr) != ESP_OK) {
        publishPending.store(false); uplinkDrops.fetch_add(1);
      }
    }
    if (audioFd.load() >= 0 && millis() - lastReport >= 5000) {
      lastReport = millis();
      Serial.printf("{\"type\":\"audio_status\",\"capture\":%lu,\"sent\":%lu,\"played\":%lu,\"capture_drops\":%lu,\"uplink_drops\":%lu,\"downlink_drops\":%lu,\"write_errors\":%lu,\"heap\":%lu}\n",
        (unsigned long)captured.load(), (unsigned long)sent.load(), (unsigned long)played.load(),
        (unsigned long)captureDrops.load(), (unsigned long)uplinkDrops.load(),
        (unsigned long)downlinkDrops.load(), (unsigned long)writeErrors.load(), (unsigned long)ESP.getFreeHeap());
    }
  }
}
} // namespace

bool audioGatewayBegin(httpd_handle_t server) {
  httpServer = server;
  pinMode(AmpEnable, OUTPUT); digitalWrite(AmpEnable, LOW);
  uplink = xQueueCreate(3, sizeof(AudioFrame));
  downlink = xQueueCreate(3, sizeof(AudioFrame));
  if (!uplink || !downlink) return false;
  mic.setPins(MicBclk, MicWs, -1, MicSd);
  amp.setPins(AmpBclk, AmpWs, AmpDin);
  if (!mic.begin(I2S_MODE_STD, SampleRate, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO) ||
      !amp.begin(I2S_MODE_STD, SampleRate, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO)) return false;
  return xTaskCreatePinnedToCore(captureTask, "audio_rx", 8192, nullptr, 4, nullptr, 1) == pdPASS &&
         xTaskCreatePinnedToCore(playbackTask, "audio_tx", 8192, nullptr, 4, nullptr, 1) == pdPASS &&
         xTaskCreatePinnedToCore(senderTask, "audio_ws", 4096, nullptr, 1, nullptr, 0) == pdPASS;
}

esp_err_t audioGatewayHandle(httpd_req_t* req) {
  const int fd = httpd_req_to_sockfd(req);
  if (req->method == HTTP_GET) {
    if (audioFd.load() >= 0) {
      httpd_resp_set_status(req, "503 Service Unavailable");
      return httpd_resp_send(req, "Audio client already connected", HTTPD_RESP_USE_STRLEN);
    }
    xQueueReset(uplink); xQueueReset(downlink);
    lastDownlinkMs.store(0);
    audioFd.store(fd);
    return ESP_OK;
  }
  if (fd != audioFd.load()) return ESP_FAIL;
  httpd_ws_frame_t frame{};
  if (httpd_ws_recv_frame(req, &frame, 0) != ESP_OK || frame.type != HTTPD_WS_TYPE_BINARY ||
      !frame.final || frame.len != PacketBytes) return ESP_FAIL;
  AudioFrame packet{};
  frame.payload = packet.bytes;
  if (httpd_ws_recv_frame(req, &frame, PacketBytes) != ESP_OK || !valid(packet)) return ESP_FAIL;
  lastDownlinkMs.store(millis());
  newest(downlink, packet, downlinkDrops);
  return ESP_OK;
}

void audioGatewayClosed(int fd) {
  int expected = fd;
  if (audioFd.compare_exchange_strong(expected, -1)) {
    digitalWrite(AmpEnable, LOW);
    if (uplink) xQueueReset(uplink);
    if (downlink) xQueueReset(downlink);
  }
}

#endif // CAREROVER_AUDIO_GATEWAY
