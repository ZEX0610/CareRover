# Windows 现场进度（接手后维护）

初始状态：已于 2026-09-15 在本机开始接手。历史 Mac/CI 和旧包现场结果仍不算本轮实物验收；以下只记录本机实际执行结果。

## 当前交接点

- 当前阶段：W0 Windows 环境与构建复现（PASS）；W1 设备核对与完整备份（PASS）；W2 CAM 烧录/串口验收（PASS）；W3 observe（IN PROGRESS）；主控与 CAM 的 DEMO_BALANCED 设备包均已烧录并通过串口验收
- 最近完成：主控 `7a160cd4dc396869-s5-follow`（stage5-follow DEMO_BALANCED）与 CAM `demo_balanced` stream 均已写入并通过 Hash 校验；两板完整备份与硬复位完成；CAM/主控 115200 串口日志已保存并核对新固件版本
- 下一步：保持舵机 5 V 断开、车轮架空，先加入 CareRover-EE68（密码 88888888）并访问 192.168.4.1 做网页/无线 observe；随后再由现场授权进行传感器、人物框和运动验收
- 阻塞/待用户提供：尚未进行真人入镜、手指稳定放置、舵机落地或十分钟无线实物验收；这些需要现场输入和明确的运动授权
- 版本同步：本地提交 `922ed6d` 已生成；推送到 GitHub 因本机 Git Credential Manager 无可用凭据而被拒绝，待用户在本机完成 GitHub 登录后重试，未使用强制推送
- 最近修改及原因：仅补充本轮 Windows 可复现证据；尚未修改产品源码
- 下一条命令或人工操作：保持舵机 5 V 断开、车轮架空；电脑连接 `CareRover-EE68`（密码 `88888888`），打开 `http://192.168.4.1/?transport=ws&video=mjpeg`，执行网页 observe。需要运动时必须先由用户明确授权并保留急停可达。

- 软件部署：已完成一轮”手动模式/姿态/视觉显示/手势响应”改进并通过主机回归；主控 `7a160cd4dc396869-s5-follow` 与 CAM `demo_balanced` 设备包已在本机（COM6/COM3）烧录并通过串口验收（见下方 2026-09-16 记录）。

## 环境与设备

| 项目 | 实际值/证据 |
|---|---|
| Windows / Python / Git / Node | Windows 11 家庭中文版 10.0.26200 build 26200；Python 3.12.10；Git 2.52.0.windows.1；Node 24.15.0 / npm 11.12.1 |
| Arduino CLI / Core / 库 / FQBN | Arduino IDE bundled CLI 1.5.1；esp32:esp32 3.3.10；ArduinoJson 7.4.2 等见 `build/environment-report.json`；实板 profile 已核对：主控 16MB/OPI PSRAM，CAM 8MB/OPI PSRAM |
| ESP-IDF / ESP-DL 版本及 commit | ESP-IDF v5.3.4 / `1b459d9c4950395dec12ce19256c73f9a41e306f`；ESP-DL v3.3.11 / `5d9c36063dddbe98b5387828c831d6bbadb1370f`；两者工作树 clean；IDF 自带 CMake 3.30.2、Ninja 1.12.1 |
| 主控型号、Flash、PSRAM、串口 | COM6：ESP32-S3 rev0.2、16 MB Flash、8 MB embedded PSRAM，MAC `68:ee:8f:60:68:24`；烧录后 115200 日志含 `firmware=6123d4056525afc4-s5-follow`、`stage=5`、`backend=tracking`、`ap=true`、`imu_status valid=true calibrated=true`、`vision_link bad=0` |
| CAM 型号、Flash、PSRAM、串口 | COM3：ESP32-S3 rev0.2、8 MB Flash、8 MB embedded PSRAM，MAC `44:b1:76:b9:fe:b8`；烧录后 115200 日志含 CRC 正确的 `@G/@P`、`cam_metrics`，gesture/face/JPEG 约 2.4–2.9 FPS，gesture 约 249 ms，face 约 38–40 ms，`jpeg_drops=0`、`capture_drops=0`、`wifi=true` |
| 主控 / CAM profile 路径（不写密码） | 本地忽略文件 `config/board.local.json`、`config/cam-board.local.json` 和 `firmware/main_wireless/wifi_secrets.h` 已按实测板型与现场 AP 配置生成；密码不入库 |
| Flash 与 NVS 备份路径、大小、SHA-256 | 主控 `build/backups/main-before-20260915-com6.bin`，16,777,216 B，SHA-256 `89102E245B56035C004265584E5F80BD18FD0EC7725EB6FA50CE5E204D9969B9`；CAM `build/backups/cam-before-20260915-com3.bin`，8,388,608 B，SHA-256 `CB117F3D78E612CC43DEF0E200E13A145CF5C3B201EB4240DA90738E0736D8B5`；NVS 包含在整片备份内，未单独擦除 |
| 接线 / 电源 / 架空状态 | 本轮烧录前按现场约定保持舵机 5 V 断开、车轮架空；串口烧录/抓取未驱动舵机。真实落地运动和供电负载仍未验收 |

## 阶段记录

| 阶段 | 状态 | 构建版本 | 原始证据路径 | 问题与下一步 |
|---|---|---|---|---|
| W0 接收/环境/构建 | PASS | `a7d1f4135c88a114d85a697e994ae9c8dee7ed04` | `build/environment-report.json`；`output/windows/0915-replay.json`；五个 `build/*-check` / `build/cam-*-compile_only` 包；localhost Mock 浏览器记录 | Python/Node/C++/清单/回放、主控三组、CAM 两组及 Mock PASS；全部软件/模拟结果，不能代替实物验收 |
| W1 设备核对/备份 | PASS | COM6=16MB、COM3=8MB，均为 ESP32-S3 rev0.2 / 8MB PSRAM；两板完整备份成功 | `build/backups/main-before-20260915-com6.bin`、`build/backups/cam-before-20260915-com3.bin`（本地产物，未入库）；两份 SHA-256 见上表 | NVS 未擦除；备份文件不提交 Git，恢复时保留原路径 |
| W2 CAM | PASS | `cam-stream-safe_baseline-windows_baseline_confirmed` | `build/windows/cam-after-safe.jsonl`；备份 `build/backups/cam-20260915-213322.bin` | 烧录全部分区 Hash PASS，硬复位后 `cam_metrics`/`@G`/`@P` 连续输出；未做真人手势/人物实物验收 |
| W3 observe | IN PROGRESS | `6123d4056525afc4-s5-follow` | `build/windows/main-after-safe.jsonl` | 主控已烧录且 `wireless_status ap=true`、IMU valid、vision_link bad=0；待网页连接和现场输入验证 |
| W4 校准 | NOT RUN | — | — | — |
| W5 manual | NOT RUN | — | — | — |
| W6 follow | NOT RUN | — | — | — |
| W7 十分钟无线 | NOT RUN | — | — | — |

## 修改与测试追加记录

每项记录：时间、复现步骤、预期/实际、原始 FAIL 日志、改动文件与原因、相关回归、设备结果、遗留项。修复后保留旧失败记录，不覆盖成 PASS。

### 2026-09-15 W0 Windows 首轮复现

- Git：按交接命令获取 `feat/main-wireless`，HEAD `a7d1f4135c88a114d85a697e994ae9c8dee7ed04`；确认基线 `ad633bc7161f5dd2b12723e455aba7d4cf4246ac` 是其祖先；开始时工作树 clean。
- Python：使用 3.12.10 新建 `.venv`，安装 `tools/requirements.txt`、`hardware/requirements.txt`、`mock/requirements.txt`，`pip check` PASS；Python unittest 33/33 PASS。
- Web：本机 Node 24.15.0（高于文档 Node 22 基线），`node --test tests\\*.test.js` 21/21 PASS。
- C++：第一次由 PATH 命中 `C:\\Users\\new20\\MinGW\\bin\\c++.exe`（GCC 6.3），因不支持项目使用的完整 C++17 `std::clamp`/inline variable 而 FAIL；未改源码规避。改为本机既有 `D:\\MSYS2\\usr\\bin\\g++.exe`（GCC 15.2）后 `tools/check_firmware.py` 全部 PASS，包括双调优档、保护与故障场景。
- 交付清单：`tools/verify_demo_handoff.py` 核验原包内层 617 项 PASS；外层 ZIP 不在仓库中，因此外层 ZIP SHA 仍 NOT RUN。
- 离线回放：使用 GCC 15.2 对原包 evidence 完成 DEMO_BALANCED 回放，报告为 `output/windows/0915-replay.json`。旧真实 PPG 首次 HR 仍是 14350 ms；旧人物摘要不含 bbox，因此此回放不能证明低延迟目标、框误差、多人连续性或实车运动。
- Arduino：项目环境命令自动找到 Arduino IDE 内置 CLI 1.5.1，已安装 esp32 Core 3.3.10，无需下载或切换 Core；库与路径见 `build/environment-report.json`。
- 主控构建：`SAFE_BASELINE observe` PASS（程序 1,081,478 B / 34%，动态内存 62,048 B / 18%）；`SAFE_BASELINE follow` PASS（1,090,854 B / 34%，62,072 B / 18%）；`DEMO_BALANCED follow` PASS（1,096,318 B / 34%，56,760 B / 17%）。产物均标记 `compile_only`，命令确认 `NO DEVICE WAS FLASHED`。
- CAM 工具链：首次和第二次从 GitHub clone ESP-IDF 均因 443 连接失败；失败目录均未残留。随后从旧交接目录复制并核验完全匹配锁文件的 ESP-IDF v5.3.4 `1b459d9...`，项目 `prepare --idf` PASS；ESP-DL 为 `5d9c360...`。运行 IDF `install.bat esp32s3` 补齐用户级工具及独立 Python 环境，未改变仓库源码。
- CAM 构建：`SAFE_BASELINE stream` PASS，app 二进制 `0x3ffe60`，app 分区剩余约 43%；`DEMO_BALANCED stream` PASS，app 二进制 `0x400550`，剩余约 43%。两包均标记 `compile_only`，命令确认 `NO DEVICE WAS FLASHED`。
- 浏览器 Mock：启动 `mock/server.py` 并访问 `http://127.0.0.1:8080/?transport=ws&video=canvas`。跟随模式启用固定向右演示绕障后先显示 18 cm、速度归零和“停车确认”，随后恢复 120 cm 并显示“绕障已完成”；模拟断线时显示“Telemetry stale; input released”、速度归零，视觉/健康/遥测清空，随后自动重连；再次在绕障期间触发急停，收到 ACK，界面进入“急停锁定”，速度保持 `+0.00 / +0.00 / +0.00` 且模式控件禁用。解除急停出现人工确认框后选择取消，未越过确认。记录为 Mock PASS，不是实物停车或运动验收。
- 误用记录：在 Git 工作副本根目录直接运行 `tools/verify_handoff.py` 得到 `SOURCE_MANIFEST.json missing` / `EVIDENCE_MANIFEST.json missing`。该脚本固定校验“解压发布包根目录”且不接收路径参数，因此此 FAIL 不代表仓库内容损坏；本轮适用的 `verify_demo_handoff.py <0915原包>` 已 617 项 PASS，保留本记录避免将不适用入口误报为产品故障。
- 设备：串口盘点只有蓝牙 COM4/COM5。未据历史资料猜测主控/CAM COM、FQBN、Flash、PSRAM、供电或校准状态，也未烧录。

### 2026-09-15 W1 只读串口身份核对（主控）

