# CAM / 网页视频提速：2026-09-25 现场记录

## 目标与证据口径

目标是在保留 320×240 实时图像、人物框、手势、主板遥测和网页控制的前提下，提高网页观感及 CAM 推理频率。电脑现有互联网不可切换，因此基线和烧录后主要用 COM3 的 115200 串口指标；网页端让另一设备接入小车 AP 时，CAM 的 `stream_fps` 记录 HTTP 成功送出的帧率。`stream_fps` 不等于浏览器最终显示帧率，后者仍需现场画面与丢帧检查。

## 原版基线（只读设备，未切网）

COM3 已由 `@G/@P` 与 `cam_metrics` 确认为 CAM；COM6 为主板。2026-09-25 使用 `C:\Users\new20\Desktop\硬设资料\ESP32-S3开发\CareRover_Perf_2026-09-25\probe_cam_fps.ps1` 连续读取 12 秒：

| 指标 | 原版 |
|---|---:|
| JPEG 中位 FPS | 2.46 |
| 人脸平均 FPS | 2.56 |
| 手势平均 FPS | 4.99 |
| JPEG / 画面目标 | ≥4.00 FPS（第一阶段，不代表浏览器验收） |
| 串口探针判定 | FAIL；8 秒复测 JPEG 2.60 FPS，仍 FAIL |

额外串口样本显示 `gesture_ms≈132–133`、`face_ms≈37–39`、`jpeg_drops=0`、`capture_drops=0`、`wifi=true`。代码里 `videoPublish(frame)` 在 AI 推理后的同一循环调用 `frame2jpg()`，并受 200 ms 调度限制；当前数据支持“软件 JPEG 与 AI 串行争时间”是主要可优化点。仅凭此不能判定网页端不到 2 FPS 的额外损耗比例。

## 本轮改动（待实物 A/B）

- RGB565 相机帧仍给原有手势/人脸模型；在主循环把最新一帧复制到两个 PSRAM 原始帧槽之一，立即归还相机帧。
- 独立 JPEG 编码任务放在 AI 主循环的另一核心，帧过期时只保留最新待编码帧，不堆积延迟；现有三 JPEG 槽的 HTTP 读者租约继续保护慢客户端。
- JPEG 调度由 200 ms 改为 120 ms；编码速度不足时由“最新帧覆盖旧待处理帧”自然限速，`jpeg_drops` 计入队列替换/编码失败。新增 `jpeg_ms`、`stream_fps` 指标用于区分 CPU 编码和网络发送。
- HTTP `/stream` 地址、帧尺寸、JPEG 质量值、Web `<img>`/叠加框逻辑、`@G/@P` 串口协议、主控控制逻辑均不变；主控无需因这一步重新烧录。
- 编码队列的未完成写入不可读、最新帧替换、读者租约不可覆盖已通过本机 C++14 单元测试。ESP-IDF compile-only 和实机配置版均构建成功，应用分区仍有 43% 空间；设备包应用 SHA-256 `530D2033D16841C2826853F03775D858328D7432FD810CD94A101EF6917E7DE4`。

## 到货/现场验收表

