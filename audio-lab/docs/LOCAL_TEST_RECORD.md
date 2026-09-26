# 2026-09-25 本地音频原型测试记录

范围：`CareRover_Audio_Lab_2026-09-25/` 独立目录。没有向主板或 CAM 的串口写入，没有修改 `C:\CareRover\firmware`、网页或小车电源接线，也没有切换电脑 Wi-Fi。

| 项目 | 命令/方法 | 结果 |
|---|---|---|
| 主机音频帧 | `g++ -std=c++17 -Wall -Wextra -Werror tests/audio_frame_test.cpp`；运行产物 | PASS：帧长度、版本、序号、PCM 符号转换 |
| Arduino ESP32-S3 草图 | Arduino CLI 1.5.1 / Core 3.3.10；FQBN `esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`；compile only | PASS：程序 1,020,347 B/32%；全局 46,504 B/14% |
| Node 协议/中继/音频工作线程 | `cd relay; npm test` | PASS：5/5。含错误鉴权、跨源、重复连接拒绝，双向 648 B 转发，100 个按节奏送帧，本机自签证书 WSS，以及 48 kHz→16 kHz/50 帧工作线程 |
| JS 语法 | `node --check` 对 `server.mjs`、`public/client.js`、`public/capture-worklet.js` | PASS |
| 依赖审计 | `npm audit --omit=dev --audit-level=high` | PASS：`ws@8.21.3`，0 个已报告漏洞。初版 8.18.3 发现公告后已升级并重测；服务器另设最大 8 个分片、16 个缓冲片段 |
| INMP441 的真实电压/音量/I²S 左声道 | 需实物及万用表/串口 | NOT RUN |
| MAX98357 与 3 W/8 Ω 扬声器的音质、功放电流 | 需实物及新电源 | NOT RUN |
| 公网证书/热点/家长手机真实通话 | 需现场设备与公网中继 | NOT RUN |
| 与 CAM 视频、AI、摇杆、Server酱同时运行 | 需明确决策后再整合进当前固件 | NOT RUN |

结论：**本地可编译、协议可转发，不等于车上已经能通话。** 本轮“远程”只在本机 HTTPS/WSS 自签证书模拟环境验证了链路；公网主机、真实热点、浏览器音质、回声消除与小车整机资源竞争都未验收。下次现场必须按 [接线与阶段测试](WIRING_AND_TEST.md) 从断开功放 5 V 的麦克风测试开始。
