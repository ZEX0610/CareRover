# iPhone 客户端接入契约（当前实现，2026-09-27）

此处写的是**已实现的私有云中继接口**，不是规划中的 API。权威实现分别在 `../relay/server.mjs`、`../relay/public/client.js`、`../gateway/gateway.mjs`；主板 JSON 的字段、单位与新鲜度见 `../../main-web/docs/protocol.md` 和 `../../main-web/js/protocol.js`。任何示例都不含真实令牌。

## 1. 网络与身份

- 当前 HTTPS origin：`https://carerover-relay.tail86bfa5.ts.net`；WebSocket 使用 `wss://` 同主机。**不要用旧的 `taild6125f.ts.net`，不要在远程 App 请求 `192.168.4.1/.2`。**
- Mac/iPhone 安装 Tailscale 并激活与云服务器相同的 `new20070610` tailnet；此站点由 Tailscale Serve 私有发布，未开启 Funnel。Mac/iPhone 独立上网，车旁 Windows 需保持两路网络和网关程序运行。
- 家长有一枚独立随机 `AUDIO_PARENT_TOKEN`。请经密码管理器分发并放入 iOS Keychain；不要写入源码、App 包、URL、日志、截图、GitHub 或查询参数。设备专用 `AUDIO_DEVICE_TOKEN` 只在服务器与 Windows 网关使用，**绝不给家长 App**。
- `/health` 是无凭据的链路诊断，不是身份或授权证明。`controlDevice`、`videoFresh`、`audioPaired` 分别表示设备控制 WS、近 2 秒收到视频、车端/家长端音频 WS 同时在线；不能把它们单独解释为儿童在场或真实心率。

## 2. 浏览器/WebView 路径（推荐首个可用版本）

1. 在 Safari/WKWebView 中打开上述 HTTPS origin；`GET /` 未登录时 303 到 `/login`。
2. `POST /login`：表单编码 `token=<家长令牌>`。成功 303 回 `/?transport=ws&video=mjpeg`，响应设置 `cr_session` HttpOnly、Secure、SameSite=Strict、8 小时 Cookie；失败 403。不要尝试用 Tailscale 登录代替此令牌。
3. `GET /` 返回 R5 远程控制台。`GET /stream` 是需要 Cookie 的 MJPEG；`wss://.../ws` 需要同一 Cookie 与精确的 HTTPS `Origin`。网页顶部电话按钮在**当前顶层页面**显示通话控件，不再使用 iframe；独立 `GET /call` 仍可用。已登录的浏览器可用同一会话 Cookie 建立音频 WS，无需重复输入家长令牌，并须允许麦克风。旧版 `parent.<令牌>` 子协议仍兼容。
4. 车旁声音实时播放；按住“说话”时发送家长麦克风、松开后收听。现有网页做了 700 ms 半双工抑制和回声处理；家长端宜戴耳机。关闭 R5 通话控件会停止麦克风和音频 WS。

浏览器安全上下文要求 HTTPS；局域网的 `http://192.168.4.1` 页面不能直接调用浏览器麦克风。局域网 R5 页面顶部电话按钮因此只说明去私有 HTTPS 家长端使用。

## 3. 原生客户端的当前 HTTP/WSS 表