| 阶段 | 结果 | 证据/待办 |
|---|---|---|
| 原版 USB 串口基线 | PASS（测得性能问题） | 探针 JPEG 2.46 / 2.60 FPS |
| 队列主机测试 | PASS | `tests/firmware/video_work_queue_test.cpp` |
| 新 CAM compile-only / 设备包 | PASS | ESP-IDF 5.3.4 / ESP-DL 3.3.11，最终应用 0x4015a0 B、分区剩余 43% |
| 旧 CAM 镜像保全 | PASS | `build/cam-stream-demo_balanced-boardexact-pre-perf-20260925` 应用 SHA-256 `1978648C69452163589F01907EB19CCF8DD4D7BDD06DEB341F4B41255C33573F` |
| 新 CAM Flash 备份/烧录 | PASS | 用户确认舵机 5 V 断开、四轮架空、IDLE。原版完整备份 `build/backups/cam-20260925-pre-perf-460800.bin` 为 8,388,608 B，SHA-256 `BEE97AF87FAB84837DFC427CAA9186E7BF6188F926235EE87559512F18BAF007`；COM3 同一 ESP32-S3 / 8 MB / MAC，三分区写入 Hash verified。低速首次完整读取中断；用户报告接头曾瞬断，可能相关但未证实。 |
| 新固件无观众 JPEG/AI 指标 | PASS | 同一 12 秒探针：JPEG 中位 **5.04 FPS**，人脸 2.47、手势 5.03 FPS，JPEG 编码均值 74.5 ms；`stream_fps=0` 符合无人连接；20 秒日志 `build/windows/cam-after-perf-20260925.jsonl` 中 103 个 `@G/@P` 全部 CRC 正确。 |
| 有观众网页视频 + 框 + 手势 | PARTIAL | 手机接入小车 AP，电脑保持互联网；用户确认视频更流畅、人物框仍跟随且“1”手势显示，但高帧率版本的框略滞后，见下文同场景 A/B。 |
| 主板运动/心率/IMU/OLED/摇杆回归 | PARTIAL | COM6 只读确认 `gesture`/`person`/`health`/`imu_status`/`front_status`/`wireless_status` 持续输出，`vision_link.bad` 连续 0；未给舵机 5 V 上电，运动/摇杆未验收。 |

本机 Node 23/23 PASS；Python 全套在项目 `.venv` 中此前 36/36 PASS，加入“已有备份 SHA 与尺寸”用例后仍需重跑总套。刷机辅助脚本因本机 IDF Python 自带 esptool 4.12.0，而原代码使用 esptool 5 的命令/选项拼写，已做版本兼容并通过定向测试。电脑没有切换小车 AP，互联网保持原连接。

## 如第一阶段仍不足的下一步

