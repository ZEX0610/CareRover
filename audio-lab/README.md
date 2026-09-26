# CareRover 独立音频实验工程（2026-09-25）

本目录**没有并入** `C:\CareRover\firmware` 或现有网页，也没有烧录主板/CAM。现有小车 AP+STA、约 6 FPS 视频、人物/手势、摇杆、HC-SR04/Server酱功能保持原状。原型目标是先验证 INMP441 + MAX98357 + 3 W/8 Ω 扬声器的双向语音链路，再决定如何集成。

## 当前成果与证据边界

| 阶段 | 状态 | 已有证据 / 尚缺 |
|---|---|---|
| 原理图、资料与空闲 GPIO 对照 | PASS（文档核对） | [接线与分阶段测试](docs/WIRING_AND_TEST.md)；实物丝印尚需核对 |
| 16 kHz I²S 麦克风/功放独立草图 | PASS（编译） | Arduino-ESP32 3.3.10：程序 1,020,347 B（32%）、全局 46,504 B（14%）；**未烧录、未测实际声音** |
| 音频帧格式主机测试 | PASS | `tests/audio_frame_test.cpp` |
| HTTPS/WSS 中继双向转发与鉴权 | PASS（本机模拟） | `relay/`，Node 5/5 测试，包括双向转发、凭据/Origin/重复设备、按节奏发送 100 帧、48 kHz→16 kHz 工作线程及本机自签证书 WSS；`ws@8.21.3` 审计 0 漏洞；**未部署公网、TLS 公网链路未测** |
| 家长网页麦克风、按住说话、播放 | NOT RUN（浏览器/手机实测） | `relay/public/`；静态检查不等于音质验收 |
| 真实跨公网通话、与小车全部功能并发 | NOT RUN | 需要实物、独立热点、HTTPS 域名和长时间压测 |
| 离线语音命令或自由对话 | 方案阶段 | [整体架构与语音控制](docs/ARCHITECTURE_AND_VOICE.md) |

远程通话的最短链路为：**小车主板通过独立热点 STA → 公网 HTTPS/WSS 中继 ← 家长的 HTTPS 页面**。iPad 仍可连小车 AP 使用原控制台。新电源模块只改善供电余量，**不会提升 CPU、Wi-Fi 空口或 HTTPS 内存余量**；这些必须在未来整机并发试验中量化。

当前网页 `http://192.168.4.1` 不能直接打开浏览器麦克风：`getUserMedia()` 只在可信安全上下文（例如 HTTPS 或本机 localhost）可用。这是浏览器限制，不是 GPIO/固件问题。实验页单独由 HTTPS 中继托管，不修改原网页。[MDN 原始说明](https://developer.mozilla.org/en-US/docs/Web/API/MediaDevices/getUserMedia)

## 文件

- `firmware/audio_lab/audio_lab.ino`：独立 Arduino IDE 草图，串口 `mic / tone / loop / remote / idle / status`，默认不开 Wi-Fi、不启用功放。`remote` 只有填妥本地私有配置后才可用。
- `firmware/audio_lab/audio_frame.h`：固定 20 ms 的 PCM16 帧格式，主机测试可直接编译。
- `firmware/audio_lab/audio_config.example.h`：本地配置模板；私有 `audio_config.local.h` 不应上传。
- `relay/server.mjs`：一台设备、一位家长的最小音频中继；公网上必须 HTTPS/WSS，令牌分角色。无音频存盘。
- `relay/public/`：独立家长实验页；按住说话发送，松开即停，接收侧可听到小车麦克风。
- `docs/WIRING_AND_TEST.md`、`docs/ARCHITECTURE_AND_VOICE.md`、`docs/LOCAL_TEST_RECORD.md`：接线、现场验收、并发策略、后续集成接口及本地测试证据。

## 本机复现（不接设备）

Windows PowerShell：

```powershell
Set-Location 'C:\Users\new20\Desktop\硬设资料\ESP32-S3开发\CareRover_Audio_Lab_2026-09-25\relay'
npm ci
npm test
```

音频帧主机测试（用可用的 `g++`/C++17）：

```powershell
g++ -std=c++17 -Wall -Wextra -Werror '..\tests\audio_frame_test.cpp' -o audio_frame_test.exe
.\audio_frame_test.exe
```

Arduino IDE 中打开 `firmware/audio_lab/audio_lab.ino`，板型按当前主板选择 ESP32S3 Dev Module、16 MB Flash、OPI PSRAM，Arduino-ESP32 Core **3.3.10**，115200。先只点“验证/编译”，**不要上传到当前小车主板**；该独立草图会覆盖现有完整小车固件。实物测试优先借一块备用 ESP32-S3 板。

## 远程中继的独立试运行

本机可用 `AUDIO_INSECURE_LOCALHOST=1` 开 `127.0.0.1` 的模拟模式，**仅供本机协议测试，不能给手机/公网用**。真实远程中继需公网主机、域名、受信任 TLS 证书及分别生成的高熵 `AUDIO_DEVICE_TOKEN` / `AUDIO_PARENT_TOKEN`；在服务器进程环境变量设置 `AUDIO_TLS_KEY` / `AUDIO_TLS_CERT` 为文件路径。防火墙开放 HTTPS 端口，浏览器访问域名；设备私有头文件填写相同设备令牌、热点信息、中继主机/端口、证书根 CA。CA 宏用 C++ 原始字符串格式 `R"PEM(-----BEGIN CERTIFICATE----- ... -----END CERTIFICATE-----)PEM"`，保留实际换行。未配置私有头文件时，`remote` 会直接报告 `REMOTE_NOT_CONFIGURED`。不要使用 `setInsecure()` 或把账号/令牌提交到 Git。

中继只传音频，**不代理现有实时视频或控制**；实际接入时沿用原控制页的控制权和急停逻辑，不允许语音音频流直接给舵机发 PWM。