| 路径 | 方法/内容 | 鉴权与实际用途 |
| --- | --- | --- |
| `/health` | GET JSON | 无凭据，只读链路状态。可用于诊断，不要用作业务在线唯一依据。 |
| `/login` | POST `application/x-www-form-urlencoded`，字段 `token` | 家长令牌换 8 小时 `cr_session` Cookie；成功 303，错误 403。 |
| `/` | GET HTML | 会话 Cookie；R5 远程控制台。 |
| `/call` | GET HTML | 会话 Cookie；双向通话页面。 |
| `/stream` | GET `multipart/x-mixed-replace; boundary=frame` | 会话 Cookie；每 part 为 `Content-Type: image/jpeg` + `Content-Length` + JPEG。320×240 源图，远程实际 FPS 以现场网络为准。 |
| `/ws` | WSS 文本 JSON | 会话 Cookie + 精确 `Origin: https://carerover-relay.tail86bfa5.ts.net`；接收主板遥测、发送控制。当前只允许一位家长控制连接，设备离线时拒绝升级。 |
| `/events` | WSS 文本 JSON，只读 | 原生 App 用 `Authorization: Bearer <AUDIO_PARENT_TOKEN>` 且**不发送 Origin**；网页登录页可用会话 Cookie + 精确 HTTPS Origin。可与网页控制连接并存，最多 8 位订阅者。不得向此连接发送指令。 |
| `/audio` | WSS 二进制 | 已登录家长可用 `cr_session` Cookie + 子协议 `audio-v1` + 精确 `Origin`；也兼容 `audio-v1` 与 `parent.<家长令牌>` 的旧方式。家长音频只能一条。设备仍使用独立 Bearer 令牌。 |
| `/css/*`, `/js/*`, `/client.js`, `/capture-worklet.js` | GET 静态文件 | 会话 Cookie；给现有网页使用。 |
| `/device/ws`, `/device/frame` | WS / POST | **设备侧** Bearer 令牌接口。App 不调用、不保存设备令牌。 |

原生 Swift 网络栈不会必然自动带浏览器式 `Origin`。接入 `/ws`、`/audio` 时必须按当前服务器要求设置准确 Origin；HTTP Cookie 也须从 `/login` 响应正确保存并随 `/ws`、`/audio`、`/stream` 发送。这一原生握手路径尚未在实机 App 验收，建议先通过 WebView 验证，然后添加自动化握手测试。服务器不提供通用注册/刷新令牌、REST 运动指令或 WebRTC 信令。

### App 顶部提醒与来电铃声（2026-09-29 新增契约）

原生 App 另开一条只读 `wss://carerover-relay.tail86bfa5.ts.net/events`，在 WebSocket 握手中设置 `Authorization: Bearer <AUDIO_PARENT_TOKEN>`，不要把令牌放 URL、子协议、日志或仓库。`/events` 不占用唯一的 `/ws` 控制席位；设备暂时离线时仍可连接，但历史事件**不持久化**。连接成功先收到一次 `care_state` 快照，随后按发生顺序接收 `care_event`：

```json
{"type":"care_state","camera_online":true,"face_alert_active":false,"seat":"seated","call_requested":false,"call_sequence":4,"audio_paired":false,"updated_at":"2026-09-29T08:00:00.000Z"}
{"type":"care_event","event_id":"1790668800000-5","occurred_at":"2026-09-29T08:00:00.000Z","kind":"face_absent","active":true,"title":"30 秒未识别到人脸","body":"请查看实时画面确认孩子情况。","sound":"none","requires_confirmation":false,"source":"carerover-relay"}
{"type":"care_event","event_id":"1790668810000-6","occurred_at":"2026-09-29T08:00:10.000Z","kind":"face_restored","active":false,"title":"重新识别到人脸","body":"无人脸提醒已解除。","sound":"none","requires_confirmation":false,"source":"carerover-relay"}
{"type":"care_event","event_id":"1790668820000-7","occurred_at":"2026-09-29T08:00:20.000Z","kind":"seat_left","active":true,"title":"离座提醒","body":"前方距离变化提示可能离座，请查看画面确认。","sound":"none","requires_confirmation":false,"source":"carerover-relay"}
{"type":"care_event","event_id":"1790668830000-8","occurred_at":"2026-09-29T08:00:30.000Z","kind":"call_invite","active":true,"sequence":5,"title":"孩子请求通话","body":"请确认是否接听。","sound":"ring","requires_confirmation":true,"source":"carerover-relay"}
```