1. 利用新 `jpeg_ms`、`jpeg_drops`、`stream_fps` 判别编码、缓冲、Wi-Fi/浏览器三段瓶颈，再逐项调整 JPEG 间隔、质量与帧尺寸，不先牺牲人物框坐标分辨率。
2. 若 JPEG 编码仍占主导，做可回退的 A/B：摄像头原生 JPEG 供直播、单独解码给 AI；这会增加 AI 解码成本与 PSRAM 压力，不应凭理论直接替换。Espressif 的 [esp32-camera 文档](https://github.com/espressif/esp32-camera/blob/master/README.md)指出 RGB/YUV+Wi-Fi 会给 PSRAM 带来压力，并建议在其典型场景中优先 JPEG，但本项目是 RGB565 AI 双用途，须按两项真实 FPS 和框准确度决定。
3. 若 CAM 已产出 ≥4 FPS 而网页仍低于 2 FPS，检查手机/电脑客户端、信号、HTTP 发送、浏览器解码与覆盖层渲染；可比较同一 AP 下直接 `/stream` 和完整网页。保持主控遥测/摇杆 WebSocket 路径不变。
4. 高速 USB 有线视频需改变设备、布线和网页来源；现有 115200 UART 主要承载轻量检测结果，不适合传 320×240 连续 JPEG，因此不是本次无线 demo 的首选。

## 手机观看 + 手势负载对照（继续优化中）

手机作为小车 AP 客户端打开完整网页；电脑始终未切换互联网。相同的“1”手势可避免“2”触发旋转；舵机 5 V 保持断开。

| 实机 CAM 版本 / 场景 | JPEG 中位 FPS | HTTP 成功发送 FPS | 手势 FPS | 人物 FPS | 手势单帧耗时 |
|---|---:|---:|---:|---:|---:|
| 第一阶段异核编码，手机观看并持续“1”手势 | 3.38 | 3.38 | 3.38 | 1.69 | 258.5 ms |
| 第二阶段独立取帧，手机观看并持续“1”手势 | 9.19 | 9.11 | 2.64 | 1.32 | 309.3 ms |
| 第三阶段按观看启停、约 6 FPS 限速，手机观看并持续“1”手势 | 5.85 | 6.09 | 3.04 | 1.46 | 291.2 ms |

第二阶段应用 SHA-256 `7FA0E08D8DA0A86C8D170F00D99D3E924743C445A8A519BBE772E3DFC88FFD30`；COM3 写入 Hash verified，`video_capture_failures=0`，主板 COM6 无视觉协议坏帧。用户现场确认视频更流畅，人物框仍跟随但略滞后，网页显示“1”手势。单纯追求 9 FPS 使 AI 变慢，不作为最终平衡方案；该版已保存在 `build/cam-stream-demo_balanced-video-stage2-20260925`，第一阶段版保存在 `build/cam-stream-demo_balanced-video-stage1-20260925`。现已将视频任务改为仅在有人观看时采集、约 6 FPS 限速并重复同场景测量；是否再调整取决于现场观感。`stream_fps` 是 CAM HTTP 成功发送帧率，不是浏览器的逐帧统计。

第三阶段已烧录 COM3，应用 SHA-256 `EBEA840C5E7DE742C7A86B5E4585F12C5C0CE6F3ABF8A7481EFA9B7EB9BDDB9B`、长度 4,200,000 B，三段写入均 Hash verified。手机刷新后视频恢复；人物入镜、未持续展示手势的 12 秒指标：JPEG 中位 5.80 FPS、HTTP 5.45 FPS、手势 4.97 FPS、人物 2.40 FPS、视频采集失败 0。持续“1”手势时表中指标显示，相比第一阶段视频发送提高约 80%，但手势/人物频率仍分别低约 10%/14%；不能称为“AI 同时提速”。无人观看时 JPEG/HTTP 为 0 是按需休眠的预期行为，CAM AI 与串口照常工作。COM6 在第三阶段仍持续输出 `gesture`/`person`/`health`/`imu_status` 等，`vision_link.bad` 最近三次均为 0，`mode=IDLE`。用户对第三阶段观感的最终判断、实际浏览器逐帧帧率、真车跟随和摇杆运动仍待现场验收；舵机始终断电。

用户确认第三阶段视频流畅度可接受，但人物大幅快速移动时显示框偶尔漂移。40 秒连续手机观看复测：JPEG 中位 5.56 FPS、HTTP 5.44 FPS、人物 2.55 FPS、手势 5.12 FPS，视频采集失败 0。`build/windows/cam-fastmove-20260925.jsonl` 是随后约 22 秒的 CAM 原始串口记录：56 次 `@P` 中 51 次有框、5 次无框；约 11.5–12.3 秒有连续两次无框。相邻有框中心也会在约 0.4 秒检测间隔内大幅换向，因此主板显示层的 1.0 秒常速度外推可能让框暂时偏离最新图像。该记录不能直接量化浏览器画面相对框的时间偏差，因为 MJPEG 帧没有与 `@P` 共用时间戳。

只针对显示框拟作主板应用程序修正：显示调用把预测上限从 1000 ms 限到 350 ms；CAM 已送来新的高可信有框结果、而旧显示关联拒绝大跳变时，显示跟踪器重新锚定。跟随运动仍用其原先严格关联与原始新鲜检测，不走此宽松显示入口。新增回归先在旧 API 上编译失败，修正后通过；当前工作区八套固件测试 PASS，实机快照隔离构建中的 tracking 测试也 PASS。主板源文件快照与先前已核验设备包相比，仅 `box_track.h`、`wireless_runtime.cpp`、`build_version.h` 有变化；原 FFat/网页、bootloader、分区表和 boot_app0 二进制 SHA-256 完全一致。主板实物烧录与视觉验收结果待后续记录，不能将本机测试算作实物 PASS。

上述显示框修正已在主板实机烧录：COM6 先做 16 MB 完整只读备份 `build/backups/main-before-display-track-20260925.bin`，SHA-256 `B990BD42FF969FF50E574499121D9EB54F385A7E1A2C6F89F6F8CB70B8CE7218`。与 2026-09-21 已核验的完整实机备份逐区域比较，只有 NVS 不同；app0/app1/FFat/引导/分区表/OTA/coredump 均相同。因此仅写 0x10000 的 app0 应用区 1,108,624 B，esptool 写入后 `Hash of data verified`。新应用 SHA-256 `A4E15F3A2DD04656C54FAEF911CD213BBA5E9C27FCD0AEDAFD11B65CDE2E67E2`，该版包现归档在 `build/stage5-follow-display-track-disp1-20260925-device`，manifest 更新后 `tools/carerover.py verify` PASS。COM6 实机输出新版本 `0569bb8f3f3c66d2-s5-follow-disp1`、IDLE、`vision_link.bad=0` 最近三次，人物、手势、健康、IMU、前方、无线消息仍持续。此项只证明系统启动和遥测持续，手机画面快速移动的框效果仍待用户确认；舵机 5 V 全程断开。健康当前 `sample_timeout` 且无手指、IMU `valid=true`/`calibrated=false`（重启后待静置校准），均不能冒充为对应传感器最终通过。

disp1 手机快移验收：用户确认视频恢复、框飘移有一定改善，但出现“原地保持一段时间后瞬移”的新观感。这是 350 ms 硬预测上限和大跳变后立即重新锚定的直接后果，不能把 disp1 当作最终方案。COM3 同时复测 JPEG 中位 5.74 FPS、HTTP 6.05 FPS，视频采集失败 0。随后主板 IMU 静置复查已 `valid=true`、`calibrated=true`、`tilt_fault=false`；心率血氧仍为 `sample_timeout` / `sample_hz=0` 且无手指，此次视频调试没有完成健康传感器验收。

disp2 已隔离构建并烧录：仅显示预测改为随时间平滑减速（不再在 350 ms 截断），高可信大跳变以 220 ms 视觉过渡对齐新框；跟随运动 `track_.view()` 默认路径不变。新测试先因缺少 `displayPredictionSeconds` 红，新增实现后 PASS，工作区八套固件回归和隔离快照 tracking 回归均 PASS。disp1 已烧录包另存 `build/stage5-follow-display-track-disp1-20260925-device`，完整 Flash 备份不变。disp2 包为 `build/stage5-follow-display-track-20260925-device`，仅写 COM6 app0 @0x10000、1,108,960 B，应用 SHA-256 `1C55C325FF0CAF429B377ED980AD5C6DDDC00D6D234194FCAB15CC7F515148D8`，esptool 报告 `Hash of data verified`。烧录后 COM6 确认 `0569bb8f3f3c66d2-s5-follow-disp2`、IDLE、`estop=false`、`fault=false`，`vision_link.bad=0` 且有效包持续增加；IMU `valid=true/calibrated=true/tilt_fault=false`。手机观看时 COM3 同一 12 秒探针 JPEG 中位 5.99 FPS、HTTP 平均 6.04 FPS、人物 1.50 FPS、手势 2.98 FPS、采集失败 0；仍不等于手机实际显示帧率。这些只证明固件启动、串口链路和视频发送指标，手机快速移动框的视觉效果仍待用户 A/B 反馈；舵机 5 V 仍断开，未做实车运动验收。电脑互联网未切换。

用户反馈 disp2 相比 disp1 好一些，仍希望显示框更平滑。网页叠加层 `js/video-overlay.js` 现以浏览器现有最多 60 Hz 绘制节奏，对约 10 Hz 的主板框遥测做自适应时间常数插值（大位移约 60 ms，小抖动约 85 ms）；目标失效或页面尺寸变化时立即重置，不保留幽灵框。这只影响可视框，不改变 CAM 推理、主板人物跟随控制、手势与视频 HTTP 流。新增 Node 回归后 24/24 PASS。独立网页包 `build/stage5-follow-display-track-disp2-websmooth-20260925-device` 与 disp2 包相比，应用、引导、分区表、boot_app0 二进制哈希均相同；只有 FFat 变化。FFat @0x610000、10,354,688 B，SHA-256 `CAEEA4A795CCC12B75B4095E84AF2CB8C6C0B478AB20546312A51473F1430E1B`，esptool 报告 `Hash of data verified`。COM6 随后恢复 `...-disp2`、IDLE、`estop=false/fault=false`、`vision_link.bad=0` 且有效包持续增加。手机视觉 A/B 仍待用户反馈；电脑互联网未切换、舵机 5 V 仍断开。

网页更新后的独立 12 秒 CAM 探针仍 PASS：JPEG 中位 5.68 FPS、HTTP 平均 5.85 FPS、人物平均 1.88 FPS、手势平均 3.74 FPS、视频取帧失败 0。不同测量时刻的手势/人物入镜负载不相同，不能把这一组和前一组的差值归因于网页平滑；此变更不影响 CAM 推理代码。
