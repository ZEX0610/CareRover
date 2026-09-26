#include "serverchan_notify.h"
#include "wifi_secrets.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <cJSON.h>
#include <esp_timer.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <cstring>

namespace {
// Public ISRG Root X1, SHA-256 96BCEC06264976F37460779ACF28C5A7CFE8A3C0AAE11A8FFCEE05C0BDDF08C6.
// Do not replace this with setInsecure(): the SendKey is sent in the HTTPS URL.
constexpr char RootCa[] = R"PEM(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAwTzELMAkGA1UE
BhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2VhcmNoIEdyb3VwMRUwEwYDVQQD
EwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQG
EwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMT
DElTUkcgUm9vdCBYMTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54r
Vygch77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+0TM8ukj1
3Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6UA5/TR5d8mUgjU+g4rk8K
b4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sWT8KOEUt+zwvo/7V3LvSye0rgTBIlDHCN
Aymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyHB5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ
4Q7e2RCOFvu396j3x+UCB5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf
1b0SHzUvKBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWnOlFu
hjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTnjh8BCNAw1FtxNrQH
usEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbwqHyGO0aoSCqI3Haadr8faqU9GY/r
OPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CIrU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4G
A1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY
9umbbjANBgkqhkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ3BebYhtF8GaV
0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KKNFtY2PwByVS5uCbMiogziUwt
hDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJw
TdwJx4nLCgdNbOhdjsnvzqvHu7UrTkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nx
e5AW0wdeRlN8NwdCjNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZA
JzVcoyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq4RgqsahD
YVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPAmRGunUHBcnWEvgJBQl9n
JEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57demyPxgcYxn/eR44/KJ4EBs+lVDR3veyJ
m+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)PEM";

struct Event { carerover::SeatNotice kind; uint64_t createdMs; };
QueueHandle_t queue = nullptr;
constexpr uint64_t EventTtlMs = 60000;

bool configured(const char* value) {
  return value && *value && strncmp(value,"REPLACE_",8) != 0;
}
bool subnetCollision() {
  const IPAddress ip = WiFi.localIP();
  return ip[0] == 192 && ip[1] == 168 && ip[2] == 4;
}
bool sendEvent(carerover::SeatNotice kind) {
  WiFiClientSecure client;
  client.setCACert(RootCa);
  client.setTimeout(5000);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  const String url = String("https://sctapi.ftqq.com/") + CAREROVER_WECHAT_SENDKEY + ".send";
  if (!http.begin(client,url)) return false;
  http.addHeader("Content-Type","application/x-www-form-urlencoded");
  const char* body = kind == carerover::SeatNotice::Seated
    ? "title=CareRover-Seated&desp=HC-SR04+confirmed+seated"
    : "title=CareRover-Vacant&desp=HC-SR04+confirmed+vacant";
  const int status = http.POST(String(body));
  bool ok = false;
  if (status == 200 && http.getSize() <= 2048) {
    const String response = http.getString();
    if (response.length() <= 2048) {
      cJSON* json = cJSON_Parse(response.c_str());
      const cJSON* code = json ? cJSON_GetObjectItemCaseSensitive(json,"code") : nullptr;
      ok = cJSON_IsNumber(code) && code->valueint == 0;
      if (json) cJSON_Delete(json);
    }
  }
  http.end();
  // Never print URL, response body, SSID, hotspot password or SendKey.
  Serial.printf("{\"type\":\"wechat_push\",\"event\":\"%s\",\"ok\":%s,\"http_status\":%d}\n",
    kind == carerover::SeatNotice::Seated ? "seated" : "vacant",ok ? "true" : "false",status);
  return ok;
}
void worker(void*) {
  Event pending{};
  bool hasPending = false, clockStarted = false, collisionLogged = false;
  for (;;) {
    Event next{};
    if (xQueueReceive(queue,&next,pdMS_TO_TICKS(1000)) == pdTRUE) {
      pending = next; hasPending = true;
    }
    if (!hasPending) continue;
    const uint64_t now = esp_timer_get_time() / 1000ULL;
    if (now < pending.createdMs || now - pending.createdMs > EventTtlMs) {
      hasPending = false; Serial.println("{\"type\":\"wechat_push\",\"status\":\"expired\"}"); continue;
    }
    if (WiFi.status() != WL_CONNECTED) continue;
    if (subnetCollision()) {
      if (!collisionLogged) Serial.println("{\"type\":\"wechat_push\",\"status\":\"uplink_subnet_collision\"}");
      collisionLogged = true; continue;
    }
    if (!clockStarted) { configTime(0,0,"pool.ntp.org","time.google.com"); clockStarted = true; }
    if (time(nullptr) < 1735689600) continue; // Verified TLS needs a real UTC clock.
    // Server酱没有本项目可用的幂等键。即使请求超时也可能已经投递，
    // 所以每次入/离座事件最多尝试一次，绝不自动重发造成重复微信消息。
    sendEvent(pending.kind);
    hasPending = false;
  }
}
}

bool serverchanConfigured() {
  return configured(CAREROVER_STA_SSID) && configured(CAREROVER_WECHAT_SENDKEY);
}
void serverchanBegin() {
  if (!serverchanConfigured()) {
    Serial.println("{\"type\":\"wechat_push\",\"status\":\"not_configured\"}");
    return;
  }
  queue = xQueueCreate(1,sizeof(Event));
  if (!queue || xTaskCreate(worker,"wechat",10240,nullptr,1,nullptr) != pdPASS) {
    Serial.println("{\"type\":\"wechat_push\",\"status\":\"worker_failed\"}");
    return;
  }
  WiFi.setAutoReconnect(true);
  WiFi.begin(CAREROVER_STA_SSID,CAREROVER_STA_PASSWORD);
  Serial.println("{\"type\":\"wechat_push\",\"status\":\"sta_starting\"}");
}
void serverchanSeatEvent(carerover::SeatNotice notice,uint64_t nowMs) {
  if (notice == carerover::SeatNotice::None || !queue) return;
  const Event event{notice,nowMs};
  xQueueOverwrite(queue,&event); // A current event supersedes an unsent old one.
}
