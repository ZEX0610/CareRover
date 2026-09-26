#include "wireless_runtime.h"
#include "safety_controller.h"
#include "frame_guard.h"
#include "gesture_actions.h"
#include "mpu6050_soft.h"
#include "motion_calibration.h"
#include "hcsr04.h"
#include "seat_notification_gate.h"
#include "serverchan_notify.h"
#include <Arduino.h>
#include <WiFi.h>
#include <FFat.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <atomic>
#include <cstring>
#include <cmath>
#include <cctype>
#include <sys/socket.h>
#include <unistd.h>
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#else
#error "Create private wifi_secrets.h from wifi_secrets.example.h before compiling"
#endif
#if __has_include("build_version.h")
#include "build_version.h"
#else
#define CAREROVER_BUILD_VERSION "unversioned-local"
#endif
#ifndef CAREROVER_STAGE
#define CAREROVER_STAGE 5
#endif
#ifndef CAREROVER_INTEGRATION
#define CAREROVER_INTEGRATION 0
#endif
static_assert(CAREROVER_INTEGRATION>=0 && CAREROVER_INTEGRATION<=3, "Bad integration mode");
static_assert(CAREROVER_STAGE >= 1 && CAREROVER_STAGE <= 5, "Stage must be 1..5");

namespace {
using namespace carerover;
constexpr size_t MaxClients = 4, MaxFrame = 65536;
portMUX_TYPE safetyMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE sourceMux = portMUX_INITIALIZER_UNLOCKED;
SafetyController safety;
FrontInstallation frontInstallation;
GestureActionLatch gestureActions(4,3,tuning::balanced);
uint64_t pendingGestureFollowUntil=0;
uint32_t pendingGestureStopSequence=0;
ContinuousServoDrive drive;
bool calibrationReady=false;
std::atomic<bool> driveReady{false};
httpd_handle_t server = nullptr;
std::atomic<bool> publishPending{false};
std::atomic<bool> apOnline{false};
std::atomic<uint32_t> publishDrops{0};
std::atomic<uint32_t> maxSafetyGapMs{0};
struct Sources {
  BoxTrack displayTrack;
  bool gestureHeld=false;uint64_t gestureFreshMs=0;
  WirelessHealth health;
  VisionPacket person;
  ImuSample imu;
  bool personSeen=false;
  uint32_t resultCount=0;
  uint64_t fpsStart=0;
  float aiFps=0;
  char label[24] = "NONE";
  char displayLabel[24] = "NONE";
  float score = 0;
  float displayScore = 0;
  bool accepted = false;
  bool displayAccepted = false;
  uint32_t inferMs = 0, gestureSeq = 0, healthSeq = 0, ppgSeq = 0;
  uint64_t gestureMs = 0, displayGestureMs = 0, ppgMs = 0;
  uint32_t ppg[5] = {};
} sources;
uint32_t partialPpg[5] = {};
size_t partialCount = 0; // Written only by Arduino sensor loop.
struct Session {
  int fd = -1;
  uint32_t id = 0;
  bool clockReady = false;
  double unixBase = 0, lastPingId=-1;
  uint64_t monoBase = 0;
  uint32_t gestureSeq = UINT32_MAX, healthSeq = UINT32_MAX, ppgSeq = 0;
  bool gestureFresh = false, healthFresh = false;
};
Session clients[MaxClients]; // Only HTTP server task touches session state.
uint32_t nextClientId = 1;
SafetySnapshot readSafety(uint64_t now) {
  portENTER_CRITICAL(&safetyMux); auto s = safety.snapshot(now); portEXIT_CRITICAL(&safetyMux); return s;
}
void failSafe(bool fault, const char* reason) {
  const auto now = wirelessNowMs();
  portENTER_CRITICAL(&safetyMux);
  if (fault) safety.fault(true, now); else safety.stop(now, reason);
  portEXIT_CRITICAL(&safetyMux);
}
Session* sessionFor(int fd) { for (auto& c : clients) if (c.fd == fd) return &c; return nullptr; }
void closeSocket(httpd_handle_t, int fd) {
  if (auto* c = sessionFor(fd)) {
    const auto now = wirelessNowMs();
    portENTER_CRITICAL(&safetyMux); safety.disconnect(c->id, now); portEXIT_CRITICAL(&safetyMux);
    *c = Session{};
  }
  close(fd);
}
void drop(Session& c) {
  const auto now = wirelessNowMs();
  portENTER_CRITICAL(&safetyMux); safety.disconnect(c.id, now); portEXIT_CRITICAL(&safetyMux);
  // Shutdown immediately interrupts pending socket I/O. close_fn owns final cleanup.
  shutdown(c.fd, SHUT_RDWR);
  httpd_sess_trigger_close(server, c.fd);
}
double timestamp(const Session& c, uint64_t now) { return c.clockReady ? c.unixBase + double(now - c.monoBase) : 0; }
cJSON* object(const char* type, const Session& c, uint64_t now) {
  auto* j = cJSON_CreateObject();
  if (j) { cJSON_AddStringToObject(j,"type",type); cJSON_AddNumberToObject(j,"ts",timestamp(c,now)); }
  return j;
}
bool sendJson(Session& c, cJSON* message) {
  if (!message) { drop(c); return false; }
  char* text = cJSON_PrintUnformatted(message); cJSON_Delete(message);
  if (!text) { drop(c); return false; }
  httpd_ws_frame_t frame{}; frame.type = HTTPD_WS_TYPE_TEXT;
  frame.payload = reinterpret_cast<uint8_t*>(text); frame.len = strlen(text);
  // This API may block on a slow socket. It runs ONLY on the HTTP task, never
  // the sensor or safety task. One pending publish job bounds queue growth.
  const auto result = httpd_ws_send_frame_async(server, c.fd, &frame);
  cJSON_free(text);
  if (result != ESP_OK) { drop(c); return false; }
  return true;
}
const cJSON* field(const cJSON* j, const char* key) { return cJSON_GetObjectItemCaseSensitive(j,key); }
bool number(const cJSON* j) { return cJSON_IsNumber(j) && std::isfinite(j->valuedouble); }
bool safeInteger(const cJSON* j) { return number(j) && j->valuedouble >= 0 && j->valuedouble <= 9007199254740991.0 && floor(j->valuedouble) == j->valuedouble; }
void requestInfo(cJSON* response, const cJSON* request) {
  if (auto* id = field(request,"request_id"); safeInteger(id)) cJSON_AddNumberToObject(response,"request_id",id->valuedouble);
  if (auto* type = field(request,"type"); cJSON_IsString(type)) cJSON_AddStringToObject(response,"request_type",type->valuestring);
}
bool replyError(Session& c, const char* code, const cJSON* request = nullptr) {
  auto* j = object("error",c,wirelessNowMs());
  cJSON_AddStringToObject(j,"code",code); cJSON_AddStringToObject(j,"message",code);
  if (request) requestInfo(j,request);
  return sendJson(c,j);
}
bool replyAck(Session& c, const cJSON* request) {
  auto* j = object("ack",c,wirelessNowMs()); requestInfo(j,request);
  cJSON_AddBoolToObject(j,"ok",true); return sendJson(c,j);
}
// Reject duplicates instead of accepting an ambiguous type/vx/ts value.
bool uniqueFields(const cJSON* j) {
  for (auto* a=j->child;a;a=a->next) for (auto* b=a->next;b;b=b->next)
    if (a->string && b->string && strcmp(a->string,b->string)==0) return false;
  return true;
}
void command(Session& c, const cJSON* j) {
  const auto now = wirelessNowMs();
  const auto* type = field(j,"type");
  if (!cJSON_IsObject(j) || cJSON_GetArraySize(j)>8 || !uniqueFields(j) || !cJSON_IsString(type)) { replyError(c,"INVALID_COMMAND"); return; }
  const char* kind = type->valuestring;
  if(strlen(kind)>32) { replyError(c,"INVALID_COMMAND"); return; }
  // Emergency stop never waits for clock setup, mode, ownership, or stage.
  if (strcmp(kind,"estop")==0) {
    portENTER_CRITICAL(&safetyMux); safety.emergency(now); portEXIT_CRITICAL(&safetyMux);
    replyAck(c,j); return;
  }
  const auto* ts = field(j,"ts");
  if (!safeInteger(ts)) { replyError(c,"INVALID_COMMAND",j); return; }
  if (strcmp(kind,"ping")==0) {
    const auto* id=field(j,"id");
    if (!safeInteger(id) || ts->valuedouble < 946684800000.0 || ts->valuedouble > 4102444800000.0) { replyError(c,"INVALID_COMMAND",j); return; }
    if (!c.clockReady) { c.unixBase=ts->valuedouble; c.monoBase=now; c.clockReady=true; }
    if(id->valuedouble>c.lastPingId && commandAgeAllowed(timestamp(c,now)-ts->valuedouble)) {
      c.lastPingId=id->valuedouble;
      portENTER_CRITICAL(&safetyMux); safety.heartbeat(c.id,now); portEXIT_CRITICAL(&safetyMux);
    }
    auto* response=object("pong",c,now); cJSON_AddNumberToObject(response,"id",id->valuedouble); sendJson(c,response); return;
  }
  if (auto* id=field(j,"request_id"); id && !safeInteger(id)) { replyError(c,"INVALID_COMMAND",j); return; }
  if (!c.clockReady) { replyError(c,"CLOCK_NOT_READY",j); return; }
  if (strcmp(kind,"cmd_vel")!=0 && strcmp(kind,"set_mode")!=0 && strcmp(kind,"clear_estop")!=0) { replyError(c,"UNKNOWN_TYPE",j); return; }
  // Until stage 4, the wireless endpoint is read-only except emergency stop and zero release.
  const auto* x=field(j,"vx"); const auto* y=field(j,"vy"); const auto* z=field(j,"wz");
  const bool velocity = strcmp(kind,"cmd_vel")==0;
  const bool zero = velocity && number(x) && number(y) && number(z) && x->valuedouble==0 && y->valuedouble==0 && z->valuedouble==0;
  if ((CAREROVER_STAGE < 4 || CAREROVER_INTEGRATION==1) && !zero) { replyError(c,"READ_ONLY",j); return; }
  // Reject old queued motion/mode/recovery requests. Clock is display-only for
  // watchdogs; this additional per-session age check never refreshes a lease.
  const double age = timestamp(c,now)-ts->valuedouble;
  if (!zero && !commandAgeAllowed(age)) { replyError(c,"STALE_COMMAND",j); return; }
  const char* error = nullptr;
  if (velocity) {
    if (!number(x)||!number(y)||!number(z)) { replyError(c,"INVALID_COMMAND",j); return; }
    portENTER_CRITICAL(&safetyMux);
    error=safety.velocity(c.id,x->valuedouble,y->valuedouble,z->valuedouble,now);
    portEXIT_CRITICAL(&safetyMux);
  } else if (strcmp(kind,"set_mode")==0) {
    const auto* m=field(j,"mode");
    if (!cJSON_IsString(m)) { replyError(c,"INVALID_MODE",j); return; }
    Mode mode;
    if (strcmp(m->valuestring,"IDLE")==0) mode=Mode::Idle;
    else if (strcmp(m->valuestring,"MANUAL")==0) mode=Mode::Manual;
    else if (strcmp(m->valuestring,"HEALTH_CHECK")==0) mode=Mode::Health;
    else if (CAREROVER_INTEGRATION>=3 && !strcmp(m->valuestring,"PERSON_FOLLOW")) mode=Mode::Follow;
    else if (CAREROVER_INTEGRATION>=3 && !strcmp(m->valuestring,"GESTURE_CONTROL")) mode=Mode::Gesture;
    else { replyError(c,strcmp(m->valuestring,"PERSON_FOLLOW")==0 || strcmp(m->valuestring,"GESTURE_CONTROL")==0 ? "UNSUPPORTED_MODE" : "INVALID_MODE",j); return; }
    portENTER_CRITICAL(&safetyMux); error=safety.setMode(c.id,mode,now); portEXIT_CRITICAL(&safetyMux);
  } else {
    portENTER_CRITICAL(&safetyMux); error=safety.clear(c.id,now); portEXIT_CRITICAL(&safetyMux);
  }
  if (error) replyError(c,error,j); else if (!velocity) replyAck(c,j);
}
bool sameOrigin(httpd_req_t* req) {
  const size_t n=httpd_req_get_hdr_value_len(req,"Origin");
  if (!n) return true; // CLI diagnostic clients do not send Origin.
  if (n>160) return false;
  char origin[161], host[128];
  if (httpd_req_get_hdr_value_str(req,"Origin",origin,sizeof(origin))!=ESP_OK || httpd_req_get_hdr_value_str(req,"Host",host,sizeof(host))!=ESP_OK) return false;
  char expected[136]; snprintf(expected,sizeof(expected),"http://%s",host);
  return strcmp(origin,expected)==0;
}
esp_err_t wsHandler(httpd_req_t* req) {
  const int fd=httpd_req_to_sockfd(req);
  if (req->method==HTTP_GET) {
    if (!sameOrigin(req)) return httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"Origin rejected");
    for (auto& c:clients) if (c.fd==-1) { c=Session{}; c.fd=fd; c.id=nextClientId++; if (!nextClientId) nextClientId=1; return ESP_OK; }
    httpd_resp_set_status(req,"503 Service Unavailable");
    return httpd_resp_send(req,"Too many clients",HTTPD_RESP_USE_STRLEN);
  }
  auto* c=sessionFor(fd); if (!c) return ESP_FAIL;
  httpd_ws_frame_t frame{};
  if (httpd_ws_recv_frame(req,&frame,0)!=ESP_OK) { drop(*c); return ESP_FAIL; }
  if (frame.type!=HTTPD_WS_TYPE_TEXT || frame.len>MaxFrame || !frame.final) { drop(*c); return ESP_FAIL; }
  auto* buffer=static_cast<uint8_t*>(malloc(frame.len+1));
  if (!buffer) { drop(*c); return ESP_ERR_NO_MEM; }
  frame.payload=buffer;
  if (httpd_ws_recv_frame(req,&frame,frame.len)!=ESP_OK) { free(buffer); drop(*c); return ESP_FAIL; }
  buffer[frame.len]=0;
  if(!boundedJsonText(reinterpret_cast<char*>(buffer),frame.len)) { free(buffer); replyError(*c,"INVALID_JSON"); return ESP_OK; }
  const char* end=nullptr;
  auto* j=cJSON_ParseWithLengthOpts(reinterpret_cast<char*>(buffer),frame.len+1,&end,true);
  // Embedded NUL/trailing data are not valid JSON text frames.
  const bool complete=end==reinterpret_cast<char*>(buffer)+frame.len;
  if (!j || !complete) replyError(*c,"INVALID_JSON"); else command(*c,j);
  cJSON_Delete(j); free(buffer); return ESP_OK;
}
const char* mime(const char* path) {
  const auto* ext=strrchr(path,'.'); if (!ext) return nullptr;
  if (!strcmp(ext,".html")) return "text/html; charset=utf-8";
  if (!strcmp(ext,".js")) return "text/javascript; charset=utf-8";
  if (!strcmp(ext,".css")) return "text/css; charset=utf-8";
  if (!strcmp(ext,".png")) return "image/png";
  if (!strcmp(ext,".svg")) return "image/svg+xml";
  if (!strcmp(ext,".ico")) return "image/x-icon";
  if (!strcmp(ext,".json")) return "application/json";
  if (!strcmp(ext,".md")) return "text/plain; charset=utf-8";
  return nullptr;
}
esp_err_t fileHandler(httpd_req_t* req) {
  if (!strcmp(req->uri,"/")) {
    httpd_resp_set_status(req,"302 Found"); httpd_resp_set_hdr(req,"Location",CAREROVER_INTEGRATION ? "/?transport=ws&video=mjpeg" : "/?transport=ws&video=canvas");
    httpd_resp_set_hdr(req,"Cache-Control","no-store"); return httpd_resp_send(req,nullptr,0);
  }
  char path[160]; const size_t length=strcspn(req->uri,"?");
  if(length>=sizeof(path)) return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"Not found");
  memcpy(path,req->uri,length); path[length]=0;
  if(!strcmp(path,"/")) strcpy(path,"/index.html");
  const bool allowed=!strcmp(path,"/docs/protocol.md") || !strcmp(path,"/index.html") || !strcmp(path,"/version.json") || !strncmp(path,"/js/",4) || !strncmp(path,"/css/",5) || !strncmp(path,"/assets/",8);
  const char* contentType=mime(path);
  if(!allowed || !contentType || strstr(path,"..") || strchr(path,'%') || strchr(path,'\\')) return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"Not found");
  auto file=FFat.open(path,"r");
  if(!file || file.isDirectory()) return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"Not found");
  httpd_resp_set_type(req,contentType); httpd_resp_set_hdr(req,"Cache-Control","no-store");
  httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
  char chunk[2048];
  while(file.available()) { const size_t n=file.readBytes(chunk,sizeof(chunk)); if(!n || httpd_resp_send_chunk(req,chunk,n)!=ESP_OK) { file.close(); return ESP_FAIL; } }
  file.close(); return httpd_resp_send_chunk(req,nullptr,0);
}
const char* healthState(const WirelessHealth& h, bool fresh) {
  if(!h.ready || !fresh) return "ERROR";
  if(!h.finger) return "NO_FINGER";
  if(!strcmp(h.state,"poor_signal")) return "LOW_QUALITY";
  if(!strcmp(h.state,"acquiring")) return "ACQUIRING";
  if(h.hrValid || h.spo2Valid) return "VALID";
  return "MEASURING";
}
void publish(void*) {
  Sources source;
  portENTER_CRITICAL(&sourceMux); source=sources; portEXIT_CRITICAL(&sourceMux);
  for(auto& c:clients) {
    if(c.fd<0 || !c.clockReady) continue;
    const auto now=wirelessNowMs(); const auto state=readSafety(now);
    if(httpd_ws_get_fd_info(server,c.fd)!=HTTPD_WS_CLIENT_WEBSOCKET) continue;
    auto* j=object("telemetry",c,now);
    cJSON_AddStringToObject(j,"tuning_profile",tuning::name);
    auto* connection=cJSON_AddObjectToObject(j,"connection");
    cJSON_AddBoolToObject(connection,"camera",state.camera); cJSON_AddBoolToObject(connection,"main_mcu",true);
    auto* front=cJSON_AddObjectToObject(j,"front");
    cJSON_AddBoolToObject(front,"enabled",state.front.enabled);cJSON_AddBoolToObject(front,"ready",state.front.ready);
    cJSON_AddBoolToObject(front,"valid",state.front.valid);
    if(state.front.valid)cJSON_AddNumberToObject(front,"distance_cm",state.front.distanceCm);else cJSON_AddNullToObject(front,"distance_cm");
    cJSON_AddNumberToObject(front,"age_ms",double(state.front.ageMs));
    cJSON_AddStringToObject(front,"status",state.front.status);cJSON_AddStringToObject(front,"phase",phaseName(state.front.phase));
    cJSON_AddBoolToObject(front,"seated",state.seated);
    cJSON_AddBoolToObject(front,"release_required",state.front.held);
    cJSON_AddStringToObject(front,"stop_reason",state.stopReason);
    auto* robot=cJSON_AddObjectToObject(j,"robot");
    const char* mode=state.estop?"ESTOP":state.fault?"FAULT":modeName(state.mode);
    cJSON_AddStringToObject(robot,"mode",mode);
    cJSON_AddStringToObject(robot,"state",state.estop?"ESTOP":state.fault?"FAULT":state.mode==Mode::Manual?(state.target.vx||state.target.vy||state.target.wz?"DRIVING":"READY"):state.mode==Mode::Health?"MEASURING":state.mode==Mode::Follow?"TRACKING":state.mode==Mode::Gesture?(state.target.wz?"TURNING":"READY"):"IDLE");
    cJSON_AddBoolToObject(robot,"estop",state.estop);
    cJSON_AddBoolToObject(robot,"motion_output_installed",driveReady.load());
    cJSON_AddBoolToObject(robot,"calibration_ready",calibrationReady);
    cJSON_AddBoolToObject(robot,"control_allowed",state.owner==0 || state.owner==c.id);
    cJSON_AddNumberToObject(robot,"vx",state.target.vx); cJSON_AddNumberToObject(robot,"vy",state.target.vy); cJSON_AddNumberToObject(robot,"wz",state.target.wz);
    auto* device=cJSON_AddObjectToObject(j,"device");
    cJSON_AddStringToObject(device,"firmware",CAREROVER_BUILD_VERSION); cJSON_AddNumberToObject(device,"stage",CAREROVER_STAGE);
    cJSON_AddStringToObject(device,"backend",CAREROVER_INTEGRATION?"tracking":"test_targets");
    cJSON_AddStringToObject(device,"integration",CAREROVER_INTEGRATION==3?"follow":CAREROVER_INTEGRATION==2?"manual":CAREROVER_INTEGRATION==1?"observe":"legacy");
    auto* modes=cJSON_AddArrayToObject(device,"supported_modes");
    if(CAREROVER_STAGE>=4 && CAREROVER_INTEGRATION!=1) {
      cJSON_AddItemToArray(modes,cJSON_CreateString("IDLE"));
      cJSON_AddItemToArray(modes,cJSON_CreateString("HEALTH_CHECK"));
      cJSON_AddItemToArray(modes,cJSON_CreateString("MANUAL"));
      if(CAREROVER_INTEGRATION>=3) {
        cJSON_AddItemToArray(modes,cJSON_CreateString("PERSON_FOLLOW"));
        cJSON_AddItemToArray(modes,cJSON_CreateString("GESTURE_CONTROL"));
      }
    }
    cJSON_AddNumberToObject(device,"uptime_ms",double(now));
    cJSON_AddNumberToObject(device,"last_cmd_ms",double(state.lastCommandMs)); cJSON_AddNumberToObject(device,"stopped_at_ms",double(state.stoppedAtMs));
    cJSON_AddNumberToObject(device,"stop_sequence",state.stopSequence); cJSON_AddStringToObject(device,"stop_reason",state.stopReason);
    cJSON_AddNumberToObject(device,"max_safety_gap_ms",maxSafetyGapMs.load()); cJSON_AddNumberToObject(device,"publish_drops",publishDrops.load());
    cJSON_AddNumberToObject(device,"free_heap",ESP.getFreeHeap());
    cJSON_AddNumberToObject(device,"free_psram",ESP.getFreePsram());
    cJSON_AddNumberToObject(device,"min_free_heap",ESP.getMinFreeHeap());
    cJSON_AddNumberToObject(device,"min_free_psram",ESP.getMinFreePsram());
    auto* vision=cJSON_AddObjectToObject(j,"vision");
    cJSON_AddNumberToObject(vision,"image_width",320);cJSON_AddNumberToObject(vision,"image_height",240);
    cJSON_AddNumberToObject(vision,"ai_fps",state.camera?source.aiFps:0);
    const bool displayGestureFresh=source.displayAccepted && now>=source.displayGestureMs &&
      now-source.displayGestureMs<tuning::GestureDisplayHoldMs;
    if(c.gestureSeq!=source.gestureSeq || c.gestureFresh!=displayGestureFresh) {
      auto* gesture=cJSON_AddObjectToObject(vision,"gesture");
      cJSON_AddStringToObject(gesture,"label",displayGestureFresh?source.displayLabel:"NONE");
      cJSON_AddNumberToObject(gesture,"confidence",displayGestureFresh?source.displayScore:0);
      cJSON_AddBoolToObject(gesture,"stable",displayGestureFresh);
      cJSON_AddBoolToObject(gesture,"held",displayGestureFresh&&source.gestureHeld);
      cJSON_AddNumberToObject(gesture,"age_ms",displayGestureFresh?double(now-source.displayGestureMs):0);
      c.gestureSeq=source.gestureSeq;c.gestureFresh=displayGestureFresh;
    }
    if(CAREROVER_INTEGRATION) {
      // Use the bounded Kalman display track for every tuning profile. A
      // transient detector miss should not erase the last box; this prediction
      // is display-only and is never passed to the motion controller.
      auto p=source.displayTrack.view(now,450);
      const uint64_t age=source.personSeen&&now>=p.receivedMs?now-p.receivedMs:tuning::PersonDisplayMs;
      const bool predicted=source.personSeen &&
        ((now>=source.person.receivedMs && now-source.person.receivedMs>150) || !source.person.found);
      const bool found=source.displayTrack.ready(now)&&age<tuning::PersonDisplayMs&&p.found;
      auto* person=cJSON_AddObjectToObject(vision,"person");
      cJSON_AddBoolToObject(person,"found",found);
      cJSON_AddBoolToObject(person,"predicted",found&&predicted);
      cJSON_AddNumberToObject(person,"seq",p.seq);cJSON_AddNumberToObject(person,"age_ms",double(age));
      cJSON_AddNumberToObject(person,"x",found?p.x0:0);cJSON_AddNumberToObject(person,"y",found?p.y0:0);
      cJSON_AddNumberToObject(person,"w",found?p.x1-p.x0:0);cJSON_AddNumberToObject(person,"h",found?p.y1-p.y0:0);
      cJSON_AddNumberToObject(person,"confidence",found?p.score/1000.0:0);
      auto* video=cJSON_AddObjectToObject(j,"video");
      cJSON_AddStringToObject(video,"stream_url","http://192.168.4.2/stream");cJSON_AddStringToObject(video,"protocol","mjpeg");
      cJSON_AddNumberToObject(video,"source_width",320);cJSON_AddNumberToObject(video,"source_height",240);
      auto* imu=cJSON_AddObjectToObject(j,"imu"); const auto& i=source.imu;
      const bool fresh=i.valid&&now>=i.sampleMs&&now-i.sampleMs<tuning::ImuSafetyMs;
      cJSON_AddBoolToObject(imu,"valid",fresh);cJSON_AddBoolToObject(imu,"calibrated",i.calibrated);
      cJSON_AddBoolToObject(imu,"tilt_fault",i.tiltFault);
      cJSON_AddBoolToObject(imu,"held",i.held);cJSON_AddBoolToObject(imu,"warning_tilt",i.warningTilt);
      cJSON_AddNumberToObject(imu,"rejected_frames",i.rejectedFrames);cJSON_AddNumberToObject(imu,"accepted_frames",i.acceptedFrames);
      cJSON_AddNumberToObject(imu,"age_ms",double(now-i.sampleMs));
      if(fresh) { cJSON_AddNumberToObject(imu,"yaw_deg",i.yaw);cJSON_AddNumberToObject(imu,"pitch_deg",i.pitch);cJSON_AddNumberToObject(imu,"roll_deg",i.roll); }
      else { cJSON_AddNullToObject(imu,"yaw_deg");cJSON_AddNullToObject(imu,"pitch_deg");cJSON_AddNullToObject(imu,"roll_deg"); }
    }
    const auto& h=source.health;
    const bool fresh=h.sampleMs && now-h.sampleMs<250 && now-h.reportMs<2500;
    if(c.healthSeq!=source.healthSeq || c.healthFresh!=fresh) {
      auto* health=cJSON_AddObjectToObject(j,"health"); const char* status=healthState(h,fresh);
      const bool valid=fresh&&h.ready&&h.finger;
      cJSON_AddBoolToObject(health,"hr_valid",valid&&h.hrValid);cJSON_AddBoolToObject(health,"spo2_valid",valid&&h.spo2Valid);
      cJSON_AddBoolToObject(health,"hr_held",valid&&h.hrValid&&h.hrHeld);cJSON_AddBoolToObject(health,"spo2_held",valid&&h.spo2Valid&&h.spo2Held);
      cJSON_AddNumberToObject(health,"hr_age_ms",h.hrValid?double(now-h.hrFreshMs):0);cJSON_AddNumberToObject(health,"spo2_age_ms",h.spo2Valid?double(now-h.spo2FreshMs):0);
      cJSON_AddNumberToObject(health,"quality",h.quality);
      if(valid&&h.hrValid) cJSON_AddNumberToObject(health,"hr_bpm",h.hr); else cJSON_AddNullToObject(health,"hr_bpm");
      if(valid&&h.spo2Valid) cJSON_AddNumberToObject(health,"spo2_pct",h.spo2); else cJSON_AddNullToObject(health,"spo2_pct");
      cJSON_AddNumberToObject(health,"sqi",fresh?h.sqi/100.0:0);
      cJSON_AddBoolToObject(health,"finger_detected",fresh&&h.finger); cJSON_AddStringToObject(health,"state",status);
      c.healthSeq=source.healthSeq; c.healthFresh=fresh;
    }
    if(!sendJson(c,j)) continue;
    if(source.ppgSeq!=c.ppgSeq) {
      c.ppgSeq=source.ppgSeq;
      if(source.ppgSeq && now-source.ppgMs<400) {
        auto* batch=object("ppg_batch",c,now);
        if(!batch) { drop(c); continue; }
        cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(batch,"ts"),timestamp(c,now)-double(now-source.ppgMs));
        cJSON_AddNumberToObject(batch,"sample_rate_hz",25);
        auto* values=cJSON_AddArrayToObject(batch,"samples");
        for(auto v:source.ppg) cJSON_AddItemToArray(values,cJSON_CreateNumber(v));
        sendJson(c,batch);
      }
    }
  }
  publishPending.store(false);
}
void publisherTask(void*) {
  for(;;) {
    if(server && CAREROVER_STAGE>=3) {
      if(!publishPending.exchange(true)) {
        if(httpd_queue_work(server,publish,nullptr)!=ESP_OK) { publishPending.store(false); ++publishDrops; }
      } else ++publishDrops;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void imuTask(void*) {
  Mpu6050Soft imu; bool ready=false; uint64_t retry=0,lastReport=0; TickType_t wake=xTaskGetTickCount();
  for(;;) {
    const auto now=wirelessNowMs();
    if(!ready&&now>=retry) { ready=imu.begin();retry=now+1000; }
    auto sample=imu.sample(now); if(!sample.valid) ready=false;
    portENTER_CRITICAL(&sourceMux);sources.imu=sample;portEXIT_CRITICAL(&sourceMux);
    portENTER_CRITICAL(&safetyMux);
    safety.imu(sample.valid,sample.calibrated,sample.tiltFault,now,sample.sampleMs);
    if(sample.valid&&!sample.held&&sample.calibrated&&!sample.tiltFault) safety.updateGestureTurn(sample.yaw,now);
    portEXIT_CRITICAL(&safetyMux);
    if(now-lastReport>=1000) {
      lastReport=now;
      Serial.printf("{\"type\":\"imu_status\",\"address\":\"0x%02X\",\"valid\":%s,\"calibrated\":%s,\"tilt_fault\":%s,\"yaw\":%.3f,\"pitch\":%.3f,\"roll\":%.3f,\"bias_dps\":[%.4f,%.4f,%.4f],\"accel_g\":[%.4f,%.4f,%.4f],\"gyro_dps\":[%.4f,%.4f,%.4f]}\n",imu.address(),sample.valid?"true":"false",sample.calibrated?"true":"false",sample.tiltFault?"true":"false",sample.yaw,sample.pitch,sample.roll,sample.biasX,sample.biasY,sample.biasZ,sample.ax,sample.ay,sample.az,sample.gx,sample.gy,sample.gz);
    }
    vTaskDelayUntil(&wake,pdMS_TO_TICKS(10));
  }
}
void frontTask(void*) {
  Hcsr04 sensor;
  if(!sensor.begin(frontInstallation)) { failSafe(true,"front_pins_invalid");vTaskDelete(nullptr);return; }
  TickType_t wake=xTaskGetTickCount();uint64_t lastReport=0;
  SeatNotificationGate notices;
  for(;;) {
    double cm=0;bool valid=false;
    if(sensor.service(micros(),cm,valid)) {
      const auto now=wirelessNowMs();
      SafetySnapshot state;
      portENTER_CRITICAL(&safetyMux);safety.frontSample(cm,valid,now);state=safety.snapshot(now);portEXIT_CRITICAL(&safetyMux);
      const auto event=notices.sample(state.mode==Mode::Idle&&!state.estop&&!state.fault,
        valid&&cm>=2&&cm<=400,state.seated,now);
      serverchanSeatEvent(event,now);
    }
    const auto now=wirelessNowMs();
    if(now-lastReport>=200) {
      lastReport=now;const auto state=readSafety(now);float yaw;
      portENTER_CRITICAL(&sourceMux);yaw=sources.imu.yaw;portEXIT_CRITICAL(&sourceMux);
      Serial.printf("{\"type\":\"front_status\",\"valid\":%s,\"distance_cm\":%.1f,\"age_ms\":%llu,\"phase\":\"%s\",\"status\":\"%s\",\"seated\":%s,\"stop_reason\":\"%s\",\"yaw_deg\":%.2f}\n",
        state.front.valid?"true":"false",state.front.valid?state.front.distanceCm:-1.0,(unsigned long long)state.front.ageMs,phaseName(state.front.phase),state.front.status,state.seated?"true":"false",state.stopReason,yaw);
    }
    vTaskDelayUntil(&wake,pdMS_TO_TICKS(1));
  }
}
void followTask(void*) {
  TickType_t wake=xTaskGetTickCount();
  for(;;) {
    const auto now=wirelessNowMs();
    portENTER_CRITICAL(&safetyMux);safety.computeFollow(now);portEXIT_CRITICAL(&safetyMux);
    vTaskDelayUntil(&wake,pdMS_TO_TICKS(100));
  }
}
void safetyTask(void*) {
  TickType_t wake=xTaskGetTickCount(); uint64_t last=wirelessNowMs();
  for(;;) {
    const auto now=wirelessNowMs(); const auto gap=uint32_t(now-last); last=now;
    if(gap>maxSafetyGapMs.load()) maxSafetyGapMs.store(gap);
    portENTER_CRITICAL(&safetyMux); safety.tick(now); auto state=safety.snapshot(now); portEXIT_CRITICAL(&safetyMux);
    if(driveReady.load()) {
      if(state.estop||state.fault||(!state.target.vx&&!state.target.vy&&!state.target.wz)) drive.stopNow();
      else drive.commandChassis(state.target.vx,state.target.vy,state.target.wz,240);
      drive.tick();
    }
    vTaskDelayUntil(&wake,pdMS_TO_TICKS(5));
  }
}
} // namespace

uint64_t wirelessNowMs() { return uint64_t(esp_timer_get_time())/1000; }
void wirelessGesture(const char* label,float score,bool accepted,bool actionEligible,bool held,uint64_t ageMs,uint32_t inferMs) {
  const auto now=wirelessNowMs();
  portENTER_CRITICAL(&safetyMux); safety.cameraPacket(now); portEXIT_CRITICAL(&safetyMux);
  char upper[24]; size_t n=0;
  for(;label[n] && n<sizeof(upper)-1;++n) upper[n]=toupper(static_cast<unsigned char>(label[n]));
  upper[n]=0;
  portENTER_CRITICAL(&sourceMux);
  memcpy(sources.label,upper,n+1); sources.score=score; sources.accepted=accepted;
  const bool usable = accepted && strcmp(upper,"NO_GESTURE") && strcmp(upper,"NO_HAND");
  if(usable) {
    memcpy(sources.displayLabel,upper,n+1);
    sources.displayScore=score;
    sources.displayAccepted=true;
    sources.displayGestureMs=now;
  } else if(sources.displayAccepted && now>=sources.displayGestureMs &&
            now-sources.displayGestureMs>=tuning::GestureDisplayHoldMs) {
    sources.displayAccepted=false;
    sources.displayLabel[0]='\0';
    sources.displayScore=0;
  }
  sources.gestureHeld=held;sources.gestureFreshMs=now>=ageMs?now-ageMs:0;
  sources.inferMs=inferMs; sources.gestureMs=now; ++sources.gestureSeq;
  portEXIT_CRITICAL(&sourceMux);
  // Display hysteresis may temporarily hold a label through detector gaps.  Only
  // fresh, directly supported stable frames are allowed to arm motion actions.
  if(CAREROVER_STAGE<4 || CAREROVER_INTEGRATION<3) return;
  const auto action=gestureActions.update(actionEligible,upper,now);
  if(action==GestureAction::None) return;
  const char* error=nullptr;
  if(action==GestureAction::StartFollow) {
    portENTER_CRITICAL(&safetyMux); error=safety.autonomousFollow(now); portEXIT_CRITICAL(&safetyMux);
    pendingGestureFollowUntil=error&&!strcmp(error,"TARGET_NOT_READY")?now+2000:0;
    pendingGestureStopSequence=readSafety(now).stopSequence;
  } else if(action==GestureAction::Stop) {
    pendingGestureFollowUntil=0;
    portENTER_CRITICAL(&safetyMux); safety.autonomousStop(now); portEXIT_CRITICAL(&safetyMux);
  } else {
    pendingGestureFollowUntil=0;
    ImuSample imu;
    portENTER_CRITICAL(&sourceMux); imu=sources.imu; portEXIT_CRITICAL(&sourceMux);
    if(!imu.valid||!imu.calibrated||imu.tiltFault) error="IMU_NOT_READY";
    else {
      portENTER_CRITICAL(&safetyMux);
      error=safety.autonomousTurn(action==GestureAction::TurnClockwise,imu.yaw,now);
      portEXIT_CRITICAL(&safetyMux);
    }
  }
  Serial.printf("{\"type\":\"gesture_action\",\"label\":\"%s\",\"ok\":%s,\"error\":\"%s\"}\n",upper,error?"false":"true",error?error:"");
}
void wirelessHealth(const WirelessHealth& h) {
  portENTER_CRITICAL(&sourceMux);
  const auto& old=sources.health;
  // Sample timestamp changes at 25 Hz but does not create a new health result.
  if(old.reportMs!=h.reportMs || old.ready!=h.ready || old.finger!=h.finger || old.hrValid!=h.hrValid || old.spo2Valid!=h.spo2Valid || old.hr!=h.hr || old.spo2!=h.spo2 || old.sqi!=h.sqi || strcmp(old.state,h.state)) ++sources.healthSeq;
  sources.health=h;
  portEXIT_CRITICAL(&sourceMux);
}
void wirelessPpg(uint32_t ir) {
  static uint64_t previousSampleMs=0;
  const auto sampleNow=wirelessNowMs();
  if(previousSampleMs && sampleNow-previousSampleMs>100) partialCount=0;
  previousSampleMs=sampleNow;
  partialPpg[partialCount++]=ir;
  if(partialCount==5) {
    const auto now=wirelessNowMs();
    portENTER_CRITICAL(&sourceMux); memcpy(sources.ppg,partialPpg,sizeof(partialPpg)); sources.ppgMs=now; ++sources.ppgSeq; portEXIT_CRITICAL(&sourceMux);
    partialCount=0;
  }
}
void wirelessStatus() {
  const auto now=wirelessNowMs(); const auto s=readSafety(now);
  Serial.printf("{\"type\":\"wireless_status\",\"firmware\":\"%s\",\"stage\":%d,\"ap\":%s,\"backend\":\"%s\",\"mode\":\"%s\",\"estop\":%s,\"fault\":%s,\"vx\":%.3f,\"vy\":%.3f,\"wz\":%.3f,\"last_cmd_ms\":%llu,\"stopped_at_ms\":%llu,\"stop_reason\":\"%s\",\"max_safety_gap_ms\":%u,\"min_heap\":%u,\"min_psram\":%u}\n",
    CAREROVER_BUILD_VERSION,CAREROVER_STAGE,apOnline.load()?"true":"false",CAREROVER_INTEGRATION?"tracking":"test_targets",s.estop?"ESTOP":s.fault?"FAULT":modeName(s.mode),s.estop?"true":"false",s.fault?"true":"false",s.target.vx,s.target.vy,s.target.wz,(unsigned long long)s.lastCommandMs,(unsigned long long)s.stoppedAtMs,s.stopReason,maxSafetyGapMs.load(),ESP.getMinFreeHeap(),ESP.getMinFreePsram());
}
void wirelessBegin() {
  frontInstallation=installedFront();
  if(frontInstallation.control.enabled&&!frontPinsReady(frontInstallation))frontInstallation.control.verified=false;
  safety.configureFront(frontInstallation.control);
  if(CAREROVER_INTEGRATION) {
    ServoCalibration calibration{};calibrationReady=loadMotionCalibration(calibration);
    safety.configureHardware(true,calibrationReady);
    if(CAREROVER_INTEGRATION>=2 && calibrationReady) {
      driveReady.store(drive.begin(MOTION_SERVO_PINS,calibration));
      if(!driveReady.load()) safety.fault(true,wirelessNowMs());
    }
    if(xTaskCreate(imuTask,"imu",4096,nullptr,3,nullptr)!=pdPASS) safety.fault(true,wirelessNowMs());
    if(CAREROVER_INTEGRATION>=3 && xTaskCreate(followTask,"follow",3072,nullptr,3,nullptr)!=pdPASS) safety.fault(true,wirelessNowMs());
  }
  if(frontInstallation.control.enabled&&frontPinsReady(frontInstallation)&&
      xTaskCreate(frontTask,"ultrasonic",3072,nullptr,2,nullptr)!=pdPASS)failSafe(true,"front_task_failed");
  if(xTaskCreate(safetyTask,"safety",3072,nullptr,4,nullptr)!=pdPASS) { failSafe(true,"task_failed"); Serial.println("{\"type\":\"wireless_error\",\"code\":\"SAFETY_TASK_FAILED\"}"); return; }
  WiFi.onEvent([](arduino_event_id_t event,arduino_event_info_t) {
    if(event==ARDUINO_EVENT_WIFI_AP_STOP) {
      apOnline.store(false); const auto now=wirelessNowMs();
      portENTER_CRITICAL(&safetyMux); safety.network(false,now); portEXIT_CRITICAL(&safetyMux);
    } else if(event==ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) { /* station disconnect no longer stops motion */ }
  });
  const char* password=CAREROVER_AP_PASSWORD;
  if(strlen(password)<8 || strlen(password)>63 || !strcmp(password,"REPLACE_WITH_PRIVATE_PASSWORD")) { failSafe(true,"invalid_password"); Serial.println("{\"type\":\"wireless_error\",\"code\":\"PRIVATE_PASSWORD_REQUIRED\"}"); return; }
  char ssid[24]; snprintf(ssid,sizeof(ssid),"CareRover-%04X",unsigned(ESP.getEfuseMac()&0xffff));
  WiFi.mode(serverchanConfigured()?WIFI_AP_STA:WIFI_AP);
  if(!WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0),IPAddress(192,168,4,3)) || !WiFi.softAP(ssid,password,1,0,4)) { failSafe(true,"ap_failed"); return; }
  apOnline.store(true); const auto now=wirelessNowMs();
  portENTER_CRITICAL(&safetyMux); safety.network(true,now); portEXIT_CRITICAL(&safetyMux);
  Serial.printf("{\"type\":\"ap_ready\",\"ssid\":\"%s\",\"ip\":\"192.168.4.1\"}\n",ssid);
  if(CAREROVER_STAGE<2) return;
  if(!FFat.begin(false)) { failSafe(true,"ffat_mount_failed"); Serial.println("{\"type\":\"wireless_error\",\"code\":\"FFAT_MOUNT_FAILED_NO_FORMAT\"}"); return; }
  if(!FFat.exists("/index.html")) { failSafe(true,"ffat_payload_missing"); Serial.println("{\"type\":\"wireless_error\",\"code\":\"FFAT_PAYLOAD_MISSING\"}"); return; }
  httpd_config_t config=HTTPD_DEFAULT_CONFIG();
  config.task_priority=1; // Do not outrank the existing Arduino sensor loop.
  config.stack_size=8192; config.max_open_sockets=7; config.lru_purge_enable=false;
  config.recv_wait_timeout=1; config.send_wait_timeout=1; config.close_fn=closeSocket;
  config.uri_match_fn=httpd_uri_match_wildcard;
  if(httpd_start(&server,&config)!=ESP_OK) { failSafe(true,"http_failed"); return; }
  if(CAREROVER_STAGE>=3) {
    httpd_uri_t ws{}; ws.uri="/ws"; ws.method=HTTP_GET; ws.handler=wsHandler; ws.is_websocket=true;
    if(httpd_register_uri_handler(server,&ws)!=ESP_OK) { failSafe(true,"ws_failed"); return; }
  }
  httpd_uri_t files{}; files.uri="/*"; files.method=HTTP_GET; files.handler=fileHandler;
  if(httpd_register_uri_handler(server,&files)!=ESP_OK) { failSafe(true,"http_routes_failed"); return; }
  if(CAREROVER_STAGE>=3 && xTaskCreate(publisherTask,"telemetry",3072,nullptr,1,nullptr)!=pdPASS) failSafe(true,"publisher_failed");
  serverchanBegin();
  wirelessStatus();
}