- Windows PnP 重新枚举到 `USB-SERIAL CH340 (COM6)`（VID `1A86` / PID `7523`）；另有短暂的 COM3 Unknown 项，打开前已消失，未把它当作 CAM。
- 在不触发 reset、不开启 DTR/RTS 脉冲、不写入串口的前提下，以 115200 波特率只读 COM6 5 秒，捕获 62 行。日志连续包含 `wireless_status`、`imu_status`、`health`、`gesture`、`person` 和 `vision_link`：`firmware=fffc41be9b0db516-s5-follow`、`stage=5`、`backend=tracking`、`ap=true`、`imu address=0x68 valid=true calibrated=true`，因此 COM6 可确认是当前主控而非 CAM。
- 该捕获仅证明串口身份和运行状态：没有读取 Flash ID、没有整片备份、没有烧录、没有证明实物运动/人物框或无线网页验收。`health` 当时为 `no_finger`、`person found=false` 属于现场输入状态，不判为产品故障。
- COM6 捕获摘要仍保留工具给出的 `hardware_acceptance=NOT EVALUATED`；下一步是现场确认 CAM 单独端口和板型，再在舵机断电/车轮架空条件下执行 W1 备份。
- 版本同步：尝试 `git push origin feat/main-wireless` 返回“unable to get password from user”；提交仍安全保存在本地，远端尚未包含 `922ed6d`。

### 2026-09-15 W1 只读串口身份核对（CAM）

- CAM 重新接入后，Windows PnP 同时显示 `USB-SERIAL CH340 (COM3)` 与主控 `COM6`；COM3 状态为 OK，未再依据端口号猜测，而是读取协议内容确认归属。
- 在不触发 reset、不开启 DTR/RTS 脉冲、不写入串口的前提下，以 115200 波特率只读 COM3 5 秒，捕获 31 行，其中 28 个 CRC 正确的 `@G/@P` 帧和 3 个 `cam_metrics`。指标显示 gesture 约 2.50–2.86 FPS、face/JPEG 约 2.38–2.99 FPS、gesture 推理约 248–249 ms、face 推理约 37–40 ms、`jpeg_drops=0`、`capture_drops=0`、`wifi=true`，可确认 COM3 为 CAM。
- 当次画面为空背景：`person_found=0`、`max_person_gap_ms=438`，不能作为人物识别或跟随验收；这次捕获只完成串口身份/链路健康检查。

### 2026-09-15 W1 Flash ID 与完整备份

- 使用 esptool 读取两板身份（用户已授权进入烧录流程）：COM6 为 ESP32-S3 rev0.2、16MB Flash、8MB embedded PSRAM、MAC `68:ee:8f:60:68:24`；COM3 为 ESP32-S3 rev0.2、8MB Flash、8MB embedded PSRAM、MAC `44:b1:76:b9:fe:b8`。容量与私有 local profile 完全匹配。
- 主控从地址 0 读取 `0x1000000` 字节成功；CAM 从地址 0 读取 `0x800000` 字节成功。备份文件大小与目标容量一致，SHA-256 已写入表格。备份包含 NVS，未执行擦除或修改。
- 随后启动 SAFE_BASELINE 主控设备包编译；因用户要求暂停，在 Arduino 编译完成前中止。此次中止不涉及串口写入，未产生设备变更；恢复时可直接重新运行同一命令。

### 2026-09-15 W2 CAM SAFE_BASELINE 实物烧录与串口验收

- 使用 `tools/tracking.py cam-flash build/cam-stream-safe_baseline-windows_baseline_confirmed --port COM3`，以 venv 的 esptool 5.1.0 执行；先自动保存整片 8 MB 备份 `build/backups/cam-20260915-213322.bin`（SHA-256 `5E24EAB61E519A9C102454B89F5A6BDB7F9C83AF6F3D55A8E16D1EB5B0B358E6`），再写入 bootloader、分区表和应用。
- 三个写入区域均报告 `Hash of data verified`，最终重新读取 Flash ID 仍为 8 MB，并通过 RTS 硬复位；无擦除 NVS 的操作。
- 115200 串口抓取 `build/windows/cam-after-safe.jsonl`：连续出现 CRC 正确 `@G/@P`，`cam_metrics` 显示 `wifi=true`、gesture/face/JPEG 约 2.4–2.9 FPS、gesture 约 249 ms、face 约 38–40 ms，`jpeg_drops=0`、`capture_drops=0`。本次画面为空背景，不能据此宣称真人手势或人物跟随 PASS。

### 2026-09-15 主控 SAFE_BASELINE stage5-follow 实物烧录与串口验收

- 使用 `tools/carerover.py flash build/stage5-follow-6123d4056525afc4-device --port COM6 --baud 460800 --only all`；先自动保存整片 16 MB 备份 `build/backups/main-before-20260915-214923.bin`（SHA-256 `CBBCFBBF34713707AD8A545CC334F1801C978CD583758ED1D01874DE88E0E655`），随后写入 bootloader、分区表、boot_app0、应用和 FFat。
- 所有写入区域均报告 `Hash of data verified`，检测到 16 MB Flash，最终通过 RTS 硬复位；未擦除 NVS。
- 115200 串口抓取 `build/windows/main-after-safe.jsonl`：`firmware=6123d4056525afc4-s5-follow`、`stage=5`、`backend=tracking`、`ap=true`、`mode=IDLE`、`estop=false`；`imu_status address=0x68 valid=true calibrated=true`；`vision_link valid` 持续增长且 `bad=0/stale=0/resync=0`；健康/手势/人物事件均能持续输出。未因无手指、无人入镜而判为模块故障。
- 当前只完成软件写入与串口健康证据；网页连接、真人入镜跟随、手势动作和车轮运动仍属于后续 W3–W7 现场验收。

### 2026-09-15 W3 前置与同步状态

- 主控串口抓取证明 `ap=true`、固件 `6123d4056525afc4-s5-follow`、IMU `0x68 valid=true calibrated=true`；当前 Windows Wi-Fi 仍连接 `Tsinghua-Secure`，未擅自切换网络，因此网页 observe 尚未执行。
- `git push origin feat/main-wireless` 本轮再次尝试时因 HTTPS connection reset 失败；本地提交 `b7955c9` 已保留，未使用强制推送。待网络稳定或用户完成 GitHub 凭据后重试。

### 2026-09-15 手动/姿态/视觉回归修复（尚未烧录）

- 根因：主控 `SafetyController::tick()` 原先在 CAM 来源超过 490 ms、IMU 来源超过 100 ms 或滤波器瞬时无效时直接 `stop()`；模式被置为 `IDLE` 后，网页下一包非零速度自然收到 `NOT_IN_MANUAL`。视觉发布在 SAFE_BASELINE 直接采用原始人脸包，单帧漏检就清框。
- 修复：CAM/人物来源门限调整为 900 ms；IMU 新鲜度调整为 180 ms，单次 I²C/坏帧在窗口内保留最近健康状态；倾角改为 55°、恢复 42°、持续 400 ms；`BoxTrack` 改为有界常速度 Kalman（仅显示/关联，不驱动车轮），所有调优档均启用短时框保持；手势显示独立保持 2.4 s/无手 1.5 s；CAM 调度改为两帧手势一帧人脸，提高手势帧率。
- 回归：`tools/check_firmware.py` 全部测试 PASS；`npm test` 21/21 PASS。新主控包 `build/stage5-follow-bd75ab23c459ce7b-device`、新 CAM 包 `build/cam-stream-demo_balanced-windows_baseline_confirmed` 均已 compile-only 构建，尚未写入设备。

## 2026-09-12 超声波集成增量

新增 HC-SR04 驱动、前方保护/绕障状态机、网页/OLED 和 Mock。Windows/实物测试仍为 NOT RUN；接手时另读 ultrasonic-development.md。前方探头和绕障参数分别有 verified 门禁，模板 null 不可填写推测值。

## 2026-09-15 Mac 接收与软件优化（未操作实物）

已合并 0915 现场修复和现有超声波功能，三档配置及新字段详见 [0915 开发记录](0915-demo-development.md)。原包内层 617 项 SHA-256 PASS；外层 ZIP 缺失，NOT RUN。根目录 C++（含双配置）/Node 21/Python 33 通过，主控三配置、CAM balanced stream、独立校准工程编译通过，浏览器桌面/手机显示与断流清空通过。

历史 Windows W2–W6 记录保存在原包 `reference_docs/windows-progress.md`，不能用本文件的旧模板反推现场从未测试，也不能把历史 PASS 算到新包。现场下一阶段为 safe/observe 复现与舵机断电 A/B；真人延迟、多人框关联、倾斜 A/B、落地及十分钟测试均待补。真实 PPG 回放首次 HR 14.35 s，低延迟目标尚未全部达到。

## 2026-09-16 手动/IMU/视觉第二轮放宽（仍未烧录）

- 根因补充：`DEMO_BALANCED` 原先把正常车体振动/加速度变化当作 `transient` 坏帧；坏帧持续超过旧的 180 ms IMU 新鲜度后，`SafetyController::tick()` 将模式置为 `IDLE`，网页下一次非零 `cmd_vel` 因而得到 `NOT_IN_MANUAL`。这不是摇杆协议错误，而是模式已被安全控制器收回。
- IMU：删除动态加速度瞬态拒绝，仅拒绝非有限值、`|a|<0.12 g`、`|a|>3.5 g` 或 `|gyro|>1000 dps` 的物理异常；实际停车仍要求滤波后的俯仰/横滚达到 55° 并持续 400 ms，恢复阈值 42°。IMU 新鲜度窗口改为 750 ms。
- 链路与跟随：CAM/人物来源窗口改为 1200 ms，人物显示窗口 1400 ms；人物跟随接受分数为 0.40（CAM balanced 候选阈值 0.32），目标丢失宽限 1.1 s；命令看门狗 300 ms，仍要求网页持续刷新命令。
- 视觉：`BoxTrack` 使用有界常速度 Kalman（中心 x/y、log-area），只用于检测框显示和关联，不直接驱动车轮；预测显示上限 1.0 s。前端 `VISION_STALE_MS=1400`，覆盖当前 CAM 的 2G:1P 调度周期，单个漏检不会清框。
- 手势：CAM 采用两帧手势后再跑一帧人脸；显示进入/保持门限为 0.30/0.14（safe 档仍为 0.35/0.18），保持 2.6 s、无手 1.7 s；动作门限 balanced 为 0.40。
- 回归证据：`tools/check_firmware.py` 全部固件套件 PASS；`npm test` 21/21 PASS。主控包 `build/stage5-follow-7a160cd4dc396869-device`（16 MB，固件版本 `7a160cd4dc396869-s5-follow`）与 CAM 包 `build/cam-stream-demo_balanced-windows_baseline_confirmed`（8 MB，应用 `0x400e70`，约 43% 分区余量）均已 compile-only 构建；manifest 的 `source_commit=23ff6505b5cc8caf0d63654aadeee88d6ec25899`、`source_dirty=false`，命令输出 `NO DEVICE WAS FLASHED`。
- 当前设备状态：COM6/COM3 仍运行上一轮 SAFE_BASELINE，未因本次修复自动覆盖。需现场明确授权后，才可按备份和 115200 串口步骤烧录并做真人/实车验收；本机 Wi-Fi 未切换。

## 2026-09-16 DEMO_BALANCED 实物烧录与串口验收

