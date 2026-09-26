# Windows AI 完整交接说明

> 本文保留早期集成快照与改动索引；从 Git 接手时以 [当前 Windows 整机与超声波交接](WINDOWS_ULTRASONIC_HANDOFF.md) 为入口。下文旧提交号、“未提交/未上传”、历史 COM 口及 ZIP 资源描述不代表当前源码或设备状态。

交接日期：2026-09-12。本文件提供不依赖上一段聊天历史的上下文。根目录 `AGENTS.md` 是自动发现入口，`WINDOWS_START_HERE.md` 是操作者入口。下一项工作是 **Windows 环境复现与实物集成验收**，不是重新编写已完成模块。

## 1. 当前状态与源码权威来源

- Git 基线 `feat/main-wireless` / `115898bab9eb852725691f20052ed2228dd47c04`。Mac 上的集成改动未提交、未上传 GitHub。ZIP 包含这些实际改动；只检出该提交会丢失本轮集成。
- 根目录是最新工作源码；原交接目录共 123 个文件，按字节保留。里面旧固件/厂商工程是参考，不是最新集成运行入口。
- 本包没有 `.git`、Mac 虚拟环境、ESP-IDF 安装、下载的 ESP-DL 依赖或本轮编译缓存。工具会按锁定版本取回依赖；构建支持无 `.git` 解压目录。
- `EXPORT_INFO.json` 给出导出来源；`SOURCE_MANIFEST.json` 给出实际文件指纹；`EVIDENCE_MANIFEST.json` 给出 Mac 证据指纹。先运行 `py -3.12 tools/verify_handoff.py`，修改后校验报告变化是正常现象，但先保存原包。
- Mac 最终主控内容版本 `8173f3a5e239de21`，网页 `86015d4f6fc2c68f`。后续只修改打包/来源追踪工具和交接文档，没有修改该固件及网页运行代码。Windows 再编译以新生成 manifest 为准。
- Mac Node 14/14、Python 20/20、三个固件 C++ 测试套件及 tracking sanitizer 通过；主控 observe/manual/follow/legacy、CAM gesture/vision/stream/pico、校准工程通过编译。浏览器证据来自注入 WS 和 Mock 视频。具体记录在 `evidence/`，不是 Windows 实测结果。
- 未发现连接的开发板，未烧录、未测电流或停车时间。历史主控 COM6 是 standalone 舵机固件，需现场读取确认；恢复完整健康/OLED 功能要烧录聚合固件。

## 2. 哪些文件改了、改动目的是什么

以下路径均相对于项目根，适用于 Windows 解压后的路径。单独列出的新模块不要被旧工程覆盖。

