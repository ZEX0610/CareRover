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
3. `GET /` 返回 R5 远程控制台。`GET /stream` 是需要 Cookie 的 MJPEG；`wss://.../ws` 需要同一 Cookie 与精确的 HTTPS `Origin`。网页顶部电话按钮打开同源 `/call`，浏览器另需家长令牌来建立音频 WS，并须允许麦克风。
4. 车旁声音实时播放；按住“说话”时发送家长麦克风、松开后收听。现有网页做了 700 ms 半双工抑制和回声处理；家长端宜戴耳机。关闭通话弹窗会卸载 iframe，停止麦克风和音频 WS。

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
| `/audio` | WSS 二进制 | 子协议列表 `audio-v1` 与 `parent.<家长令牌>`，且同样要求精确 `Origin`；家长音频只能一条。它不使用 `/login` Cookie 作认证。 |
| `/css/*`, `/js/*`, `/client.js`, `/capture-worklet.js` | GET 静态文件 | 会话 Cookie；给现有网页使用。 |
| `/device/ws`, `/device/frame` | WS / POST | **设备侧** Bearer 令牌接口。App 不调用、不保存设备令牌。 |

原生 Swift 网络栈不会必然自动带浏览器式 `Origin`。接入 `/ws`、`/audio` 时必须按当前服务器要求设置准确 Origin；HTTP Cookie 也须从 `/login` 响应正确保存并随 `/ws`、`/stream` 发送。这一原生握手路径尚未在实机 App 验收，建议先通过 WebView 验证，然后添加自动化握手测试。服务器不提供通用注册/刷新令牌、REST 运动指令或 WebRTC 信令。

## 4. 控制与遥测

`/ws` 每条消息是 JSON 文本。家长可发的中继白名单目前只有 `cmd_vel`、`set_mode`、`estop`、`clear_estop`、`ping`；主板其他内部命令或文档中的 `set_demo_bypass` **目前不会经云中继转发**。示例：

```json
{"type":"set_mode","ts":1788940000000,"mode":"MANUAL","request_id":1}
{"type":"cmd_vel","ts":1788940000100,"vx":0.3,"vy":0,"wz":0}
{"type":"cmd_vel","ts":1788940000150,"vx":0,"vy":0,"wz":0}
{"type":"estop","ts":1788940000200}
```

`vx` 正=前、`vy` 正=右、`wz` 正=顺时针，三项均在 [-1,1]。摇杆只是目标量，不代表已实测车速；四轮 PWM 由主板安全仲裁和布局代码负责。非零手动命令只在主板确认 MANUAL、持有控制权、链路/IMU/前方条件满足时有效。运行中推荐约 20 Hz 续发目标；松手、页面退后台、网络断开或模式变化时立刻发零速度，主板仍有约 240 ms 输出租约作为兜底。请求模式不等于模式已切换，必须等待 ACK 和新鲜 telemetry。急停优先，不能用摇杆解除。

从 `/ws` 接收的类型为 `telemetry`、`ppg`、`ppg_batch`、`ack`、`error`、`pong`。重要字段示意（**只示结构，不是实测值**）：

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