- 现场授权后，在本机 COM6（主控）/ COM3（CAM）烧录 DEMO_BALANCED 设备包：均先自动整片备份、再逐区写入并 `Hash of data verified`、RTS 硬复位，未擦除 NVS，未驱动舵机。
- 主控：`tools/carerover.py flash build/stage5-follow-7a160cd4dc396869-device --port COM6 --baud 460800 --only all`，写入 bootloader / partitions / boot_app0 / app / ffat；整片 16 MB 备份 `build/backups/main-before-20260916-173024.bin`（16,777,216 B，SHA-256 `452d8710d368d99be4334c857b70802a59aa88ac80726b49a77463993f400e73`）。
- CAM：`tools/tracking.py cam-flash build/cam-stream-demo_balanced-windows_baseline_confirmed --port COM3`，写入 bootloader（21,552 B @0x0）/ partition-table（3,072 B @0x8000）/ app（4,198,000 B @0x10000）；整片 8 MB 备份 `build/backups/cam-20260916-173019.bin`（8,388,608 B，SHA-256 `9dd09f7fedfc8ca5b8d3e2040f4391086c7f4a86260fcbaf5344f931c7ee5231`）。
- 主控 115200 抓取 `build/windows/main-after-balanced.jsonl`：`firmware=7a160cd4dc396869-s5-follow`、`stage=5`、`backend=tracking`、`ap=true`；IMU `0x68 valid=true calibrated=true`；`min_heap=218132` / `min_psram=8324064`。`mode=FAULT`、`stop_reason=boot`、`tilt_fault=true`（`roll≈-67.8°`）为架空/倾斜姿态下安全控制器正确保持停车，非故障；`vision_link valid=0` 因当时 CAM 正在自行烧录未上线。
- CAM 115200 抓取 `build/windows/cam-after-balanced.jsonl`：296 个 CRC 正确 `@G/@P`、0 坏帧；`cam_metrics` 显示 gesture≈3.14 FPS（较 SAFE 的 ~2.5–2.86 提升，印证 2G:1P 调度）、face≈1.35–1.75 FPS / 39–40 ms、JPEG≈2.69 FPS、`jpeg_drops=0`、`capture_drops=0`、`wifi=true`、`min_heap=36807` / `min_psram=2516924`。
- 遗留：真人入镜、手指稳定放置、舵机落地、十分钟无线实物验收仍未执行；网页 observe 与运动授权仍按现场约定待办。

## 2026-09-16 跟随灵敏度/步长/丢目标续跟 调整（已烧录）

- 用户要求：更灵敏识别人脸移动、更小步长、框大小稳定、参考框=首次识别框、出框不退出跟随（30 s 等待）、暂不改 IMU 校准门禁（第 7 点跳过）。
- 改动（仅主控）：`person_follow.h` 降低中心/距离死区（0.04/0.025、0.05/0.03）、增益 0.40/0.50、`maxVx=0.08`/`maxWz=0.10`、加减速限幅 0.15/0.20；参考框由"前三帧中位数"改为"首帧面积"；`demo_tuning.h` 增 `FollowLostTimeoutMs=30000`；`safety_controller.h` 的 `person()` 在 `follow_.lost()` 时不再 `stop()`，改为保持不动并 30 s 后 `target_lost_timeout`，前方绕障丢目标仍即时停。
- 框大小稳定：沿用 `BoxTrack` 对数面积 Kalman + 跟随 EMA（0.30）；EMA 未进一步调低，避免拖慢收敛并破坏 SAFE_BASELINE 回归。
- 回归：`tools/check_firmware.py` 8 套全 PASS（经 MSYS2 `bash -lc` 用 `CXX=g++`，规避 Git Bash 下 cygwin g++ 找不到 `<cmath>` 的挂载问题）；`tests/firmware/tracking_test.cpp` 参考框断言改为首帧、新增 30 s 续跟超时用例。
- 构建/烧录：`build/stage5-follow-a0a4b33575a57b1a-device`（`a0a4b33575a57b1a-s5-follow`，程序 34%/动态 17%）；刷 COM6，整片备份 `build/backups/main-before-20260916-212413.bin`（SHA-256 `db7a150896d75077fd274bfed627104a983a41aeb14c0cb23add473a67a1e3b3`）。
- 串口：`build/windows/main-after-followtune.jsonl` 确认 `firmware=a0a4b33575a57b1a-s5-follow`；`mode=FAULT`/`tilt_fault=true`（`roll≈-101°`）为架空倾斜姿态，非故障。
- 遗留：真人入镜验证跟随灵敏度/小步/框稳定/出框续跟仍待现场；IMU 校准门禁（第 7 点）按用户要求未改。

## 2026-09-16 去除全部运动限制 + IMU 竖直安装（已烧录）

- 根因：现场 IMU 以矩形长边（Y 轴）竖直安装，重力落在 Y 轴上；旧代码按水平安装（重力在 Z）算倾角，`roll=atan2(ay,az)≈-101°`，瞬时锁存 `tilt_fault` → 模式退回 Idle → 下一次 `cmd_vel` 报 `NOT_IN_MANUAL`。
- 按用户要求去除全部运动限制（`safety_controller.h`）：移除 `watchdog`、`imu_timeout`/`imu_invalid`、`tilt_fault`、`camera_timeout`、`person_timeout`、`owner_watchdog`、`network_down`、`owner_disconnected` 的 `stop()`；`velocity`/`setMode`/`autonomousFollow`/`autonomousTurn` 不再因 IMU 健康/校准/相机/网络而拒绝；`snapshot().fault` 与 `clear()` 仅保留真正硬件 `fault`。仍保留：急停 `estop`、硬件 `fault`、前方超声波绕障。
- IMU 竖直安装（`imu_filter.h`）：`pitch=atan2(ax,ay)`、`roll=atan2(az,ay)`（Y 向上、重力沿 +Y 为水平）；互补滤波 yaw 用 gyro Y（`gy`）、pitch 用 `gz`、roll 用 `gx`。注：+Y 向上这一符号约定待真车水平时确认，若水平时 pitch/roll≈180° 需翻转符号。
- 回归：`tools/check_firmware.py` 8 套全 PASS（`safety/tracking/front/demo/tuning` 断言改为"模式在断开/超时/断网/丢相机时保持"，IMU 用例改竖直输入）；经 MSYS2 `bash -lc` 用 `CXX=g++`（Git Bash 下 cygwin g++ 因挂载找不到 `<cmath>`）。
- 构建/烧录：`build/stage5-follow-0baad7a2cc3f0802-device`（`0baad7a2cc3f0802-s5-follow`，程序 34%/动态 17%）；刷 COM6，整片备份 `build/backups/main-before-20260916-230338.bin`（SHA-256 `9a0f269837bcedcb7795c9e5a92bfceae1c90abbdc61c4a1b1e53768fc2a3a85`）。首次烧录因读备份时 Windows 串口 `PermissionError(13)` 中断（未写、未损坏），重试成功。
- 串口 `build/windows/main-after-unrestricted.jsonl`：`firmware=0baad7a2cc3f0802-s5-follow`；`mode=IDLE`、`fault=false`、`estop=false`（板上倾斜时不再显示 FAULT，网页可进入手动）。
- 卡尔曼说明：`BoxTrack`（`box_track.h`）确有常速度卡尔曼（`ScalarKalman`×3：中心 cx/cy + 对数面积 area），用于人物框显示与 DEMO_BALANCED 跟随测量（`track_.view()`）；IMU 倾角走互补滤波（陀螺+加速度 `alpha` 融合），非卡尔曼。
- 遗留：真车水平时确认倾角符号；运动限制已全去除，急停/硬件故障/前方绕障为唯一剩余保护。

## 2026-09-17 IMU 实为 X 轴竖直 + 恢复倾角保护 + 跟随解耦/预测/手势（已烧录）

- IMU 实测 `accel_g≈[1.03,-0.11,-0.07]`，重力落在 +X：现场实为 **X 轴竖直**（非 Y）。`imu_filter.h` 改为 `pitch=atan2(ay,ax)`、`roll=atan2(az,ax)`；互补滤波 yaw 用 `gx`、pitch 用 `gz`、roll 用 `gy`。串口确认 `pitch≈-6°`、`roll≈-4°`、`tilt_fault=false`。
- 按用户要求**恢复倾角保护**：`safety_controller.h::imu()` 在 `tilt` 且运动模式时恢复 `stop("tilt_fault")`（ImuFilter 55°/400ms 锁存）。其余限制仍去除。
- 转弯只一边轮子：跟随把 `vx`+`wz` 同时输出，`RR=vx-wz≈0` 被抵消。改为**解耦**：偏离中心时纯旋转（四轮同转）、居中后才前后；并提高 `maxVx=0.15`/`maxWz=0.20`（前后跟随原 `maxVx=0.08` 几乎无效果）。
- 人脸预测：跟随测量改用 `track_.view(now+lookaheadMs=300)`，用 BoxTrack 常速度卡尔曼预测未来 300 ms 人脸位置。
- 手势：`TWO`→顺时针一周（已有）、新增 `THREE`→逆时针一周（原逆时针为 OK，二者并存）；`LIKE`→开跟随、`DISLIKE`→停跟随（已有，4 帧确认/3 帧释放）。
- 回归：`tools/check_firmware.py` 8 套全 PASS；IMU 用例改 X 竖直输入；新增 THREE 手势用例。
- 构建/烧录：`build/stage5-follow-6a29b278dd82b6f6-device`（`6a29b278dd82b6f6-s5-follow`）；刷 COM6，整片备份 `build/backups/main-before-20260917-000518.bin`（SHA-256 `327e30b9064bc99d4494bf2552e0532bd08b8410d86e0e219db4db2fd437b4b1`）。
- 超声：`front_config.h` 定义 `FrontInstallation{trig,echo,FrontConfig}`；允许引脚 1/2/14/15/16/21/38–42；功能=前方障碍保护（stopCm/slowCm/warnCm/releaseCm）+ 自动绕障 demo bypass（lateralSpeed/forwardSpeed/settleMs/marginMs/passMs/lateralTimeoutMs）。接入需按 `front_config.example.h` 建 `front_config.local.h` 填 trig/echo 与阈值并置 `enabled`，`verified` 待实测后再置真。

## 2026-09-22 两块实机源码与烧录包精确同步（未操作设备）

- 权威来源：使用 2026-09-21 从实机读取的完整 Flash 备份进行分区级对照。主板 16 MiB 完整备份 SHA-256 `398FF2C941F84CE333E4C585E08672A1CC0DC760CA1AC385013341D15DE48F73`；CAM 8 MiB 完整备份 SHA-256 `6CE2830BDDE46F821E2758B4F8C8D156AE2790CE900FBE525F6DD16B8076B680`。
- 主控源码：把 `firmware/main_wireless/front_guard.h` 恢复为实机构建快照，并补齐生成时使用的 `build_version.h`；版本固定为 `0569bb8f3f3c66d2-s5-follow`。同步前本地版本已保存在外部 `CareRover_PreSync_Backup_2026-09-22`，没有删除。
- CAM 源码：确认当时构建使用的 `demo_tuning.h` 与当前主控版本不同。为避免两块板继续争用一份可变头文件，新增 `firmware/cam_tracking/board_exact_shared/` 保存 CAM 实机精确的 `vision_protocol.h`、`box_track.h`、`demo_tuning.h`；`tools/tracking.py` 在 Windows 暂存构建时按原相对路径复制它们。
- 标准包核验：主控 `build/stage5-follow-0569bb8f3f3c66d2-device` 的 bootloader、partition、app、FFat、boot_app0 共 5/5，与主板备份对应区域逐字节一致；CAM `build/cam-stream-demo_balanced-windows_baseline_confirmed` 的 bootloader、app、partition 共 3/3，与 CAM 备份对应区域逐字节一致。
- 重建结果：CAM 在 ESP-IDF 5.3.4 / ESP-DL 3.3.11、`C:\CareRoverTemp2` 和原 epoch 下，三个文件可逐字节复现。主控在 Arduino ESP32 Core 3.3.10、`C:\CareRoverTemp` 和原 epoch 下，功能载荷完全相同；新 ELF 只造成应用描述 ELF SHA 和尾部校验/验证 SHA 共 65 个构建元数据字节变化，之外无差异。标准包保留了已从实机核验的原应用镜像。
- 防回退：新增 `tools/rebuild_board_exact.py`。`verify` 检查主控全部固件/网页运行时输入的聚合版本 `0569bb8f3f3c66d2`，逐文件检查 CAM 全部 13 个构建输入以及主控 `front_guard.h`/生成的 `build_version.h`，并检查 8 个标准包文件 SHA-256；`build` 重建并检查主控 65 字节差异白名单、CAM 逐字节一致，且不会连接串口或烧录。
- 回归：Node 23/23 PASS；Python 34/34 PASS；`tools/carerover.py verify` PASS；`tools/rebuild_board_exact.py verify` PASS，输出 `NO DEVICE WAS FLASHED`。
- 文档：新增 `docs/BOARD_EXACT_SOURCE_2026-09-22.md`，记录证据目录、源码映射、完整哈希、重建环境、核验命令和 65 字节元数据边界。
- 设备状态：本阶段没有打开 COM 口、没有复位、没有擦除 NVS、没有烧录；结论来自既有实机完整备份、归档构建输入与离线重建核验。