| 模块 | 文件入口 | 已完成工作 / 后续定位用途 |
|---|---|---|
| CAM 集成工程（新增） | `firmware/cam_tracking/main/main.cpp`、`main/CMakeLists.txt`、`sdkconfig.defaults`、`partitions.csv` | 保留原相机引脚、RGB565BE、QVGA；单调度器交替手势与人脸，目标 IoU 匹配、UART 编码，四个构建变体；现场验证模型表现/帧率 |
| CAM 视频（新增） | `firmware/cam_tracking/main/video_server.cpp`、`latest_frame.h` | 有界 JPEG 缓存、读者生命周期、单观看者，慢连接不会持有推理帧；现场检验内存与断流 |
| UART 协议（新增） | `firmware/main_wireless/vision_protocol.h` | CAM/Main 共用 CRC8、G/P 语义校验、半包/粘包/溢出恢复、序号与重同步；故障查 `vision_link` 日志 |
| 主控入口（修改） | `firmware/main_wireless/main_wireless.ino` | 新 adapter 接入、来源分离、周期诊断；保留 MAX30102/OLED 算法路径 |
| 无线与任务聚合（修改） | `wireless_runtime.cpp`、`wireless_runtime.h`（主控目录） | observe/manual/follow 配置、支持模式、10 Hz 遥测、IMU/运动任务、所有者 ping，DHCP 为 CAM 保留 .2 |
| 安全状态机（修改） | `firmware/main_wireless/safety_controller.h` | 手动/跟随独立有效期、所有者约束、故障/急停/校准门禁、立即停止；连续零命令不覆盖首次停止时刻 |
| 跟随控制（新增） | `firmware/main_wireless/person_follow.h` | 三个新匹配框启动、面积中位数、EMA、横移/转向滞回与限幅；参数集中于 FollowConfig |
| IMU（新增） | `mpu6050_soft.h`、`imu_filter.h`（主控目录） | GPIO6/7 有界开漏软件 I²C、5 秒静止校准、互补姿态/相对 yaw、倾角故障及恢复；现场测采样/总线性能 |
| 舵机与 NVS（新增/合并参考） | `continuous_servo_drive.*`、`omni_kinematics.h`、`motion_calibration.h`（主控目录） | 实际四路 LEDC、X-drive、每轮 neutral/invert/span、verified 门禁、正常缓动/安全中值；无编码器闭环 |
| 校准工程（新增） | `firmware/motion_calibration/` | standalone 校准命令，增加分轮 span 与显式确认；先通过工具生成完整 sketch，再进行现场校准 |
| 网页协议与状态（修改） | `js/protocol.js`、`js/state.js` | URL 统一校验、query→遥测→同源优先级、来源 seq/age 防旧框续期、IMU flags、能力模式 |
| 网页交互/视频（修改） | `js/app.js`、`js/video.js`、`js/sim.js`、`index.html`、`css/app.css`、`js/i18n.js` | 跟随所有者保活、退出/隐藏释放、MJPEG 动态切换和重试、相对航向/IMU 状态、Mock 故障场景 |
| 构建与配置（新增/修改） | `tools/carerover.py`、`tools/tracking.py`、`config/tracking-development.json`、`config/cam-*.json`、`config/cam-dependencies.lock` | 主控与 CAM 分板构建、编译包禁止烧录、备份/显式分区写入、锁定依赖、来源追踪、串口采集与发布 |
| 自动检查（新增/修改） | `tests/firmware/`、`tests/fakes/`、`tests/tracking.test.js`、`tests/test_tracking_tools.py`、`tests/browser/`、`tools/check_firmware.py` | 直接测试固件纯 C++；驱动 fake 只替换 Arduino 引脚 API；浏览器场景不是实物录像 |
| 交接工具（新增） | `tools/verify_handoff.py`、`WINDOWS_START_HERE.md`、`AGENTS.md` | ZIP 接手、清单完整性、脱离 Git 构建与续作约束 |

`docs/protocol.md` 是当前网页/设备增量接口说明。旧 `docs/wireless-*` 保留 legacy 阶段语义；集成部署以 `docs/tracking-development.md` 为入口。

## 3. 不应改变的接口与范围

- 主控 AP `.1`、CAM STA `.2`（完整地址 192.168.4.x），视频 `http://192.168.4.2/stream`。CAM 8 MB / 主控 16 MB 的示例分区不可混用；现场核实硬件。
- UART 115200，`@G`/`@P`、CRC8 0x07/init0；320×240 坐标。不要通过重复发送旧结果给来源续期。
- MPU6050 GPIO6/7；OLED GPIO4/5；MAX30102 GPIO8/9。不能假设存在第三个硬件 TwoWire 控制器。
- FL/FR/RL/RR GPIO10/11/12/13；vx 前进、vy 右移、wz 顺时针。具体轮向必须实测校准。
- 人脸检测跟随，无身份注册、无自动搜索；三帧置信度 ≥0.60，面积是距离代理。低置信度/丢目标停车。
- 安全优先于模式；内部速度/保活/输出 240 ms，CAM/目标 490 ms，IMU 100 ms；安全任务目标 5 ms。机械停车上限必须用实物证据测量。
- 急停解除、重启、故障恢复不能自动续跑；未 verified 校准不允许运动；低压仅有故障接口，真实采样尚未实现。

## 4. Windows 下一步任务和阶段出口

