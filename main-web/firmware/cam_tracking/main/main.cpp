#include "sdkconfig.h"
#include "demo_tuning.h"
#include "box_track.h"
#include <algorithm>
#include <list>
#include <cstdio>
#include <cstring>
#include <atomic>
#include "driver/uart.h"
#include "esp_camera.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "hand_detect.hpp"
#include "hand_gesture_recognition.hpp"
#include "human_face_detect.hpp"
#include "vision_protocol.h"
#include "video_server.h"
namespace {
constexpr char TAG[]="carerover_cam";
using namespace carerover;
constexpr int CAM_PWDN = -1;
constexpr int CAM_RESET = -1;
constexpr int CAM_XCLK = 15;
constexpr int CAM_SIOD = 4;
constexpr int CAM_SIOC = 5;
constexpr int CAM_D0 = 11;
constexpr int CAM_D1 = 9;
constexpr int CAM_D2 = 8;
constexpr int CAM_D3 = 10;
constexpr int CAM_D4 = 12;
constexpr int CAM_D5 = 18;
constexpr int CAM_D6 = 17;
constexpr int CAM_D7 = 16;
constexpr int CAM_VSYNC = 6;
constexpr int CAM_HREF = 7;
constexpr int CAM_PCLK = 13;

constexpr uart_port_t MAIN_UART = UART_NUM_1;
constexpr int MAIN_UART_TX = 47;
constexpr int MAIN_UART_RX = 48;
constexpr int MAIN_UART_BAUD = 115200;
esp_err_t init_link_uart()
{
    const uart_config_t uart_config = {
        .baud_rate = MAIN_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {},
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(MAIN_UART, 512, 512, 0, nullptr, 0), TAG, "uart install");
    ESP_RETURN_ON_ERROR(uart_param_config(MAIN_UART, &uart_config), TAG, "uart config");
    ESP_RETURN_ON_ERROR(
        uart_set_pin(MAIN_UART, MAIN_UART_TX, MAIN_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "uart pins");
    return ESP_OK;
}

esp_err_t init_camera()
{
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAM_D0;
    config.pin_d1 = CAM_D1;
    config.pin_d2 = CAM_D2;
    config.pin_d3 = CAM_D3;
    config.pin_d4 = CAM_D4;
    config.pin_d5 = CAM_D5;
    config.pin_d6 = CAM_D6;
    config.pin_d7 = CAM_D7;
    config.pin_xclk = CAM_XCLK;
    config.pin_pclk = CAM_PCLK;
    config.pin_vsync = CAM_VSYNC;
    config.pin_href = CAM_HREF;
    config.pin_sccb_sda = CAM_SIOD;
    config.pin_sccb_scl = CAM_SIOC;
    config.pin_pwdn = CAM_PWDN;
    config.pin_reset = CAM_RESET;
    config.xclk_freq_hz = 15000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 16;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    ESP_RETURN_ON_ERROR(esp_camera_init(&config), TAG, "camera init");
    sensor_t *sensor = esp_camera_sensor_get();
    ESP_LOGI(TAG,
             "camera PID=0x%04x VER=0x%02x MID=%02x:%02x, RGB565 320x240",
             sensor->id.PID,
             sensor->id.VER,
             sensor->id.MIDH,
             sensor->id.MIDL);
    return ESP_OK;
}


uint32_t sequence=0;
std::atomic<bool> wifiConnected{false};
void wifiEvent(void*,esp_event_base_t base,int32_t id,void*) {
  if(base==WIFI_EVENT && (id==WIFI_EVENT_STA_START||id==WIFI_EVENT_STA_DISCONNECTED)) {wifiConnected.store(false);esp_wifi_connect();}
  if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP)wifiConnected.store(true);
}
void wifiBegin() {
  ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
  auto* net=esp_netif_create_default_wifi_sta();
  ESP_ERROR_CHECK(esp_netif_dhcpc_stop(net));
  esp_netif_ip_info_t ip{};ip.ip.addr=ESP_IP4TOADDR(192,168,4,2);ip.gw.addr=ESP_IP4TOADDR(192,168,4,1);ip.netmask.addr=ESP_IP4TOADDR(255,255,255,0);
  ESP_ERROR_CHECK(esp_netif_set_ip_info(net,&ip));
  wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&init));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifiEvent,nullptr));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifiEvent,nullptr));
  wifi_config_t config{};strlcpy(reinterpret_cast<char*>(config.sta.ssid),CONFIG_CAREROVER_WIFI_SSID,sizeof(config.sta.ssid));
  strlcpy(reinterpret_cast<char*>(config.sta.password),CONFIG_CAREROVER_WIFI_PASSWORD,sizeof(config.sta.password));
  config.sta.threshold.authmode=WIFI_AUTH_WPA2_PSK;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&config));ESP_ERROR_CHECK(esp_wifi_start());
}
VisionPacket box(const dl::detect::result_t& result,char kind) {
  VisionPacket p;p.kind=kind;
  if(result.box.size()<4)return p;
  p.x0=std::clamp(result.box[0],0,319);p.y0=std::clamp(result.box[1],0,239);
  p.x1=std::clamp(result.box[2],0,320);p.y1=std::clamp(result.box[3],0,240);
  p.score=uint16_t(std::clamp(result.score,0.f,1.f)*1000+0.5f);p.found=p.x1>p.x0&&p.y1>p.y0;
  return p;
}
void send(VisionPacket p,int64_t began) {
  p.seq=sequence++;p.camMs=uint32_t(esp_timer_get_time()/1000);p.inferMs=uint32_t((esp_timer_get_time()-began)/1000);
  if(!p.found){p.score=p.x0=p.y0=p.x1=p.y1=0;}
  char packet[256];if(!encodeVision(p,packet,sizeof(packet))){ESP_LOGE(TAG,"Invalid generated vision frame");return;}
  uart_write_bytes(MAIN_UART,packet,strlen(packet));uart_write_bytes(MAIN_UART,"\r\n",2);
  printf("%s\r\n",packet);
}
} // namespace
extern "C" void app_main() {
  ESP_ERROR_CHECK(init_link_uart());ESP_ERROR_CHECK(init_camera());
  HandDetect hands(HandDetect::ESPDET_PICO_224_224_HAND,false);
  hands.set_score_thr(tuning::balanced?.16f:.20f);
  HandGestureRecognizer gestures(HandGestureCls::MOBILENETV2_0_5_S8_V1);
#if CONFIG_CAREROVER_FACE
  HumanFaceDetect faces(
#if CONFIG_CAREROVER_PICO_FACE
    HumanFaceDetect::ESPDET_PICO_224_224_FACE,
#else
    HumanFaceDetect::MSRMNP_S8_V1,
#endif
    false);
  faces.set_score_thr(tuning::balanced?.35f:.42f,0);
#if !CONFIG_CAREROVER_PICO_FACE
  faces.set_score_thr(tuning::balanced?.38f:.42f,1);
#endif
#endif
#if CONFIG_CAREROVER_VIDEO
  wifiBegin();ESP_ERROR_CHECK(videoBegin()?ESP_OK:ESP_FAIL);
#endif
  BoxTrack association;uint32_t associationSeq=0;
  bool tracking=false;uint8_t frameTurn=0;VisionPacket previous;
  uint32_t handCount=0,faceCount=0,captureDrops=0;int64_t epoch=esp_timer_get_time();
  uint32_t handMs=0,faceMs=0,previousJpeg=0,previousSent=0,minHeap=UINT32_MAX,minPsram=UINT32_MAX;
  for(;;) {
    auto* frame=esp_camera_fb_get();if(!frame){++captureDrops;vTaskDelay(pdMS_TO_TICKS(10));continue;}
    const dl::image::img_t image={.data=frame->buf,.width=uint16_t(frame->width),.height=uint16_t(frame->height),.pix_type=dl::image::DL_IMAGE_PIX_TYPE_RGB565BE};
    const int64_t began=esp_timer_get_time();
#if CONFIG_CAREROVER_FACE
    // Gesture inference is the interactive path. Run two gesture frames for
    // every face frame; the face cadence remains comfortably inside the
    // relaxed 900 ms source deadline while gesture response improves.
    const bool faceTurn=(frameTurn++%3U)==2U;
    if(faceTurn) {
      auto& results=faces.run(image);VisionPacket selected;float best=-1001,second=-1001;
      for(const auto& result:results) {
        auto candidate=box(result,'P');if(!candidate.found||candidate.score<(tuning::balanced?320:450))continue;
        const float score=tuning::balanced&&association.ready(began/1000)?-association.cost(candidate,began/1000):tracking&&!tuning::balanced?boxIou(previous,candidate):float((candidate.x1-candidate.x0)*(candidate.y1-candidate.y0));
        if(score>best){second=best;best=score;selected=candidate;}else if(score>second)second=score;
      }
      if(tuning::balanced){
        if(association.ready(began/1000)&&(best<=-1000||(second>-1000&&best-second<.08f)))selected=VisionPacket{};
        selected.receivedMs=began/1000;selected.seq=++associationSeq;
        if(selected.found&&!association.update(selected))selected=VisionPacket{};
      }else{
        if(tracking&&(best<0.03f||(second>=0&&best-second<0.02f)))selected=VisionPacket{};
        tracking=selected.found;if(tracking)previous=selected;
      }
      send(selected,began);++faceCount;faceMs=uint32_t((esp_timer_get_time()-began)/1000);
    } else
#endif
    {
      VisionPacket p;p.kind='G';auto& detections=hands.run(image);
      if(!detections.empty()) {
        auto best=std::max_element(detections.begin(),detections.end(),[](const auto& a,const auto& b){return a.score<b.score;});
        p=box(*best,'G');strlcpy(p.label,"no_gesture",sizeof(p.label));p.score=0;
        std::list<dl::detect::result_t> one={*best};auto result=gestures.recognize(image,one);
        if(!result.empty()){strlcpy(p.label,result[0].cat_name,sizeof(p.label));p.score=uint16_t(std::clamp(result[0].score,0.f,1.f)*1000+0.5f);}
      }
      send(p,began);++handCount;handMs=uint32_t((esp_timer_get_time()-began)/1000);
    }
    esp_camera_fb_return(frame);
    minHeap=std::min(minHeap,uint32_t(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
    minPsram=std::min(minPsram,uint32_t(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)));
    const int64_t now=esp_timer_get_time();if(now-epoch>=2000000){
      const double seconds=(now-epoch)/1000000.0;
      printf("{\"type\":\"cam_metrics\",\"gesture_fps\":%.2f,\"face_fps\":%.2f,\"jpeg_fps\":%.2f,\"stream_fps\":%.2f,\"gesture_ms\":%u,\"face_ms\":%u,\"jpeg_ms\":%u,\"jpeg_total\":%u,\"jpeg_drops\":%u,\"capture_drops\":%u,\"video_capture_failures\":%u,\"min_heap\":%u,\"min_psram\":%u,\"wifi\":%s}\n",handCount/seconds,faceCount/seconds,(videoEncoded()-previousJpeg)/seconds,(videoSent()-previousSent)/seconds,unsigned(handMs),unsigned(faceMs),unsigned(videoLastEncodeMs()),unsigned(videoEncoded()),unsigned(videoDropped()),unsigned(captureDrops),unsigned(videoCaptureFailures()),unsigned(minHeap),unsigned(minPsram),wifiConnected.load()?"true":"false");
      previousJpeg=videoEncoded();previousSent=videoSent();handCount=faceCount=0;epoch=now;
    }
    vTaskDelay(1);
  }
}