## 2026-09-25 CAM 视频首阶段提速（主板未烧录，电脑网络未切换）

- 设备/现场：115200 只读串口确认 COM3 为 CAM、COM6 为主板；用户确认舵机 5 V 断开、四轮架空、网页 IDLE。原版 12 秒探针 JPEG 中位 **2.46 FPS**、人脸均值 2.56、手势均值 4.99；8 秒复测 JPEG 2.60 FPS，低于第一阶段 4 FPS 目标。具体方法见 `docs/VIDEO_PERF_2026-09-25.md`。
- 代码：CAM 的软件 JPEG 编码从 AI 主循环拆出，异核任务只消费最新 RGB565 帧；三槽 HTTP 租约、320×240、JPEG 质量、`/stream`、`@G/@P` 不变。新增 `jpeg_ms`/`stream_fps` 诊断；队列主机测试 PASS。主控与网页均未因本轮提速烧录或修改。
- 构建：ESP-IDF 5.3.4 / ESP-DL 3.3.11 的 compile-only 与设备版均 PASS；新设备包 `build/cam-stream-demo_balanced-windows_baseline_confirmed`，应用 SHA-256 `530D2033D16841C2826853F03775D858328D7432FD810CD94A101EF6917E7DE4`。原板精确设备包另存 `build/cam-stream-demo_balanced-boardexact-pre-perf-20260925`，原应用 SHA-256 `1978648C69452163589F01907EB19CCF8DD4D7BDD06DEB341F4B41255C33573F`。
- 备份/烧录：首次低速完整备份中断，用户报告 CAM USB 接头曾短暂断开，因未保留底层错误不能证明唯一原因；此阶段没有写入。随后 460800 小读与 115200 小读哈希一致；460800 整片 8 MB 读回 PASS，`build/backups/cam-20260925-pre-perf-460800.bin`，SHA-256 `BEE97AF87FAB84837DFC427CAA9186E7BF6188F926235EE87559512F18BAF007`。本机 esptool 4.12.0 命令/选项与工具原来的 esptool 5 拼写不一致，已加版本兼容及回归；复用经长度/哈希验证的备份后，仅 CAM COM3 三分区写入，工具逐段 `Hash of data verified`。
- 初测：新 CAM 无网页观众时同一探针 JPEG 中位 **5.04 FPS**（约 2.05×）、人脸 2.47、手势 5.03、编码约 74.5 ms；20 秒串口 `build/windows/cam-after-perf-20260925.jsonl` 含 103 个 CRC 正确 `@G/@P`、0 坏帧。网页实际收帧、真人框/手势与 COM6 整机回归尚未验收；用户已被提示用第二台设备连 AP，电脑互联网不切换。
- 手机网页 A/B：用户确认视频和人物框可见、“1”手势显示。持续“1”手势时第一阶段 JPEG/HTTP 均 3.38 FPS，手势 3.38 FPS、人脸 1.69 FPS；异核独立取帧版 JPEG 9.19 / HTTP 9.11 FPS，但手势 2.64 / 人脸 1.32 FPS。用户报告视频更流畅、人物框稍有滞后；故继续调整视频采集负载，暂不把 9 FPS 版定为最终。COM6 只读见 `gesture`、`person`、`health`、`imu_status` 持续输出，视觉链路坏帧连续为 0；舵机 5 V 未接通，无运动验收。详见 `docs/VIDEO_PERF_2026-09-25.md`。
- 第三阶段平衡版：视频任务只在 HTTP 观众连接时采集，相机取帧约 6 FPS；COM3 写入 Hash verified，新应用 SHA-256 `EBEA840C5E7DE742C7A86B5E4585F12C5C0CE6F3ABF8A7481EFA9B7EB9BDDB9B`。手机网页已刷新并恢复；人物入镜、未持续展示手势时 JPEG 5.80 / HTTP 5.45、人物 2.40、手势 4.97 FPS；持续“1”手势时 JPEG 5.85 / HTTP 6.09、人物 1.46、手势 3.04 FPS，采集失败 0。COM6 模式 IDLE、视觉坏帧连续 0，健康/IMU/人物/手势遥测持续；舵机仍断电。9 FPS 版和第一阶段版均留有独立构建包以回退，最终观感与运动验收待用户现场判断。
- 快速移动框漂移：用户认为约 6 FPS 视频足够，但人物大幅移动时框偶尔飘移。CAM 22 秒原始串口 `build/windows/cam-fastmove-20260925.jsonl` 中 56 次人物检测 51 有框、5 无框，有两次连续无框约 0.8 秒。主板显示层原常速度预测最长 1.0 秒；仅显示框改为 350 ms 外推上限，且 CAM 已送来更新的高可信目标、旧显示关联却拒绝大跳变时重新锚定。跟随运动使用的严格关联保持原样。新增测试先红后绿；八套固件回归 PASS。主板从原实机精确快照隔离构建，仅 `box_track.h`、`wireless_runtime.cpp`、版本标识不同，bootloader、分区表、boot_app0、FFat 哈希均与原包相同。
- 主板实机更新：用户确认舵机 5 V 断开、四轮架空、网页 IDLE。COM6 先读完整 16 MB 备份 `build/backups/main-before-display-track-20260925.bin`，SHA-256 `B990BD42FF969FF50E574499121D9EB54F385A7E1A2C6F89F6F8CB70B8CE7218`；与 2026-09-21 实机完整备份相比，除 NVS 外每个区域（app0/app1/FFat/引导/分区/OTA/coredump）均一致。只在 0x10000 写入新应用 1,108,624 B，esptool `Hash of data verified`；程序 SHA-256 `A4E15F3A2DD04656C54FAEF911CD213BBA5E9C27FCD0AEDAFD11B65CDE2E67E2`。包 `build/stage5-follow-display-track-20260925-device` 已更新 manifest 并通过 `tools/carerover.py verify`。COM6 确认版本 `0569bb8f3f3c66d2-s5-follow-disp1`、IDLE、`vision_link.bad=0` 连续，人物/手势/健康/IMU/前方/无线遥测持续。健康当次为 `sample_timeout`/无手指，不能算 HR/SpO2 有效性验收；IMU 读数有效但重启后当次 `calibrated=false`，仍需静置后复查。手机刷新后的快速移动观感待用户回报；未给舵机上电，未做真车跟随/摇杆验收，电脑互联网未切换。详见 `docs/VIDEO_PERF_2026-09-25.md`。
- disp1 手机快移反馈：视频恢复且框漂移有所改善，但变成“原地保持后瞬移”。原因与 350 ms 硬预测截断及立即重新锚定相符；继续做显示专用 disp2：渐减速预测 + 220 ms 重锚视觉过渡，车轮跟随逻辑不改。新增测试红→绿，八套固件回归和隔离快照 tracking 回归 PASS。已烧录 disp1 包另存 `build/stage5-follow-display-track-disp1-20260925-device` 以回退。IMU 静置后复查 `valid=true/calibrated=true/tilt_fault=false`；健康仍 `sample_timeout/sample_hz=0`、无手指，另需专项检查。
- disp2 实机：用户再次确认舵机 5 V 断开、四轮架空、网页 IDLE。只烧 COM6 app0 @0x10000 的 1,108,960 B，SHA-256 `1C55C325FF0CAF429B377ED980AD5C6DDDC00D6D234194FCAB15CC7F515148D8`，esptool `Hash of data verified`。COM6 115200 只读确认版本 `0569bb8f3f3c66d2-s5-follow-disp2`，`mode=IDLE/estop=false/fault=false`，`vision_link.bad=0` 且 `valid` 持续增加，IMU `valid=true/calibrated=true/tilt_fault=false`。手机网页快速移动框的观感待用户反馈；舵机仍断电，未做运动验收，电脑互联网未切换。详见 `docs/VIDEO_PERF_2026-09-25.md`。
- disp2 手机初评更好但仍希望框更平滑；网页层单独增加每帧视觉插值，约 10 Hz 框遥测在浏览器最多 60 Hz 绘制中平滑过渡，且对大位移加快追赶。Node 测试 24/24 PASS。独立包 `build/stage5-follow-display-track-disp2-websmooth-20260925-device` 核验 PASS，应用/引导/分区表等四文件与 disp2 逐字节一致；仅烧 COM6 FFat @0x610000、10,354,688 B，SHA-256 `CAEEA4A795CCC12B75B4095E84AF2CB8C6C0B478AB20546312A51473F1430E1B`，esptool `Hash of data verified`。烧录后 `...-disp2`、IDLE、无急停/故障、CAM 坏帧 0。用户手机视觉 A/B 待回；视频/人物/手势在上轮手机观看探针分别为约 6.04/1.50/2.98 FPS（HTTP 发送/模型推理，非浏览器渲染帧率）。舵机仍断电、电脑网络未切换。

## 2026-09-25 Server酱 AP+STA 入座/离座推送（进行中）

- 现场条件：用户确认有独立 2.4 GHz 手机热点，iPad 继续连 CareRover AP，热点信息沿用私有配置；HC-SR04 继续作为入/离座来源。用户确认舵机 5 V 断开、四轮架空、网页 IDLE、COM3/COM6 USB 稳定，并允许最多两条实测微信通知。电脑互联网未切换。
- 根因：旧 `sendWechatMsg` 在 `frontTask` 中同步调用、走明文 HTTP 80，且主板仅 `WIFI_AP` 不连接外部热点，因此不能投递。直接复用旧桌宠 `WIFI_STA`/发送后关闭 Wi-Fi 的做法会破坏当前 CAM 和网页。
- 修改：主板启动 AP+STA，`192.168.4.1` AP/`192.168.4.2` CAM 拓扑不变；仅主板 STA 连接独立热点。新增 IDLE 有效回波门禁、800 ms 稳定确认、5 s 静默窗，以及长度 1 队列/低优先级 HTTPS 任务。事件最长等待联网 60 s；每个事件只尝试一次，避免 Server酱收到请求但响应丢失时重复通知。校验 TLS CA、UTC、HTTP 200、JSON `code=0`；密钥不进日志。具体接线/现场顺序见 `docs/WECHAT_APSTA_2026-09-25.md`。
- 离线回归：固件 8 套主机测试 PASS，网页 Node 24/24、Python `unittest` 37/37 PASS，`git diff --check` PASS；Arduino ESP32 Core 3.3.10 设备包 `build/stage5-follow-4e480f5e21e63c05-device` 构建/校验 PASS（应用 1,223,878 B、38%，全局 58,832 B、17%）。新引导/分区/boot_app0 与当前包一致，新 FFat 不写入；仅应用更新。
- 实机更新：COM6 `flash-id` 确认 ESP32-S3/16 MB；完整备份 `build/backups/main-before-serverchan-20260925.bin` 为 16,777,216 B，SHA-256 `53366EDEB22331C89A58717718A280BA448B17A5FCFA09AB5A5ED7D194377C1A`。备份中 app0 前 1,108,960 B 哈希 `1C55C325FF0CAF429B377ED980AD5C6DDDC00D6D234194FCAB15CC7F515148D8`、FFat 哈希 `CAEEA4A795CCC12B75B4095E84AF2CB8C6C0B478AB20546312A51473F1430E1B`，均与上一实机包匹配。仅烧主板 app0 @0x10000，1,224,032 B，SHA-256 `B9E1AF34520135C77DE65BCBA50CD035F1AD56B51FC18DBCA4B5C53DFA369AC7`，esptool `Hash of data verified`；没有写 CAM/NVS/FFat。
- 烧录后 COM6 115200：`firmware=4e480f5e21e63c05-s5-follow`、`ap=true`、`mode=IDLE`、`estop=false`、`fault=false`、`vision_link.valid=262/bad=0`；启动当次 `min_heap=194784`。用户确认 iPad 仍可打开网页并观看视频、人物框、手势，手机热点已连接到小车主板；电脑互联网始终未切换。
- 现场通知验收：用户在 IDLE 下完成 HC-SR04 入座、离座两次变化，微信恰好收到 **一条 Seated、一条 Vacant**，无重复；推送期间 iPad 视频/人物框/手势/网页连接均正常、无明显变慢。CAM COM3 12 秒只读指标 `stream_fps=6.11`、`jpeg_fps=5.68`、`face_fps=1.31`、`gesture_fps=3.06`、`wifi=true`、`jpeg_drops=0`、`capture_drops=0`；与前轮约 6 FPS 观测同量级。COM6 复查 `mode=IDLE`、`ap=true`、前方有效 41.9 cm/`seated=false`、`vision_link.valid=2857/bad=0/stale=0`，运行中 `min_heap=114160`。以上是短时静态实物验收；舵机仍断电，APSTA 下真车运动和长时间压力测试未执行。