`kind` 完整集合：`face_absent` / `face_restored`、`seat_left` / `seat_returned`、`call_invite` / `call_cancelled`、`call_connected` / `call_ended`。App 用 `event_id` 去重；用 `kind` 和 `active` 驱动顶部条，不依赖可翻译的 `title` 做判断。`face_absent` 顶部条保持到 `face_restored`；`seat_left` 顶部条保持到 `seat_returned`，入座消息短暂显示；`call_invite` 顶部条带“接听”按钮并循环播放本机铃声，直到用户点接听、收到 `call_cancelled` 或 `call_connected`。点“接听”后按第 5 节建立 `/audio`；`call_connected` 只表示两端音频 WebSocket 已配对，不等于麦克风权限或音质已通过。`call_cancelled` 表示再次有效的 CALL 手势取消请求；`call_ended` 表示一端音频断开，两者不能混淆。

CALL 和 FIVE 都必须先被稳定识别，然后手势离开/不再分类为该手势，再次稳定识别才会触发第二次；持续保持或同一手势短暂低置信度不会连发。CALL 第一次为请求，第二次为取消；FIVE 第一次进入手表模式，第二次退出。每个 CALL 请求有递增 `sequence`；同一序号的遥测重发不能重复响铃。

无人脸提醒的含义只是“CAM 在线并持续 30 秒未检测到新的人脸”，**不是儿童身份识别或离座证明**。CAM 离线时不启动 30 秒计时，也不会假称重新识别；已产生的提醒保留到真实人脸再次出现。离座/入座来自 IDLE 时有效的前方超声测距，稳定约 800 ms 后上报；通话期间超声暂停时不产生新离座判断。App 应将两种提醒分开展示。`care_state.seat="unknown"` 表示尚无可靠基线。

这个接口是**已连接 App 的实时事件流**，不是 APNs。App 在后台、被系统挂起、断网或未启动时，服务器目前不能保证弹窗或铃声；若要离线推送，需要另行建设 APNs 设备令牌、推送凭据、队列及隐私/权限流程。重连时用 `care_state` 恢复当前持续状态，不把它误当新的离座事件；若 `call_requested=true` 且 `audio_paired=false`，可恢复未接来电提示。Xcode 项目在另一台 Mac，仓库未包含 App 源码，故 App 的界面、本机铃声和通知权限仍须由 App 开发者接入验收。

## 4. 控制与遥测

`/ws` 每条消息是 JSON 文本。家长可发的中继白名单目前只有 `cmd_vel`、`set_mode`、`estop`、`clear_estop`、`ping`；主板其他内部命令或文档中的 `set_demo_bypass` **目前不会经云中继转发**。示例：

```json
{"type":"set_mode","ts":1788940000000,"mode":"MANUAL","request_id":1}
{"type":"cmd_vel","ts":1788940000100,"vx":0.3,"vy":0,"wz":0}
{"type":"cmd_vel","ts":1788940000150,"vx":0,"vy":0,"wz":0}
{"type":"estop","ts":1788940000200}
```

`vx` 正=前、`vy` 正=右、`wz` 正=顺时针，三项均在 [-1,1]。摇杆只是目标量，不代表已实测车速；四轮 PWM 由主板安全仲裁和布局代码负责。非零手动命令只在主板确认 MANUAL、持有控制权、链路/IMU/前方条件满足时有效。运行中推荐约 20 Hz 续发目标；松手、页面退后台、网络断开或模式变化时立刻发零速度，主板仍有约 240 ms 输出租约作为兜底。请求模式不等于模式已切换，必须等待 ACK 和新鲜 telemetry。急停优先，不能用摇杆解除。

从 `/ws` 接收的类型为 `telemetry`、`ppg`、`ppg_batch`、`ack`、`error`、`pong`，网页还会收到 `care_state` / `care_event`。重要字段示意（**只示结构，不是实测值**）：