| 阶段 | AI 可推进的工作 | 需要的现场信息/操作 | 何时才能进入下一阶段 |
|---|---|---|---|
| W0 接收与盘点 | 校验、读文档、枚举环境、安装精确依赖、主机回归、构建 compile_only | 两板型号/容量/PSRAM、接线、电源、实际串口映射 | 记录真实环境，解决平台构建失败；有对应版本和日志 |
| W1 备份 | 准备设备 profile，按工具校验、读取 Flash、记录 SHA-256/NVS 所在分区 | 唯一识别接入的目标板，串口不被其他工具占用 | 两板恢复镜像存在、大小和哈希核验；不能只有“应该备份了” |
| W2 CAM | gesture→vision→stream 分阶段构建/烧录、采集两分钟串口、分析异常 | 用户展示手势与人脸，连接 AP，观察视频 | G/P 正确、视频与推理并行，保存真实 FPS/内存/掉帧，问题已定位 |
| W3 主控 observe | 烧录只读聚合，查看 WS/遥测/日志、健康和 OLED 回归 | MPU 静置校准；用户配合健康采样及 OLED 观察 | 来源年龄、框、IMU、MAX30102/OLED 都有证据；不启动轮子 |
| W4 架空校准 | 校准工程/命令、读写 NVS、记录每轮数据 | 架空轮组、标记前向、逐轮验证中值/方向/范围、电源测量 | 操作者实测后 confirm_calibration，断电复核；null 不得当实测 |
| W5 manual | 六方向及释放/失联/故障矩阵、日志与停止证据对齐 | 可控测试区域、倾斜/断链/急停操作、录像或测量 | 控制/释放 ≤250 ms，CAM/目标失效 ≤500 ms，无自动恢复；有实际轮子停止证据 |
| W6 follow | 基于记录调参，目标静止/前后/横移/离开，记录改动 | 人脸目标与现场观察，必要时急停 | 跟随正确且故障按契约停车；未通过不能跳到长期运行 |
| W7 无线整机 | 同时运行视频/手势/健康/OLED/IMU/运动至少十分钟 | 外部供电、完全断开 USB，保存演示 | 无崩溃，真实 FPS/最低内存/停止延迟/无线演示齐全 |

不得因为现场阻塞就重复生成已经完成的集成计划。可继续独立的软件诊断、日志工具和构建修复；涉及真实轮组操作时，把所需动作说清楚交给用户执行。发现代码缺陷可增量修复，先留下失败证据，再运行相关回归和受影响固件构建。不要仅修改测试期待值掩盖安全行为回退。

## 5. 接手后第一条回复应该给用户什么

读完并完成只读核验后，给出：包校验结果；本机已有/缺失的工具；已识别与尚未确定的两块板；当前停在 W0/W1 的具体位置；下一项可执行动作和必要的人工配合。不要直接宣称设备已完成，也不要一上来启动轮子。

可以使用 Plan mode 做一次简短的现场计划对齐：计划应基于当前枚举结果，列出 W0–W7 的前置条件和证据。它是 Windows 现场执行计划，不是重新设计整套软件。用户允许执行后按阶段持续推进。Goal 可按当前用户明确授权创建；文档本身不要求自动创建 Goal 或另开任务。

## 6. 续作记录与最终回交

从 `docs/windows-progress.md` 开始，每次中断前写明当前阶段、最近通过项、未解决错误、下一条命令/操作、串口映射、所用 profile、构建版本和证据路径，使下一个 AI 不需要聊天历史。

保留本包 `evidence/` 原样，新日志放 `output/windows/<日期>/`；私有 `.local.json`、Wi-Fi 密码、完整 Flash/NVS 备份保存在用户本地，回交公共包前剔除私人配置。最终回交包括：修改源码和清单、精确环境、真实轮组/IMU 校准、阶段记录、原始故障/停车日志、至少十分钟运行日志、无线演示，以及仍为 NOT RUN 的条目。

当前 `tracking.py release` 收集 `output/evidence/`，不会自动收集 Windows 日志或解压后已有的 `evidence/`。下一次发布前将需要保留的原 Mac 证据和经检查的 Windows 证据复制进 `output/evidence/` 的不同子目录，避免同名覆盖；核验无私人信息，再发布和复核 SHA-256。不能只打源码包便宣布所有验收完成。

## 后续超声波增量

HC-SR04 前方保护和向右单障碍演示已加入当前源码；本文件上方版本号与测试计数为原集成快照。最新超声波配置、协议、测试和实物门禁见 [ultrasonic-development.md](ultrasonic-development.md)。不沿用模拟阈值作为实测标定；当前设备验收仍为 NOT RUN。