## 2026-09-27 私有远程网页和音频联调（进行中）

- Windows 保持 iPhone USB 上网和 CareRover Wi-Fi 双网络；远程私有 HTTPS 经无代理 Chrome 登录后，用户确认视频、人物框、手势、心率血氧、IMU 实时更新。此前普通浏览器访问失败定位为本机 HTTP 代理不支持 tailnet 内私有地址，而非云服务器证书或网页服务故障。远程摇杆与真车运动仍未验收。
- 独立 INMP441/MAX98357 实验已确认收音和静音录制后回放可听；本轮在 `CAREROVER_AUDIO_GATEWAY` 编译开关下为主板增加 I²S0/1 `/audio` WebSocket，功放未上电时编译成功。新应用 1,129,440 B，SHA-256 `F58ED43BC91FD773B3FBC9082FBA5AD3C9923DCA7D5E676B9C67EA7DD3B6CF77`；COM6 识别 ESP32-S3 16 MB 后只写 app0 @0x10000，esptool 报 `Hash of data verified`，CAM/NVS/FFat 未写入。当前主板版本标记 `4e480f5e21e63c05-s5-follow-apdiag-audio1`，仍为临时 AP-only，微信通知暂停；原完整 16 MB 备份 SHA-256 `D739B6EC3139761E2E435185F4BA942F93564BCA982C13A0BC3717764AC678DA` 可回退。
- Windows Wi-Fi 在烧录重启后自动断开，已用现有配置重新关联小车 AP，iPhone USB 默认上网未切换。主板首页 HTTP 302 约 0.07 s、CAM 根路径预期 HTTP 404 约 0.02 s；云中继 `controlDevice=true`、`videoFresh=true` 恢复。功放断电时本地 `/audio` 6 秒收到 301 个 648 B 帧（约 50 帧/秒），峰值 1681 PCM；期间网页响应和远程视频仍正常。
- 以 `-EnableAudio` 重启 Windows 网关后，车端音频上行约 50 帧/秒，控制/视频仍在线。服务器一度显示 `audioPaired=true` 且收到家长端下行帧；功放上电后的实际双向听感和通话页面验收仍待用户确认，不可把传输计数视为通话完成。舵机 5 V 保持断开、四轮架空。
- 首轮真实通话：用户确认小车麦克风说话可在电脑听到、电脑说话也可在小车听到，但原版电脑→小车方向回声多且刺耳；此项不合格。已请用户结束通话并断开功放 5 V，针对性地增加三层半双工处理：主板下行播放数字幅度 `/8` 并限幅、车端在家长下行后 700 ms 不上行；Windows 网关在收到家长帧后 700 ms 抑制车端上行；浏览器按住说话时及松开后短时静音收听通道，并给车端低电平收音加 10 倍增益和压缩限幅。网关 2/2、云中继 7/7 测试通过。
- 烧录前读回实机 audio1 应用 `build/backups/main_audio1_pre_echo_20260927.bin`，SHA-256 `F58ED43BC91FD773B3FBC9082FBA5AD3C9923DCA7D5E676B9C67EA7DD3B6CF77`，与原镜像一致。用户确认功放/舵机 5 V 断开、车轮架空后，只写主板 app0 的 audio2 应用 1,129,504 B，SHA-256 `1A88EDE9CB47C9EFF111902768EF35CC61001646579E9C68316D40D736BDA7A0`，esptool `Hash of data verified`；CAM/NVS/FFat 未改。Windows Wi-Fi 重关联后主板约 0.08 s、CAM 约 0.02 s 响应，私有中继 `controlDevice=true/videoFresh=true`。
- 私有服务器已更新家长通话脚本，旧版 `client.js.bak-20260927-echo` 留在服务器；Windows 网关以新版半双工逻辑重启。用户刷新页面后确认车端→电脑能听到；功放独立 5 V 上电后空闲安静无发热。再做短句交替测试，用户确认电脑、小车两端都能听到，视频与传感器数据持续；服务器 `audioPaired=true`、`controlDevice=true`、`videoFresh=true`，家长/设备音频帧均持续增长。用户尚未单独明确量化修正后的回声与音量，也尚未用另一台独立上网的家长设备验收；不能把同一台 Windows 的云回环测试称作异地验收。主板仍是 AP-only 诊断版，微信推送暂停，舵机保持断电。
- 用户补充：扬声器离电脑远一些，回声就减轻；这与双方在同一房间、电脑麦克风再次拾取车端扬声器的声学回路相符。半双工代码不能消除物理共处空间的全部声反馈；正式演示宜让家长端戴耳机或在另一房间。用户暂无另一台可独立联网并加入同一 tailnet 的设备，故**真正跨设备/异地验收暂不可执行**。同一台 Windows 经云服务的双向通话与视频/遥测并行已短时通过，尚未实测长时间稳定性。

## 2026-09-27 R5 网页与原生 iPhone App 交接（进行中）

- 用户报告此前 iPhone182 远程视频已经恢复；本轮接收 `CareRover_UI_R5_2026-09-26` 原包，39/39 manifest SHA-256 核对通过。仅移植 HTML/CSS/JS：`main-web` 为局域网 FFat 页面，`remote-hub/relay/public` 为私有 HTTPS 页面；远程视频强制使用同源 `/stream`，电话按钮打开已有 `/call`，局域网电话按钮提示改用 HTTPS。没有改动 CAM 或主板应用源码。
- 已新增根目录 `APP_INTEGRATION_START_HERE.md` 和 `remote-hub/docs/IOS_CLIENT_API.md`，将真实拓扑、会话/MJPEG/控制 WSS/PCM16 音频 WSS、遥测字段与未验收项交给另一台 Mac 上的 Swift/SwiftUI App 开发者；本轮没有取得 Xcode 项目，不能声称已修改 App。文档不包含家长或设备令牌。
- 离线检查：局域网网页 Node 24/24 PASS，云中继 Node 7/7 PASS，FFat 工具 Python `test_tooling.py` 13/13 PASS；`git diff --check` PASS。R5 FFat 镜像已生成，web version `48a98f924378a7fe`，10,354,688 B，SHA-256 `EC26279180783C5836B494B9AF166D1889E2B86BFEE52183213757F39099C994`，位于忽略的 `build/ui-r5-audio2-20260927/ffat.bin`。
- 私有云中继 R5 已部署：上传 tar SHA-256 `79979A83ACE811A3C4E031A965A55BC731CC0398247C58CE2535450C61F08D51`；服务器旧 UI 备份 `/opt/carerover/backups/relay-ui-before-r5-20260927T063159Z.tar`。`carerover-remote-hub.service` 重启并确认为 active，回环 `/health` 和 Windows 私有 HTTPS `/health` 均返回 HTTP 200。重启/主板备份过程会令设备链路暂断，需完成板上 FFat 更新、Windows 网关重连和实际浏览器验收后才能记为完整现场通过。
- 用户确认四轮架空、舵机 5 V 断开、IDLE、主板 USB 稳定；COM6 `flash-id` 确认为 ESP32-S3 rev0.2、16 MB Flash、8 MB PSRAM、MAC `68:ee:8f:60:68:24`。更新前整片 16 MB 备份 `build/backups/main-before-ui-r5-20260927.bin` 已读完，16,777,216 B，SHA-256 `09A0000FB9F186669A8041A6ABEC5FD2E3E1B74E49CA6F38CF7DA7C3DD077D5F`；其中 app0 前 1,129,504 B SHA-256 `1A88EDE9CB47C9EFF111902768EF35CC61001646579E9C68316D40D736BDA7A0`，与 audio2 实机应用吻合；旧 FFat SHA-256 `CAEEA4A795CCC12B75B4095E84AF2CB8C6C0B478AB20546312A51473F1430E1B`，与旧版镜像吻合。
- 仅写 COM6 FFat @`0x610000`，新镜像 10,354,688 B；esptool 报 `Hash of data verified` 并复位。未写 CAM、NVS 或 app0。Windows 烧录后 Wi-Fi 曾断开，使用原保存的 `CareRover-EE68` 配置重新连接，iPhone USB Internet 未切换；本地主板首页 HTTP 302 约 0.06 s、CAM 根路径预期 HTTP 404 约 0.07 s。主板实机 HTML 含 `carerover-web-version=48a98f924378a7fe`，新增 `/css/workspace.css`、`/js/workspace.js` 均 HTTP 200；云中继 `/health` 再次为 `controlDevice=true/videoFresh=true`。真人浏览器视觉、通话按钮及本地/远程视频观感仍待用户确认，不以 HTTP 200 代替现场 UI 验收。
- 用户随后在本地 CareRover Wi-Fi 页和另一台已连 Tailscale 的家长设备上强制刷新，回复“两处均正常”：R5 新布局、视频、人物框、手势、健康和 IMU 均正常更新。电话弹窗及异设备收听另行验收，舵机 5 V 仍断开，未执行运动命令。
- 全量回归发现旧测试把当前可变 `build_version.h` 锁定到 9 月 22 日快照哈希，和已烧录的 audio2 源码不符；改为检查版本头格式、stage 与 integration，同时保持真正的静态 board-exact 文件哈希断言。修正后 Python 39/39、局域网页 Node 24/24、云中继 Node 7/7、Windows 网关 Node 2/2 PASS。
- 用户随后报告 iPhone182 视频正常但听不到小车麦克风。云端只读 `/health` 显示 `deviceFrames` 持续增加而 `audioPaired=false`：车端麦克风已到中继，家长音频 WS 当时未连接。建立 35 ms 红测试：已登录网页仅携带现有会话 Cookie 连 `/audio`，旧服务返回 HTTP 403。服务器现允许同源 `cr_session` Cookie 或旧 `parent.<token>` 子协议鉴权；通话页已登录时可留空第二次口令。匿名/异源仍被拒绝；云中继 Node 8/8 PASS。旧版服务器三文件备份 `/opt/carerover/backups/relay-before-audio-session-20260927T065945Z.tar`，新服务已部署，私有 HTTPS `/health` 恢复 `controlDevice=true/videoFresh=true`，车端音频帧递增。**iPhone182 收听与播放尚待用户复测，不记为通过。**
- iPhone182 报告 R5 弹窗显示“接收帧增长”但无声；同一手机将独立 `/call` 在 Safari 顶层标签打开后，**能听到小车麦克风**。因此车端上行和手机扬声器并非全断，故障与内嵌 iframe 的播放上下文相关；这是现场 A/B，不把它泛化为所有 iOS iframe 均不可播放。该独立页按住说话约 5 秒后车端仍无声；重启前中继累计 `parentFrames=0`，说明首先排查手机按住事件/采集/发送，不应先归责功放。
- 增加 R5 顶层通话控件红测试（旧版失败、新版通过）；云中继 9/9 PASS，JS 语法与 diff 检查通过。只替换服务器 `public/client.js`、`console.html`、`css/remote.css`、`index.html`、`js/app.js`，事前备份 `/opt/carerover/backups/relay-before-top-level-call-20260927T071517Z.tar`，上传归档 SHA-256 `2106ee38939807a98af8f7bfdbe29a4ab1ad47ee7fe0ce221531df6ddb294a7e`。部署后服务 active，`controlDevice=true/videoFresh=true` 且车端音频计数恢复。**iPhone182 顶层 R5 页双向声音仍待现场复测**；舵机未上电，本轮未操作运动。
- iPhone182 顶层 R5 复测确认能听到小车麦克风，但按住约 5 秒后车端仍无声。服务器当次仅收到 `parentFrames=22`，约 0.44 秒，因而重点转向手机触摸保持事件。增加真实 `touchstart`→`touchend` 处理与永久可见的“麦克风采集/已发送”计数，旧版触摸红测试失败，新版 relay 10/10 PASS。第二次只部署六个网页文件（含独立通话页 CSS），备份 `/opt/carerover/backups/relay-before-top-level-call-20260927T072323Z.tar`，上传归档 SHA-256 `896ddfc2c31c61c42fe5aa17b73db4fb2cf95cc42e68be664b3cd4cb9bac257f`；服务和车端控制/视频恢复。
- 第二次现场复测服务器已收到 `parentFrames=184`，随后累计 `498`；Windows 网关 `audioDownFrames` 同步从 `273` 增至 `771`，用户确认“手机按住说话小车扬声器才有声音”。这符合当前按住说话、松开收听的半双工交互；iPhone182 也已确认松开后能听到小车麦克风。**异设备双向音频链路已现场接通**。持续音质、回声及视频/数据并行表现仍待最后一轮交替验收，不据此宣称长期稳定。
- 随后用户完成约 20 秒的交替短句复测，回复“两端清楚，视频数据正常”。据此可验收当前 iPhone182 经 Tailscale 私有 HTTPS → 云中继 → Windows 网关 → 小车的**短时远程双向半双工通话**，并确认这段测试中视频和实时数据并行可用。未做长时间掉线重连、舵机上电运动或公网免 Tailscale 访问验收；主板仍为 AP-only 音频诊断应用，Server酱微信推送暂停。