void wirelessVision(const carerover::VisionPacket& p,bool resync) {
  portENTER_CRITICAL(&safetyMux);
  if(resync) safety.cameraReset(p.receivedMs);
  safety.cameraPacket(p.receivedMs);
  if(p.kind=='P') {
    safety.person(p);
    if(pendingGestureFollowUntil) {
      if(p.receivedMs>pendingGestureFollowUntil || safety.snapshot(p.receivedMs).stopSequence!=pendingGestureStopSequence) pendingGestureFollowUntil=0;
      else if(p.found&&p.score>=SafetyController::PersonAcceptScoreMilli&&!safety.autonomousFollow(p.receivedMs)) pendingGestureFollowUntil=0;
    }
  }
  portEXIT_CRITICAL(&safetyMux);
  portENTER_CRITICAL(&sourceMux);
  if(resync)sources.displayTrack.reset();
  if(p.kind=='P') {
    sources.person=p;
    sources.personSeen=true;
    // Keep the UI track alive through a single invalid detector frame. The
    // filtered prediction is display-only; follow commands still use the raw
    // accepted source and its freshness gate.
    sources.displayTrack.updateDisplay(p);
  }
  if(!sources.fpsStart) sources.fpsStart=p.receivedMs;
  ++sources.resultCount;
  if(p.receivedMs-sources.fpsStart>=1000) {
    sources.aiFps=sources.resultCount*1000.0f/float(p.receivedMs-sources.fpsStart);
    sources.resultCount=0;sources.fpsStart=p.receivedMs;
  }
  portEXIT_CRITICAL(&sourceMux);
}

void wirelessDiagnostics() {
  static uint64_t reported=0; static uint32_t sequence=UINT32_MAX;
  const auto now=wirelessNowMs();const auto state=readSafety(now);
  if(state.stopSequence!=sequence || now-reported>=1000) {
    sequence=state.stopSequence;reported=now;wirelessStatus();
  }
}

carerover::FrontSnapshot wirelessFront() { return readSafety(wirelessNowMs()).front; }
const char* wirelessStopReason() { return readSafety(wirelessNowMs()).stopReason; }
