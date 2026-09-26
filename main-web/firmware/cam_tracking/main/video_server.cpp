#include "video_server.h"
#include "latest_frame.h"
#include "video_work_queue.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "img_converters.h"
#include <atomic>
#include <cstdio>
#include <cstring>

namespace {
constexpr size_t kRawCapacity = 320 * 240 * 2;
LatestFrame frames;
VideoWorkQueue work;
SemaphoreHandle_t jpegMutex = nullptr, rawMutex = nullptr;
TaskHandle_t encoderHandle = nullptr;
BaseType_t backgroundCore = 1;
uint8_t* jpegBuffers[3] = {};
uint8_t* rawBuffers[2] = {};
camera_fb_t rawMeta[2] = {};
std::atomic<bool> viewer{false};
std::atomic<uint32_t> encoded{0}, dropped{0}, encodeMs{0}, sent{0};
std::atomic<uint32_t> captureFailures{0};

// Camera frames for the live view must not be paced by gesture classification.
// esp_camera_fb_get() takes a buffer from the camera driver's frame queue;
// the AI task uses the other buffer until it returns it. Only this task feeds
// the JPEG work queue, which already discards stale pending frames.
void captureTask(void*) {
  for (;;) {
    // Headless tracking should not pay for JPEG encoding. The HTTP handler
    // sets viewer before starting its sender task; start within 50 ms.
    if (!viewer.load()) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    camera_fb_t* frame = esp_camera_fb_get();
    if (frame) {
      videoPublish(frame);
      esp_camera_fb_return(frame);
    } else {
      ++captureFailures;
    }
    // About six live frames/s leaves PSRAM bandwidth for hand recognition
    // and face tracking instead of filling an unseen queue at nine frames/s.
    vTaskDelay(pdMS_TO_TICKS(150));
  }
}

void encoderTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (;;) {
      xSemaphoreTake(rawMutex, portMAX_DELAY);
      const int rawSlot = work.beginRead();
      xSemaphoreGive(rawMutex);
      if (rawSlot < 0) break;

      uint8_t* jpeg = nullptr;
      size_t size = 0;
      const int64_t began = esp_timer_get_time();
      const bool converted = frame2jpg(&rawMeta[rawSlot], 60, &jpeg, &size);
      encodeMs.store(uint32_t((esp_timer_get_time() - began) / 1000));
      xSemaphoreTake(rawMutex, portMAX_DELAY);
      work.endRead(rawSlot);
      xSemaphoreGive(rawMutex);

      if (!converted || !jpeg || size < 4 || size > LatestFrame::Capacity ||
          jpeg[0] != 0xff || jpeg[1] != 0xd8 ||
          jpeg[size - 2] != 0xff || jpeg[size - 1] != 0xd9) {
        free(jpeg);
        ++dropped;
        continue;
      }
      xSemaphoreTake(jpegMutex, portMAX_DELAY);
      const int jpegSlot = frames.writable();
      xSemaphoreGive(jpegMutex);
      if (jpegSlot < 0) {
        free(jpeg);
        ++dropped;
        continue;
      }
      // A single encoder owns this writable slot. The stream may still hold
      // another slot, but never this one.
      memcpy(jpegBuffers[jpegSlot], jpeg, size);
      free(jpeg);
      xSemaphoreTake(jpegMutex, portMAX_DELAY);
      const bool ok = frames.publish(jpegSlot, size);
      xSemaphoreGive(jpegMutex);
      if (ok) ++encoded;
      else ++dropped;
    }
  }
}