## 2026-09-27 手表无线只读接收联调（进行中）

- 用户确认舵机 5 V 断开、四轮架空、IDLE、无进行中的通话/必须保持的网页会话；手表外部 5 V 继续关闭，手表只由电脑 USB 供电。COM3 再次核验为主板 16 MB ESP32-S3（MAC `68:ee:8f:60:68:24`），COM7 为手表 8 MB ESP32-S3-PICO-1。
- 更新前完整主板 Flash 已读取并校验：`build/backups/main-before-watch-rx-20260927.bin`，16,777,216 B，SHA-256 `18D444010F630965FA613C15F119B60CA2BBDD76DC908DE200527248379B92A2`；旧 app0 SHA-256 与已有 audio2 应用完全相同。只写主板 app0 的只读手表接收版 1,141,376 B，SHA-256 `D243233E177BD13E99F0A366A65EF0BD4A714BA3F05BE06BA678BA7AF1483EAD`，esptool 哈希校验通过；CAM、NVS、FFat 未写。版本 `...-audio2-watchrx1`、IDLE、无急停/故障。C++14 接收状态机测试 PASS。
- 手表 USB 供电固件仅写 COM7 app0，I²C PPG 保持关闭。发现无人监听的手表 USB CDC 逐包打印会使本应 20 Hz 的 Wi-Fi 上行周期性阻塞；去掉运行时逐包打印后，COM3 连续 18 秒每秒收到约 20 个新序号，包龄约 0–40 ms、无离线。用户动作阶段连续约 38 秒序号 1928→2670，在线/校准始终为真，三类运动分量均曾非零。分段方向、归中漂移尚待核对；这仅是只读数据通路，不驱动车轮。详情 `docs/WATCH_LINK_RX_2026-09-27.md`。
- 主板重启初期一度读到 `vision_link.valid=0`；用户随后确认 CAM 已供电、本地视频正常，再测约 7 秒 valid 2224→2268、bad 1→2、resync 3→4，证明 CAM UART 在手表 20 Hz 并发期间有有效帧。MAX30102 外部 5 V、电平转换和 Zero USB/外部 5 V 并接问题未处理，用户目前无法返工、无万用表，因此未测心率/血氧、未切独立供电。
- 用户随后确认 iPad 人物框和“一根手指”手势均随视频更新。手表动作后静止仍曾持续发出 `vy=-0.45`；同姿态按 B 归中后连续 15 秒 `vy/wz=0`、`vx≈-0.013`，没有继续发散。相对航向在多次动作后累积偏移，需要在驾驶模式实现前明确归中/漂移处置，不能把当前只读链路等同于手表控制验收。音频和长时间并发仍未复验。
- 本轮回归：主板 Arduino 编译 PASS，手表 Arduino 编译 PASS，主板接收状态机 MinGW C++14 测试 PASS，Python 39/39、网页 Node 24/24 PASS，`git diff --check` PASS。仓库内旧 `.venv` 指向另一台 Windows 的 Python 路径，故 Python 回归改用本机 Anaconda Python 3 的 pytest；不影响测试结果。未提交或推送本轮改动。
- 最终 8 秒串口复查：手表 `online=true`、已校准、包龄 39 ms、归中后 `vx=-0.015/vy=0/wz=0`；主板仍 IDLE、无急停/故障，CAM valid 4838→4894，bad 2、resync 4 均未增加。用户确认本地视频、人物框及一根手指手势正常。此结果仍不能替代外部供电、心率血氧或电机运动验收。

## 2026-09-27 手表心率血氧代码复用（进行中）

- 按用户要求，手表侧 `watch_health.h` 已改为复用主板当前 DEMO_BALANCED 的 `DemoPpg`、`TimedMetric` 和脉搏周期估算，输入源仍是手表 MAX30102 独立 I²C，结果沿现有 `watch_v1` UDP 的 `contact/hr/spo2/sqi/valid/held/age` 字段传输；主板接收代码无需改变，仍只读显示、不参与车轮运动。
- 手表合成波形和状态回归 `watch_native_test PASS`（75 BPM、91% 演示血氧；无波形保持、采样超时和无接触清除）。主板接收状态机新增有效/失效健康字段测试，MinGW C++14 PASS。默认固件 `WATCH_ENABLE_PPG=0` 编译 PASS，程序 926,163 B / 全局 RAM 53,216 B；启用版仅做编译检查，不在未确认电压的焊接线上烧录启用。
- 电气未验收：MAX 模块 D/C 已直接焊于 Zero GPIO6/7、可能为 5 V 上拉；Zero USB 与外部 5 V 同接的反灌状况也未测。现场无万用表且暂不能返工。因此尚无真实 MAX 采样或血氧准确性证据，不应让用户此刻拔 USB 改外部独立电源。
- PPG 启用版另以编译参数 `WATCH_ENABLE_PPG=1` 仅离线编译 PASS：程序 927,299 B / 全局 RAM 53,216 B，app SHA-256 `F79736400D17581AC5EA6F89ED19C241EFC1201D2EBF5931CA7BAF53F0344029`。此版未烧录。默认关闭版 app SHA-256 `178B1D2D62F49B4A02A9207E781EA96372B8364C3DBE8446EE96AFF94FA734A2`；待确认 COM7 供电状态后才能写入并做无 PPG 的链路回归。
- COM7 再次识别为手表 N8R8 MAC `ac:27:6e:d2:c7:ec`，此前整片 8 MB 备份尺寸/哈希核对一致；仅将默认 PPG 关闭版写入手表 app0 @0x10000，esptool 写后哈希校验通过。COM7 启动日志 `MPU=1`、`PPG_DISABLED_UNTIL_LEVEL_CHECK`、`WATCH_WIFI_CONNECTED ip=192.168.4.3`；COM3 主板连续约 8 秒 `watch_status` 序号 847→988、约 20 包/秒、在线/已校准、包龄 0–39 ms、静止动作约 0，健康有效标志均为 0（预期）。外部 5 V 未开启，真实 MAX、腕部数值及独立供电仍未验收。

## 2026-09-27 手表健康数据 OLED 来源显示

- 按用户已确认的 IDLE、舵机 5 V 断开、四轮架空、无通话且主板 USB 稳定条件，主板 OLED 现按同一来源选取一对心率/血氧：手表链路在线且报告接触时选手表，其他情况回落车载；右侧 `W`/`C` 标明来源，有接触但数值未成熟时显示 `--`，保持值显示 `H`。只读手表接收不参与运动。手表 PPG 未启用，因此当前无法现场验收手表数值或 `W` 实际显示。
- Arduino CLI 用 16 MB / `app3M_fat9M_16MB` / OPI PSRAM 和当前 AP-only、音频网关宏编译 PASS：程序 1,141,366 B，全局 RAM 59,640 B；app 长度 1,141,520 B，SHA-256 `419F015D6F7397BC9D76E154B7A27FDE6BD9D08FCA5ACC652E0964DCED873A5C`。MinGW C++14 `watch_link_test` PASS；`git diff --check` PASS。
- COM3 只读核验为主板 ESP32-S3、16 MB Flash、MAC `68:ee:8f:60:68:24`。烧录前只读备份当前 app0 3,145,728 B 至 `build/backups/main-before-watch-oled-20260927-app0.bin`，SHA-256 `EFF7B891E660042DDDD6CB695765151B0DE8F5C37DAA823FA31271C3F9219EB0`，其前 1,141,376 B 与旧 watchrx1 app 二进制逐字节一致。随后仅写主板 app0 @`0x10000`，esptool `Hash of data verified`；未写 CAM、NVS、FFat 或手表。
- 实机串口核对新版本 `...-watchrx1-oled1`、AP 在线、模式 IDLE、无急停/故障；手表序号 38139→38280，每秒约 20 包，包龄 0–39 ms；CAM valid 543→606、bad/resync 维持 0。手表 HR/SpO2 有效标志仍为 0，符合 MAX 未上电。OLED 屏幕的人工目视和网页刷新结果待用户回复，不把串口当作显示验收。
- 用户请求跳过电压核对直接接通手表独立 5 V。商家模块原理图外侧 I²C D/C 由 `5V_PU` 上拉，JP2 可接内部 5 V，而 D/C 已直焊 Zero GPIO6/7、无万用表且不能返工；用户确认外部 5 V 尚未接通。因此保留手表 `WATCH_ENABLE_PPG=0`，不把离线已编译的 PPG-on 二进制烧入手表，也不要求用户开始独立供电测试。软件无法消除可能的 5 V I/O 或 USB 反灌风险；实物条件未满足。
- 用户目视确认主板 OLED 心率/血氧旁显示来源字母 `C`，本地网页视频正常。由此验收当前车载来源显示与网页视频回归；手表 `W` 来源及真实腕部心率/血氧仍未验收。

## 2026-09-28 四角红外防跌落（右前单路已烧录，待触发实测）

