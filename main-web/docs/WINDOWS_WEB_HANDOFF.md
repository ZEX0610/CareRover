# CareRover 网页优化/重设计 交接文档

> 交接对象：负责网页优化与重设计的 agent。
> 更新时间：2026-09-17。固件版本 `6a29b278dd82b6f6-s5-follow`（主控 DEMO_BALANCED stage5-follow）。

## 1. 项目一句话

CareRover 是一台 ESP32-S3 全向轮机器人，主控托管网页 + WebSocket，CAM 板做人脸/手势推理并回传；网页是唯一的人机界面（手动驾驶、跟随、手势、健康测量、急停、前方障碍）。

## 2. 网页在哪里、怎么跑

- 网页由**主控 ESP32-S3** 从 FFat 分区（`webroot`）通过 HTTP 提供，地址 `http://192.168.4.1`（主控开热点 `CareRover-EE68`，密码见本地 `firmware/main_wireless/wifi_secrets.h`，不入库）。
- 源码在仓库根：`index.html` + `js/` + `css/`。**不是独立前端工程**，是随主控固件打包进 `ffat.bin` 的静态资源。
- 本地浏览器调试（无硬件）：`python3 -m http.server 8080 --bind 127.0.0.1` 后打开 `http://127.0.0.1:8080/?transport=mock`；或用 `.venv/Scripts/python.exe mock/server.py` 后打开 `http://127.0.0.1:8080/?transport=ws&video=canvas`。
- 视频：主控透传 CAM 的 MJPEG（`/stream`），`video.js` + `video-overlay.js` 负责显示与框/手势叠加。

## 3. 文件地图

| 文件 | 职责 |
|---|---|
| `index.html` | 页面骨架 + 各面板容器 + 弹窗 |
| `css/app.css` | 全部样式（单文件，约 20 KB） |
| `js/config.js` | 全局可调参数（`CMD_VEL_HZ`、`DEADMAN_TIMEOUT_MS`、`WS_RECONNECT_*`、`MODES` 等） |
| `js/protocol.js` | 消息编解码/校验（`OUT_TYPES`、`cmdVel`、`setMode`、`estop`…） |
| `js/transport.js` | WebSocket 传输 + 重连 |
| `js/state.js` | 中心状态 store（`applyTelemetry`、`setJoystick`、estop 本地锁存等） |
| `js/app.js` | 主入口：命令发送循环、模式切换、急停、ping、telemetry 路由、UI 渲染 |
| `js/joystick.js` | 全向摇杆输入（`MotionInput`、`joystickVector`） |
| `js/sim.js` | 浏览器内仿真器（`?transport=mock` 用） |
| `js/mock-transport.js` | Mock 传输层 |
| `js/front-panel.js` + `js/front-sim.js` | 前方障碍面板 + 绕障模拟 |
| `js/video.js` + `js/video-overlay.js` | 视频 + 人脸框/手势叠加 |
| `js/telemetry.js` | 遥测新鲜度/健康状态 |
| `js/debug.js` | 调试日志环形缓冲 |
| `js/i18n.js` | 中英双语字符串 |

## 4. 协议（权威文档 `docs/protocol.md`）

- 传输：WebSocket（`/ws`）+ MJPEG 视频（`/stream`）。
- 出站命令：`cmd_vel`、`set_mode`、`estop`、`clear_estop`、`ping`、`set_demo_bypass`。
- 入站：`telemetry`（机器人状态/视觉/IMU/健康/手势/前方/视频 URL）、`ack`、`error`、`pong`、`ppg_batch`。
- 错误码：`ESTOP_ACTIVE`、`NOT_IN_MANUAL`、`INVALID_COMMAND`、`INVALID_MODE`、`UNKNOWN_TYPE`、`INVALID_JSON`、`CONTROL_BUSY`、`FAULT_ACTIVE`、`FRONT_RELEASE_REQUIRED`、`CAMERA_OFFLINE`、`IMU_NOT_READY`、`TARGET_NOT_READY` 等。
- **命令死限**：网页以 20 Hz 发 `cmd_vel`（`config.js::CMD_VEL_HZ`）；主控侧命令看门狗已按现场要求移除，但网页仍保留 `DEADMAN_TIMEOUT_MS` 作为 UI 侧兜底。

## 5. 主控当前行为（网页要对接的关键状态）

- 模式：`IDLE` / `MANUAL` / `PERSON_FOLLOW` / `GESTURE_CONTROL` / `HEALTH_CHECK`；系统态 `ESTOP` / `FAULT`。
- 跟随：`BoxTrack` 常速度卡尔曼 + **300 ms 前瞻预测**，**转向与前后解耦**（先原地转正，再前后），`maxVx=0.15`/`maxWz=0.20`。
- 手势：`LIKE` 开跟随、`DISLIKE` 停、`TWO` 顺时针 360°、`THREE`/`OK` 逆时针 360°。
- 安全：急停 + 硬件故障 + **倾角保护已恢复**（>55° 持续 400 ms 停车）；`watchdog`/IMU 新鲜度/相机新鲜度/断网/断连等自动停车**已去除**。
- 前方超声（HC-SR04）：前方障碍保护 + 固定向右绕障（demo bypass），详见 `docs/wiring-current.md` 与 `docs/ultrasonic-development.md`。

## 6. 网页优化/重设计注意点

1. **体积受限**：网页要打进 FFat（约 10 MB 分区）。新增图片/字体/依赖前先看 `tools/carerover.py` 的 `runtime_files()` 打包清单（`index.html` + `js`/`css`/`assets`）。
2. **`web_version` 指纹**：`tools/carerover.py payload` 会用 `content_id`（文件内容哈希）生成 `<meta name="carerover-web-version">` 并写入 `version.json`；改网页后需重新构建主控才生效。
3. **移动端优先**：现场是手机连热点操作，摇杆、按钮、急停必须触屏友好；切后台会节流 `setInterval`，命令循环要防断流。
4. **双语**：UI 字符串走 `js/i18n.js`，别散落在各处硬编码。
5. **回归**：改完跑 `npm test`（`node --test tests/*.test.js`，当前 21/21）；网页改动不烧板时用 `?transport=mock` 自测。

## 7. 仍未完成（本交接的优化目标）

- 网页视觉/交互重设计与性能优化（本次交接的核心目标）。
- 真实超声接入后的前方面板/绕障 UI 实测（硬件标定见 `ultrasonic-development.md`，全部 NOT RUN）。

## 8. 需要现场/用户配合

- 网页改动要上板：重新构建主控（`tools/carerover.py build ...`）+ 刷 COM6；构建/刷写流程见 `docs/windows-progress.md`。
- 本机 GitHub 推送当前被账号权限阻塞（登录账号 `ZEX0610` 无 `NeedleAss/spider-webite-dev` 写权限），见仓库历史 `docs/windows-progress.md` 的同步记录。