void streamTask(void* arg) {
  auto* req = static_cast<httpd_req_t*>(arg);
  xSemaphoreTake(jpegMutex, portMAX_DELAY);
  uint32_t last = frames.sequence();
  xSemaphoreGive(jpegMutex);
  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=carerover");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "http://192.168.4.1");
  int64_t lastFrame = esp_timer_get_time();
  for (;;) {
    xSemaphoreTake(jpegMutex, portMAX_DELAY);
    const int slot = frames.acquire(last);
    const size_t length = frames.length(slot);
    if (slot >= 0) last = frames.sequence();
    xSemaphoreGive(jpegMutex);
    if (slot < 0) {
      if (esp_timer_get_time() - lastFrame > 2000000) break;
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    lastFrame = esp_timer_get_time();
    char header[128];
    const int n = snprintf(header, sizeof(header),
      "--carerover\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
      unsigned(length));
    const bool ok = httpd_resp_send_chunk(req, header, n) == ESP_OK &&
      httpd_resp_send_chunk(req, reinterpret_cast<char*>(jpegBuffers[slot]), length) == ESP_OK &&
      httpd_resp_send_chunk(req, "\r\n", 2) == ESP_OK;
    xSemaphoreTake(jpegMutex, portMAX_DELAY);
    frames.release(slot);
    xSemaphoreGive(jpegMutex);
    if (!ok) break;
    ++sent;
  }
  httpd_resp_send_chunk(req, nullptr, 0);
  httpd_req_async_handler_complete(req);
  viewer.store(false);
  vTaskDelete(nullptr);
}

esp_err_t stream(httpd_req_t* req) {
  if (viewer.exchange(true)) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_send(req, "Video viewer busy", HTTPD_RESP_USE_STRLEN);
  }
  httpd_req_t* async = nullptr;
  if (httpd_req_async_handler_begin(req, &async) != ESP_OK) {
    viewer.store(false);
    return ESP_FAIL;
  }
  if (xTaskCreatePinnedToCore(streamTask, "mjpeg", 4096, async, 2,
                              nullptr, backgroundCore) != pdPASS) {
    httpd_resp_set_status(async, "503 Service Unavailable");
    httpd_resp_send(async, "No stream task", HTTPD_RESP_USE_STRLEN);
    httpd_req_async_handler_complete(async);
    viewer.store(false);
  }
  return ESP_OK;
}
}  // namespace

bool videoBegin() {
  jpegMutex = xSemaphoreCreateMutex();
  rawMutex = xSemaphoreCreateMutex();
  if (!jpegMutex || !rawMutex) return false;
  for (auto& buffer : jpegBuffers) {
    buffer = static_cast<uint8_t*>(heap_caps_malloc(
      LatestFrame::Capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer) return false;
  }
  for (auto& buffer : rawBuffers) {
    buffer = static_cast<uint8_t*>(heap_caps_malloc(
      kRawCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer) return false;
  }
  backgroundCore = xPortGetCoreID() == 0 ? 1 : 0;
  if (xTaskCreatePinnedToCore(encoderTask, "jpeg_encoder", 8192, nullptr,
                              1, &encoderHandle, backgroundCore) != pdPASS) return false;
  if (xTaskCreatePinnedToCore(captureTask, "video_capture", 4096, nullptr,
                              1, nullptr, backgroundCore) != pdPASS) return false;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_open_sockets = 3;
  config.recv_wait_timeout = 1;
  config.send_wait_timeout = 1;
  config.lru_purge_enable = true;
  httpd_handle_t server = nullptr;
  if (httpd_start(&server, &config) != ESP_OK) return false;
  httpd_uri_t uri = {};
  uri.uri = "/stream";
  uri.method = HTTP_GET;
  uri.handler = stream;
  return httpd_register_uri_handler(server, &uri) == ESP_OK;
}

bool videoPublish(camera_fb_t* frame) {
  if (!rawMutex || !encoderHandle || !frame || !frame->buf ||
      frame->format != PIXFORMAT_RGB565 || frame->len > kRawCapacity) return false;
  xSemaphoreTake(rawMutex, portMAX_DELAY);
  const unsigned replacedBefore = work.replaced();
  const int slot = work.beginWrite();
  xSemaphoreGive(rawMutex);
  if (slot < 0) { ++dropped; return false; }
  memcpy(rawBuffers[slot], frame->buf, frame->len);
  rawMeta[slot] = *frame;
  rawMeta[slot].buf = rawBuffers[slot];
  xSemaphoreTake(rawMutex, portMAX_DELAY);
  const bool ok = work.finishWrite(slot);
  dropped.fetch_add(work.replaced() - replacedBefore);
  xSemaphoreGive(rawMutex);
  if (ok) xTaskNotifyGive(encoderHandle);
  else ++dropped;
  return ok;
}

uint32_t videoEncoded() { return encoded.load(); }
uint32_t videoDropped() { return dropped.load(); }
uint32_t videoLastEncodeMs() { return encodeMs.load(); }
uint32_t videoSent() { return sent.load(); }
uint32_t videoCaptureFailures() { return captureFailures.load(); }
