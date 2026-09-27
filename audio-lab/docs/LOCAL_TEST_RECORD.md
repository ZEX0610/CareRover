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

## 2026-09-26 现有主板临时实物测试

- 用户确认四轮架空、舵机 5 V 断开、主板 USB 稳定。读取当前主板整片 16 MB Flash 到本机 Git 忽略目录 `main-web/build/backups/main_pre_audio_2026-09-26_full16mb.bin`，长度 16,777,216 字节，SHA-256 `D739B6EC3139761E2E435185F4BA942F93564BCA982C13A0BC3717764AC678DA`。该文件可能含现场凭据，不上传 Git。仅临时改写主板应用分区 `0x10000`，CAM 未烧录。
- 现场接线与独立草图一致：INMP441 GPIO1/2/16、MAX98357 GPIO39/42/21、功放使能 GPIO38。两组时钟分别由 I²S0 RX 与 I²S1 TX 输出，不共用 GPIO。ESP32-S3 / 16 MB / OPI PSRAM 编译 PASS；主板采集持续约 49–51 帧/秒，静音 RMS 约 38–99，说话时出现更高 RMS。
- 独立功放 5 V 上电时，`idle` 保持安静；4 秒低音量 440 Hz `tone` 能清楚听到，结束恢复安静。功放与 3 W/8 Ω 扬声器的基本数字输出链路 PASS。
- 早期实时 `loop` 自听人声不可辨；提高增益后出现啸叫，串口显示采/放各约 50 帧/秒、`i2s_tx_err=0`、输出 RMS 接近 5000 峰值上限。根因是当前麦克风/扬声器物理布置下的声学正反馈；**实时开麦外放不通过**，不得把增大增益视为修复。
- 改为“功放静音录音 3 秒到 PSRAM → 再单独播放”，记录 150 帧，播放约 50 帧/秒且 `i2s_tx_err=0`；用户确认能听到基本清楚的人声、无啸叫、结束安静。采集/播放硬件能力 PASS；这不等于已完成远程双向通话。
- 下一阶段以按住说话、收/发互斥的半双工通话做首版，避免单麦克风/扬声器直通反馈；如需免提同时双向说话，必须实测物理隔声与回声消除。原 16 MB Flash 已全量写回且 esptool 校验写入哈希；串口再次显示原固件 `4e480f5e21e63c05-s5-follow`、`mode=IDLE`、`ap=true`、MPU6050 有效及 CAM 串口包持续增长。iPad 能看到 AP 并打开原网页，但视频出现原先经历过的断连/卡顿；Windows 此刻扫描未见小车 SSID。故代码恢复已证实，**网页/视频运行质量仍待继续排查**。