- 用户给出四个 YL-62 规划为 3.3 V 供电、共地，越过灰白桌面时 OUT 高；左前/右前/右后/左后 OUT 拟接主板 GPIO40/41/47/48。随后明确**目前仅右前 GPIO41 实际接好**，其他三路未接；用户明确右后阻止的是**向右横移**。
- 在主板 5 ms 安全输出路径新增四角高电平即时拦截、低电平连续 30 ms 后释放。仅 MANUAL 模式：左前禁 `vy<0`、右前禁 `vx>0`、右后禁 `vy>0`、左后禁 `vx<0`；任一角高电平禁 `wz!=0`。对角线只去掉被禁分量，其他方向保留。当前 `CliffInstalledMask=0x02`，**仅右前实际启用**；另外三角逻辑虽已实现但未启用，待实物接线验证后改为 `0x0f`。遥测新增 `cliff` 状态，串口 `wireless_status` 新增 `cliff_mask` 和已安装位掩码。详情 `docs/CLIFF_GUARD_2026-09-28.md`。
- 新四角 C++ 测试 PASS，已纳入 `tools/check_firmware.py`；原有 8 套 C++ 固件测试 PASS，网页 Node 24/24 PASS，`git diff --check` PASS。Arduino CLI 1.5.1 / ESP32 core 3.3.10 在 ASCII 临时目录以当前 AP-only、音频网关参数编译 PASS：程序 1,142,166 B（36%）、全局 RAM 59,680 B（18%）。第一次直接在中文工作区编译时链接器路径失真，故改用临时目录；最终镜像 `build/cliff-fr1-20260928-app.bin` 为 1,142,320 B，SHA-256 `91D395B3E75C3C61E448A59124A5D3504D21D37E71B64BC3E32B35D3CD12D398`。
- 用户确认四轮架空、舵机 5 V 断开、IDLE、无通话、右前 3.3 V 供电并许可更新主板 app；COM6 只读识别为原主板 MAC `68:ee:8f:60:68:24`、16 MB Flash。更新前完整 Flash 已读入 `build/backups/main-before-cliff-20260928-full.bin`，16,777,216 B，SHA-256 `3D45B208C3B48229620373A8B8A0CEE8BB013FDEC89576C22033EFF36491F979`。新镜像分区表与备份原表逐字节一致。仅写 COM6 主板 app0 @`0x10000`，esptool `Hash of data verified`；未写 CAM、FFat、NVS、分区表或 bootloader。
- 烧录后 COM6 115200 检查：固件后缀 `-clifffr1`、AP 在线、`mode=IDLE`、`fault=false`、`estop=false`、`cliff_installed_mask=2`、静置 `cliff_mask=0`，8 秒无重启迹象。CAM 的 `vision_link.valid` 此时为 0，尚不能据此判定视频断链，待网页与 CAM 实测。右前红外 LOW→HIGH→LOW 的现场状态和电机输出均尚未验收；车轮、舵机仍不运行。
- 其后 COM6 连续观察到 GPIO41 对应的 `cliff_mask` 在 `0` 与 `2` 间反复变化；下一段 8 秒读数持续为 `0`。已询问用户跳变时是否正移动灰白测试纸，以区分有意触发与电位器临界抖动。无车轮运动，不能把串口状态变化称作防跌落实车验收。
- 只读检查 GitHub：`main` 仍为 `2b1016c`；新网页改动在 NeedleAss 的 `codex/web-call-ui` 分支，`247e36e` 比 `main` 超前 1 提交，涉及 `remote-hub/relay/public` 六个文件和一项触摸测试。该更新未合入 `main`、未同步本地/部署服务器；本轮不擅自合并以免覆盖已有未提交手表/OLED 工作。GitHub 仓库元数据目前显示 `visibility=public`，与用户此前希望仓库仅本人和 NeedleAss 可访问的要求不符，应由有管理员权限者在 GitHub 设置中改回 Private 并审核历史公开内容。

## 2026-09-28 五指切换手表控制、双红外与 GitHub 通话网页

- 用户确认当前仅右前 OUT→GPIO41、左后 OUT→GPIO48，两路 3.3 V 供电/共地、悬空输出高；左前和右后尚未接。手表只保留 Zero＋MPU6050，无 MAX30102。车轮架空、舵机 5 V 断开。安装掩码改 `0x0a`；右前阻止前进、左后阻止后退，任一路高电平阻止两种旋转，在手动和新手表模式均生效。未安装的两角不提供防跌落覆盖。
- 新增 `WATCH_CONTROL`：CAM 连续识别五指动作进入，再次放下/重新五指后退出；进入需新鲜已校准手表包，并先收到**进入后的新中立包**才允许运动。手表姿态持续保持且按约 20 Hz 更新才持续运动，回中立、B 键重新归中、第二次五指、网页 IDLE/急停或手表包超过 350 ms 未更新均停车。网页只显示手表模式，不允许网页直接请求进入。前向超声和车体倾斜保护继续生效；详情 `docs/WATCH_CONTROL_2026-09-28.md`。
- GitHub `codex/web-call-ui` 提交 `247e36e` 的远程页面变更已同步到本地 `remote-hub/relay/public`；额外修正两端网页对 `WATCH_CONTROL` 的遥测解析与中文/英文显示。主板 AP 页面仍沿用 R5 主布局，其电话按钮明确提示改用 Tailscale HTTPS 家长端（普通 `192.168.4.1` HTTP 无浏览器麦克风权限）。Node 本地页 25/25、远程中继 12/12 测试 PASS；手表模式上报可显示但不可从网页发起有两端自动回归。暂未将工作区推回公开的 GitHub 仓库。
- 远程页面 tar SHA-256 `DABA720A7BF5FF322240D57748FFB14BCEACE193C2112E934596591946F4A5E3` 已在现有服务器先备份后部署，备份 `/opt/carerover/backups/relay-before-top-level-call-20260927T184351Z.tar`，服务 active、本机 `/health` 正常。新 UI 把**同一个**视频节点移到顶层通话弹窗，不另占 CAM 视频连接；通话仍使用原有 16 kHz 双向音频网关。部署时 Windows 车端网关未运行，`controlDevice/videoFresh=false`，故真实远程媒体需网关恢复后再验收。
- 固件原生十套测试 PASS，Arduino CLI 在 ASCII 目录编译 PASS（程序 1,143,014 B，静态 RAM 59,696 B）。用户许可更新后只读识别 COM6 为原主板 MAC `68:ee:8f:60:68:24`、16 MB；更新前整片备份 `build/backups/main-before-watchctl-cliff2-20260928-full.bin`，16,777,216 B，SHA-256 `B55C45C32FCC9C5745BE5B45A1EB2783B168C39B9A8DD6F6069EEC6DCA1063EC`。新编译分区表与实机备份逐字节一致；只写 app0 @`0x10000`（1,143,168 B，SHA-256 `78E4BB986A872F355C42A78A893A0E5F6D6B36E20814F97430680E3EBC8D4CBC`）和 FFat @`0x610000`（10,354,688 B，web version `1f8c9fc1777e29c4`，SHA-256 `7B2C0D2DA1DF2240E80A5D0103720B40B0A9348A5B4BFEA09BC399546E27CB6B`），两段均 `Hash of data verified`；未写 CAM、NVS、bootloader、分区表或手表。
- 复位后 COM6 12 秒串口显示 `...-watchctl1-cliff2`、AP 在线、IDLE、无 fault/estop，`cliff_installed_mask=10`，CAM `vision_link.valid` 442→489 且 bad=0，安全任务间隔峰值约 6 ms。手表尚未通电时 `watch_status.online=false` 符合预期。架空状态 GPIO48 对应位在 `cliff_mask=10` 和 `2` 之间跳变，不能据此断定已校准；须人工对灰白桌面/悬空做稳定电平对照。尚未做运动和真实五指切换验收。
- 此刻 Windows 的 WLAN 是互联网链路（183.173.103.205），并未连接小车 AP；为遵守“不切断电脑互联网”，未擅自切 WLAN，已请用户先建立独立 USB 上网。Windows 网关尚不能恢复，远程媒体链路待 dual-network 条件满足后验证。
- 用户之后建立 iPhone USB `172.20.10.10` 独立上网，WLAN 已是 CareRover `192.168.4.4`；绑定 USB 源地址的 SSH 到云服务器成功，局域网主板首页 HTTP 302 正常，`/?transport=ws&video=mjpeg` 实机 HTML 标记 web version `1f8c9fc1777e29c4`。`gateway/start-windows.ps1 -EnableAudio` 已启动且保持运行，状态 `boardUp/relayUp/cameraUp/audioBoardUp/audioRelayUp=true`、视频帧持续上传。私有 Tailscale HTTPS `/health` 返回 200，中继经回环健康检查 `controlDevice=true/videoFresh=true`；`audioPaired=false` 因没有家长端正在通话。CAM 视频流曾短暂自行重连，本地其他视频标签关闭后连续上传，目前未测真正双端新 UI 通话听感。
- 手表仅 Zero＋MPU，经 USB 上电后主板连续收到约 20 包/秒、`watch_status.online=true`、`cal=1`、包龄约 0–52 ms、静止 `vx/vy/wz≈0`；MAX 未连接，HR/SpO2 有效标志均为 0。CAM `vision_link.valid` 同时持续增加且 bad=0。五指开/关模式尚待实际手势验收。
- 红外实物对照：右前对灰白桌面、仅左后悬空时 10 秒每秒 `cliff_mask=8`；左后对桌面、仅右前悬空时约 9 秒每秒 `cliff_mask=2`；两路均对桌面时约 8 秒每秒 `cliff_mask=0`。第一轮移动对照的时序不清，故以三段稳定记录为准。证明两路输入能独立识别现有测试面，但没有驱动车轮，也不证明未安装角落安全。
- 首次实物五指尝试被 CAM 分类为 `four`，未切模式；用户张开拇指重试后，CAM 连续识别 `five` 并产生 `gesture_action FIVE ok=true`，主板进入 `WATCH_CONTROL`。该次车身被倾斜放置，IMU 触发 `tilt_fault`，模式立即回到 IDLE 且速度归零。车身直立后再次五指成功进入手表模式，主板约 2 秒保持 `WATCH_CONTROL`，手表中立 `vx/vy/wz=0`、红外掩码 0；记录见 `build/windows/main-watch-five-upright-20260928.jsonl`。
- 首次手表动作记录中，`WATCH_CONTROL` 收到转动目标 `wz=0.35` 以及平移目标 `vx=0.254, vy=0.091`；随后车身 IMU 自身升至俯仰约 46°、横滚约 60°，保护退出 IDLE、目标速度清零。用户确认当时连小车本体也一起拿起，并非手表数据单独致停。车身固定后 IMU `tilt_fault=false`、红外 0；但随后 70 秒复试无新手势，因为 CAM 实际未上电，`vision_link.valid` 全程固定 12862。手表包仍在线且变化，但模式保持 IDLE、目标速度始终 0，所以该次“无运动”是预期安全行为。
- 用户给 CAM 上电后，同一串口反馈回路由 `vision_link.valid` 70 秒增量 0 变为 8 秒增量 52，链路恢复；未改固件。又一次五指在 CAM 仅连续维持约 0.9 秒（随后变 `no_hand/no_gesture`），未达到切换判定，主板仍 IDLE；尚未完成车身固定条件下的完整手表动作/归中/第二次五指退出与舵机上电运动验收。远程新通话 UI 的异设备现场复验也尚待完成。
- iPhone182 经独立互联网/Tailscale 打开新版远程页，用户确认“已连接，可以通话”；中继 `/health` 同期 `controlDevice=true`、`videoFresh=true`，`parentFrames` 从 0 增至 119，车端网关亦记录 119 帧下行。此轮用户未逐项回答视频/人物框/音质细节，因此仅将短时通话链路记为恢复，不能证明长时间并发稳定。
- 用户反馈通话时远程“手动”按钮完全点不动；主板另一路只读 WebSocket 遥测显示 `mode=IDLE`、`estop=false`、`control_allowed=true`、`supported_modes` 含 `MANUAL`、`motion_output_installed=true`，说明固件没有主动禁用手动模式。远程网页当前用 `showModal()` 打开占满手机屏幕的通话框，后方控制按键不能同时操作；但用户尚未做“关弹窗后按钮是否可点”的 A/B，故根因只能列为最可能，不能宣称已修复。另舵机 5 V 一直断开，即使命令到达也不会有实际轮子运动。用户要求烧录完毕后暂时停止，后续再验收；没有擅自给舵机上电、放开防跌落/倾斜保护或改动通话 UI。

