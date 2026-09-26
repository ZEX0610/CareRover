> 本分支现已提供 `firmware/main_wireless` 的无线实现及测试目标状态机；尚待 Windows 原配置确认与真机验收。以下原架构描述与实现细节以 [无线开发说明](wireless-development.md) 的当前状态为准。

# 架构与硬件对接

```text
HTML / CSS
   │
app.js ─── joystick.js（输入与释放）
   ├───── video.js + video-overlay.js（图像与独立 metadata）
   ├───── telemetry.js（PPG Canvas）
   ├───── debug.js（有界日志、录制与回放）
   │
protocol.js（JSON 校验） ↔ state.js（状态、许可、新鲜度、环形缓冲）
   │
Transport
   ├─ MockTransport → RobotSim（浏览器内、纯本地）
   └─ WebSocketTransport → /ws（Python Mock、USB 开发桥或最终主控）

视频：img → /stream（独立 MJPEG HTTP 连接）
```

应用层看见统一的 connect / disconnect / send / onMessage / onStateChange 接口。视频合成器从标准 vision 元数据画场景，不读取 MockTransport 私有对象。WebSocket 模式不会下载 JS 模拟器。不存在 CDN、远程分析、云端 telemetry 或外网媒体资源。

PPG 50 Hz 与绘图频率解耦；rAF 绘制上限约 60 FPS，DOM 最多 10 Hz 更新。环形缓冲固定 4096 样本，日志 120 行，录制最多 20000 个入站消息。录制文件上限 32 MiB，回放最多一小时，结束后明确返回实时。录制只包含 JSON 数据；回放时从 metadata 重新合成画面，不录制或上传视频。关闭页面清理订阅、定时器、视频 URL 和 WebSocket。

三个独立状态不能混淆：transport 链路状态、机器人确认的 mode、UI 请求中的 mode。急停本地锁与机器人 estop 分开保存，防止网络延迟导致 UI 误解锁。

浏览器内仿真与 Python 仿真共享协议、轴约定、模式权限、急停 / 恢复和 250 ms watchdog；两者具体随机数、房间绘制及数值轨迹并不逐帧相同。Python 的物理步进与广播任务分开，慢客户端不能通过广播阻塞正常的超时判断。

## 视频坐标

源图像 iw × ih，容器 W × H。`scale=min(W/iw,H/ih)`，内容偏移 `ox=(W-iw*scale)/2`、`oy=(H-ih*scale)/2`，点映射到 `(ox+x*scale, oy+y*scale)`。Canvas 位图按 DPR 放大（上限 2），绘制仍用 CSS 像素。摄像头 MJPEG 的宽高比必须与 metadata 声明的源图像一致。

本地视频用于测试 `<video>` 播放和 overlay，不能产生真实检测结果。来自当前数据源的 bbox 会按归一化位置映射到视频的实际 contain 区域，但任意视频与独立 metadata 不保证语义对应。视频输入失败不静默冒充真实画面。Python MJPEG 640×480 与 metadata 320×240 使用相同 4:3 内容和成比例坐标。

## 部署范围

运行前端只需要静态服务器，无需 Node。Python 服务只用于本地开发验证，不烧录进 ESP32。GitHub Actions 执行逻辑及网络集成测试；浏览器视觉与真实硬件接入分别验收。

## 最终无线硬件集成契约

主 ESP32-S3 作为唯一网页网关，建立 CareRover 本地 Wi-Fi 热点并至少提供：

```text
GET /
WS  /ws         双向 JSON
```

浏览器或手机连接该热点后访问 `http://192.168.4.1/`。页面和 `/ws` 同源，因此整机运行不依赖 USB、电脑串口桥、互联网或 GitHub。

Browser → Main：`cmd_vel`, `set_mode`, `estop`, `clear_estop`, `ping`。

Main → Browser：`telemetry`, `ppg_batch`, `ack`, `error`, `pong`。

CAM → Main：沿用当前 115200 baud UART JSON 手势结果。Main 同时采集 MAX30102、未来六轴传感器并控制舵机/全向轮，在本机聚合最新状态后发布到网页。

主控执行运动、安全裁决和传感器采样，必须独立处理 >250 ms 超时停车、急停锁定和多客户端控制权。网页只发归一化速度目标，不知道 UART、轮子逆运动学或 PWM。

后续如需要真实相机画面，CAM 可加入同一 CareRover Wi-Fi，单独提供 MJPEG `GET /stream`；手势和控制数据仍以 Main 的 `/ws` 为准。视频链路缺失不得影响急停、运动 watchdog 或健康数据。

`hardware/serial_bridge.py` 只用于 USB 开发诊断，不属于最终部署链路。