```json
{
  "type":"telemetry","ts":1788940000000,
  "connection":{"camera":true,"main_mcu":true},
  "robot":{"mode":"IDLE","state":"IDLE","estop":false,"control_allowed":true},
  "vision":{"image_width":320,"image_height":240,
    "person":{"found":true,"x":100,"y":30,"w":80,"h":160,"confidence":0.7,"seq":42,"predicted":false},
    "gesture":{"label":"ONE","confidence":0.7,"stable":true}},
  "health":{"hr_bpm":76,"spo2_pct":97,"finger_detected":true},
  "imu":{"valid":true,"yaw_deg":1,"pitch_deg":0,"roll_deg":0},
  "front":{"valid":true,"distance_cm":82,"status":"CLEAR"}
}
```

块可省略；`null` 是明确清除，数值 `0` 仍有效。App 必须根据各块独立年龄/有效性显示“无结果/过期”，不能用一次旧数据永久保持“在线”。人物框以 320×240 源坐标映射到实际视频 **contain** 区域，不能把 letterbox 边框算进坐标；`predicted=true` 的框只用于显示，不能当作新检测或运动授权。远程遥测中的 `video.stream_url` 可能是 `http://192.168.4.2/stream`，**必须忽略**，远程一律使用同源 `/stream`。完整有效值、保持与新鲜度规则以 `main-web/docs/protocol.md`、`js/protocol.js`、`js/state.js` 为准。

控制 WS 断开时服务端会向设备发零速度，主板自身也会停机；App 仍应主动释放本地输入并清空未确认状态。中继不会替代小车的独立 watchdog 或急停。远程运动目前尚未实物验收，接入 App 初期仅做只读视频/遥测和零速度/急停测试。

## 5. 双向音频二进制

音频不是 WebRTC、RTSP、MP3 或 AAC，而是当前 Demo 的 `wss://.../audio` PCM16LE 帧。每帧严格 648 字节：偏移 0–1 为 ASCII `CR`，偏移 2 为版本 `1`，偏移 3 为类型 `1`，偏移 4–7 是 little-endian `uint32` 递增序号，偏移 8–647 是 320 个 little-endian `int16` 单声道样本（16 kHz，20 ms）。无压缩、无文件录制；服务器对错误长度/头关闭连接，并限制每秒帧数及缓冲。浏览器端采集工作线程、封包与播放参考 `../relay/public/capture-worklet.js` 和 `../relay/public/client.js`。

家长端目前采用按住说话：上行帧只在按住时发送，播放车端音频在说话期间及松开后 700 ms 暂停；Windows 网关和主板也做相应回声抑制。原生实现需有 16 kHz 重采样、20 ms 分帧、小抖动缓冲、播放限幅、耳机/扬声器切换和离开页面后释放音频会话；与浏览器的音质不能直接等同，须在 iPhone 实测。云服务不做自动语音识别、LLM 回复或情绪判断。

## 6. 连接状态与验收顺序

1. 先用无凭据 `/health` 验证 `controlDevice=true`、`videoFresh=true`；若为 false，排查车旁 Windows 双网络、网关和 CAM 单观看者，不先改 App 视频控件。
2. 登录后读取 `/stream` 的连续 JPEG 和 `/ws` 的新鲜 telemetry；观察人物框、手势、HR/SpO₂/IMU。网页可能有视频但音频尚未配对，属于独立状态。
3. 家长端戴耳机，做车端→家长、家长按住说话→车端两次短句；确认没有持续啸叫、视频/遥测仍更新。需要比较延迟、断网恢复和后台行为。
4. 运动控制要在舵机断电、四轮架空、IDLE 条件下从只读/零速度开始，再由现场操作者许可逐步上电验证。不要以模拟页面或 CI PASS 代替实物运动验收。

若家长手机同时也是车旁 Windows 的 USB 上网来源，手机离开后网关会失联。需要给车旁 Windows 另备持续互联网。App 当前只应显示这些实际状态，不要把截图中的“模拟在线”“演示画面”文案直接换成“实时在线”。