## 2026-09-28 四路红外与 CALL 来电手势更新

- 用户确认四路 YL-62 接线为左前 GPIO40、右前 GPIO41、右后 GPIO47、左后 GPIO48，3.3 V 供电/共地、悬空时 HIGH；车轮架空、舵机 5 V 断开、IDLE、无通话并许可更新。`CliffInstalledMask` 改为 `0x0f`，既有手动/手表模式过滤逻辑覆盖四个方向及任一角禁止旋转；不将此保护宣称覆盖人物跟随或其它自主模式。
- CAM 所用 ESP-DL 模型类别定义含小写 `call`，主板转大写并经重复帧锁存后切换 `call.requested`、递增 `call.sequence`。远程家长网页按新序号显示来电/结束通话；iPhone 浏览器麦克风仍必须由家长点击接听授权，不会静默自动接听或后台推送。
- GitHub `main` 与本地 HEAD 均为 `2b1016cfdd9d5b1fc5d1a2c5ddf58a56acb706a3`（对比接口 0 提交差异）。远程页面 CALL 增量已部署到服务器，旧版备份 `/opt/carerover/backups/relay-before-top-level-call-20260928T145502Z.tar`；主板 FFat 为当前 R5 页面加版本/协议文档更新，并非另有未同步的 main 提交。
- Node：主板网页 25/25、云中继 14/14、Windows 网关 2/2 PASS；新 CALL 和四角原生 C++ 测试 PASS。Arduino CLI 在 ASCII 暂存目录编译 PASS。烧录前完整 Flash 备份 `build/backups/main-before-cliff4-call-20260928-full.bin`，16,777,216 B，SHA-256 `402459447B03F27EB29BB164B2C899ECC2C7B5A29416E908B86769CEC253436C`。实机分区表与新镜像逐字节一致。COM6 确认为主板 MAC `68:ee:8f:60:68:24`。仅写 app0 @`0x10000`（SHA-256 `1FB05377FE8131A2EF67D3B3937ABAB86A626214C9C643168413B277F1D65EBB`）及 FFat @`0x610000`（版本 `a33499357c5bccf5`，SHA-256 `CB89383A61AD17EC0C94AC61F128B17849ACF8F7D95FA2F5DF7E4B203BF9174A`）；两段均 `Hash of data verified`，未写 CAM/NVS/bootloader/分区表。
- 复位后 COM6 12 秒串口确认固件后缀 `-cliff4-call1`、AP 在线、IDLE、无 fault/estop、`cliff_installed_mask=15`。但 `cliff_mask=15` 持续为全高，且此时 `vision_link.valid=0`、IMU 未就绪；需现场核实四探头是否对准桌面、模块供电及 CAM 供电。**未**完成四路 LOW/HIGH/LOW 对照或真实来电手势验收，舵机未上电。
- Windows 独立 iPhone USB 网卡 `以太网 3` 在线并承载默认互联网路由；WLAN 已接 `CareRover-EE68`，主板 HTTP 302→200（约 0.15 秒），CAM `192.168.4.2` 连接超时。Windows 网关音频/控制已连服务器，`boardUp=true`、`relayUp=true`、`audioBoardUp=true`、`audioRelayUp=true`，但 `cameraUp=false`，远程视频尚不可用。需先恢复 CAM 再做用户侧通话验收。
- 用户随后明确澄清：**此刻 CAM 与四个红外模块都尚未接线**，与此前“接线确认”的回复不一致。因四个输入均配置上拉，未接线时 `cliff_mask=15` 正是预期的失效保护，不能将其诊断为坏模块或已完成防跌落验收；CAM 未接线也解释 `vision_link.valid=0` 和视频超时。保持舵机 5 V 断开、车轮架空，待接线后分别做四角低/高电平对照和真实 CALL 手势/远端通话验收。

## 2026-09-29 通话期间暂停超声实测

- 用户明确要求通话期间关闭 HC-SR04、结束后恢复，不新增急停；现场确认四轮架空、舵机 5 V 断开、IDLE、无通话、主板 USB 稳定，授权完整备份后烧录。现有前方超声避障在通话期间没有新测距，不能视为仍可用；四角红外逻辑未改。
- 中继家长 `/audio` 建立/断开时经 Windows 网关向主板音频 WebSocket 发送 `call_state`，连接期间每秒续租；主板 3 秒无续租自动恢复。主板暂停时 TRIG 保持 LOW、不再触发新测距；遥测增加 `front.call_paused`，串口状态为 `PAUSED_CALL`。
- Arduino IDE bundled CLI 1.5.1 / esp32 core 3.3.10、现役 `CAREROVER_DIAG_AP_ONLY=1` 与 `CAREROVER_AUDIO_GATEWAY=1` 参数在 ASCII 暂存目录完整编译 PASS：应用 1,143,982 B（36%）、全局变量 59,720 B（18%）。新应用镜像 `build/call-sonar-pause-20260929-app.bin` 为 1,144,128 B，SHA-256 `8A904FE28CFDF74A04C0177D7A61AE5A6519B68B7F73063D13515CDC85FE7C59`；与实机备份的分区表 3,072 B 逐字节一致。Node 回归：本地网页 25/25、中继 15/15、网关 2/2 PASS；独立续租 C++ 测试 PASS。
- COM6 只读确认原主板 ESP32-S3 rev0.2、MAC `68:ee:8f:60:68:24`、16 MB Flash。更新前整片备份 `build/backups/main-before-call-sonar-pause-20260929-full.bin` 为 16,777,216 B，SHA-256 `F4EA04B6780F658F4484834A4FCC63BD202CB6451BE39E229A6A90C466CF64AB`。仅写 app0 @`0x10000`，esptool `Hash of data verified`；CAM、FFat、NVS、bootloader、分区表和手表均未写。复位后串口 `IDLE`、`estop=false`、`fault=false`、`ap=true`。
- 服务器旧 `server.mjs` 已备份至 `/opt/carerover/backups/relay-server-before-callpause-20260929.mjs`；仅部署本次差异，服务器新文件 SHA-256 `CE720218029EEFE3DB4373029A31F434528B8A6FB92757F38EEE26E3D97F9925`。服务重启后回环 `/health` HTTP 200。Windows 音频网关重新加载，Tailscale HTTPS `/health` 直连 HTTP 200、证书验证通过；车端 `controlDevice=true`、`videoFresh=true`。
- 12 秒无声音/运动的授权家长音频连接现场测试：主板串口从 `front_status=UNKNOWN` 变为 `PAUSED_CALL`，挂断后返回 `UNKNOWN`（45 秒内共收到 227 条前方状态）；远程视频/控制仍在线，`audioPaired=false` 为挂断后的预期值。此测试确认状态传输与固件暂停/恢复路径，不等同于示波器证明 TRIG 物理脉冲数；本轮未做真实人声噪声 A/B 或车轮落地运动验收。当前超声回波始终无效，原始 `UNKNOWN` 还需按硬件接线/摆位单独排查。

## 2026-09-29 离座提醒网页与心率血氧复核

- 用户授权在 IDLE、四轮架空、舵机 5 V 断开、无通话及主板 USB 稳定的条件下更新。COM6 只读识别为原主板 ESP32-S3、16 MB Flash、MAC `68:ee:8f:60:68:24`。更新前整片 Flash 备份 `build/backups/main-before-care-alerts-20260929-full.bin` 为 16,777,216 B，SHA-256 `AED5B63860DE741E152DEDB7584F0EEE35430A9FDAAF6FE7AE6EE1120E475655`，含设备配置与密钥，仅保存在忽略目录，不得上传。
- 新局域网页 FFat 镜像 `build/care-alerts-20260929/ffat.bin` 为 10,354,688 B，SHA-256 `A0AD5B4C752E1A07682608205EF022199A77EB6ABC91E698555567D8EF4A2823`；**只写** FFat @`0x610000`，esptool 报 `Hash of data verified`，未写主板应用、CAM、NVS、分区表或手表。Windows WLAN 重连 `CareRover-EE68` 后 IP 为 `192.168.4.3/24`；实机首页 302→200，网页版本 `b2df6d7fcd6e8315`，`/js/care-events.js` 与 `/js/care-banners.js` 均 HTTP 200。浏览器视觉/弹窗验收尚未进行。
- 本轮尝试完整编译主板应用两次均卡在 Arduino 库检测，已中断；**没有新的应用镜像，更没有烧录手势固件改动**。CALL/FIVE 连续手势锁存的源代码与孤立 C++ 测试已入库；须待完整构建和备份比对后另行烧录、实测。
- 心率血氧诊断：手指未贴传感器时串口为 `finger_present=false`，贴住发光/感光面约 15 秒后为 `finger_present=true`，红光/红外原始值升高且算出心率、SpO₂。用户确认主板 OLED 的**数值与波形都有**。本轮无需为此更改传感器固件；演示数据不可用于医疗判断。

## 2026-09-29 人脸驱动的离座/入座提示（本轮）

- 按用户新要求，将本地和远程 `seat_left` / `seat_returned` 统一改为：CAM 在线且连续 30 秒有新鲜、非预测的无人脸结果后提示“离座提醒”；此后新鲜人脸再次出现才提示“入座提醒”。CAM 断线和数据过期不计时。原超声 `front.seated` 不再驱动页面入座/离座文案或消息；前方距离显示与避障安全功能保持不变。两条顶部提示与 R5 表面样式一致，约 6.5 秒自动清除；入座事件立即清除仍显示的离座条；快照不重弹旧提示。该逻辑只表明人脸可见性，不能证明儿童真实离座。
- 离线 Node 验证：局域网页 29/29、远程中继 21/21 PASS，包含超声变化不触发提醒、CAM 断线重置计时、顶部提示自动清除与重连不重弹。`git diff --check` PASS。原生 iPhone Xcode 工程不在本机；`remote-hub/docs/IOS_CLIENT_API.md` 已更新 `seat_left` / `seat_returned`、`display_ms:6500` 契约，App 实际 UI 接入尚未验收。
- 私有中继增量包 SHA-256 `2B679753E9A8EDFC843E6BAA19BDFF2FA11C1366B7737CD21CCAC09C8E1A9899`，上传后经脚本校验、备份并重启 `carerover-remote-hub.service`；回退包 `/opt/carerover/backups/relay-before-care-alerts-20260929T002522Z.tar`。服务 active，回环 `/health` HTTP 200；主板备份/烧录期间设备链路暂时离线，不将此记为端到端通过。
- 用户确认 IDLE、四轮架空、舵机 5 V 断开、主板 USB 稳定且无人通话后，只读识别 COM6 为原主板 ESP32-S3 rev0.2、MAC `68:ee:8f:60:68:24`、16 MB Flash。更新前完整备份 `build/backups/main-before-face-seat-20260929-full.bin`（16,777,216 B，SHA-256 `A79DCB7BD991A667281290B7E78C0AC5732C0B8FEA4571C0D7C465D11467BDA2`）仅保留本机忽略目录，分区表与上一完整备份相同，旧应用和 FFat 的哈希与历史镜像一致。
- 新局域网页版本 `fa93f2cccdb1560a`，FFat 镜像 `build/face-seat-20260929/ffat.bin`（10,354,688 B，SHA-256 `A8D0B45AFEE8275B0F17BA3DE3809751EA292F227F94B436006E89E7CFDB3A56`）。仅写主板 FFat @`0x610000`，esptool 报 `Hash of data verified`；未写主板应用、CAM、NVS、引导或分区表。复位后 `CareRover-EE68` 热点可见且信号 99%，但 Windows WLAN 尚未重新关联，故局域网页与远端视频/事件的用户侧实机验收目前 **NOT RUN**，等待用户重连双网络。
