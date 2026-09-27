# CareRover App 与网页接入入口（2026-09-27）

此仓库是当前两板、车旁 Windows 网关、私有云中继和两套网页的源码快照；不是 iPhone App 的源码。截图中 iPhone 页面显示“模拟在线 / 演示画面”，不能视为已接入实车。此文档供另一台电脑或 App Agent 从零接手，**不包含任何运行凭据**。

## 先看哪一部分

| 目标 | 当前源码与契约 |
| --- | --- |
| iPhone 原生 App / PWA 接入远程视频、遥测、控制、语音 | [`remote-hub/docs/IOS_CLIENT_API.md`](remote-hub/docs/IOS_CLIENT_API.md)；服务器实现 `remote-hub/relay/server.mjs`；网页家长端 `remote-hub/relay/public/` |
| 局域网控制台与主板协议 | `main-web/index.html`、`main-web/js/`、[`main-web/docs/protocol.md`](main-web/docs/protocol.md)；主板 `main-web/firmware/main_wireless/` |
| CAM 图像/人物/手势 | `main-web/firmware/cam_tracking/`；经主板 telemetry 的框、手势和 CAM 独立 MJPEG `/stream` |
| 麦克风、功放与云端通话 | `main-web/firmware/main_wireless/audio_gateway.*`、`remote-hub/gateway/`、`remote-hub/relay/public/client.js` |
| 实物状态、版本和未验收项 | [`main-web/docs/windows-progress.md`](main-web/docs/windows-progress.md)、[`remote-hub/deploy/REMOTE_SERVER_PROGRESS_2026-09-26.md`](remote-hub/deploy/REMOTE_SERVER_PROGRESS_2026-09-26.md) |

## 目前实际拓扑

车旁 ESP32-S3 主板开 `CareRover-EE68` AP；CAM 在 AP 中提供 MJPEG。车旁 Windows 通过 Wi-Fi 连 AP，另由 USB/有线上网，运行 `remote-hub/gateway/start-windows.ps1 -EnableAudio`，将 `/ws`、CAM 视频和 `/audio` 转发到云中继。服务器 `carerover-relay.tail86bfa5.ts.net` 以 Tailscale Serve 提供私有 HTTPS/WSS；家长 Mac/iPhone 须加入 **new20070610** tailnet，使用独立互联网，不连接小车 AP。当前没有 ESP32 直接到云端、没有公网 Funnel、没有云端 AI 对话服务。

本轮 R5 UI 已合并到 `main-web/` 和 `remote-hub/relay/public/`。`main-web` 为 FFat 局域网页源；`remote-hub` 为云端页面源，其电话按钮在顶层页面打开真实通话控件，独立 `/call` 也可用。源文件合并不自动代表实机 FFat 或云服务器已部署，是否已部署看上述现场进度，不要凭 Git 提交推断。

## App 的最短接入路径

1. 用现有私有 HTTPS 地址加载云端控制台，先走 WebView/Safari 完成真实视频、遥测和按住说话验收；此路径复用已运行的认证、WebSocket、MJPEG 与音频实现。
2. 如要改成 Swift/SwiftUI 原生控件，按 [`IOS_CLIENT_API.md`](remote-hub/docs/IOS_CLIENT_API.md) 分别实现会话、MJPEG、控制 WS 和 PCM 音频 WS；连接时仍须使用当前服务器端鉴权和控制权语义。不要把家长令牌硬编码进 App 或仓库。
3. 演示画面与模拟健康值必须显著标注模拟；只有收到真实设备的新鲜 telemetry 和视频帧后才显示“在线/实时”。

## 当前限制

- 云端只允许一位家长控制 WS 和一位家长音频 WS；多家长账户、邀请、推送、录音、对话式 AI 与情绪识别均未实现。
- CAM 只允许一个直接 MJPEG 观看者，远程模式由 Windows 网关占用；此时本地直接 `/stream` 可能返回 `503 Video viewer busy`。远程服务器可把已上传的单路帧分发给多位已登录观看者，但控制权仍单一。
- 当前主板为临时 AP-only audio2 应用，Server酱微信推送暂停；舵机断电期间不能将远程摇杆 UI 视为实车运动验收。
- 代码、Mock/CI PASS 以及同机回环测试不等于异地 Mac/iPhone 的音视频和运动验收。
