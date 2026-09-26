# CareRover Windows 整机与超声波交接

> 本文保留 0912 环境与超声波流程。当前版本、资源表和 Agent 提示词请以 [0915 Windows 续作交接](WINDOWS_0915_HANDOFF.md) 为准；0915 原包现已入 Git，当前轮位为 GPIO10/12/13/11。下文旧提交号和旧提示词不代表最新开发状态。

适用：从 GitHub 接手当前主控、CAM、网页与 HC-SR04 集成，在 Windows 上继续接线、编译、校准和真机联调。当前不是空白工程，不需要重写网页或重新设计机器人。

## 1. 获取哪一份代码

仓库：https://github.com/NeedleAss/spider-webite-dev ，分支 `feat/main-wireless`。

功能基线为 `67fea551d5717764d47f53fa5fb8cf2e4511bd64`：主机测试及 8 个 ESP32 构建组合全部通过。后续交接文档提交在同一分支，接手时取分支最新版本并记录 `git rev-parse HEAD`。不要回退到旧的 `115898b` 无线基线，也不要用原始参考工程覆盖当前 `firmware/`。

[功能基线 CI 成功记录](https://github.com/NeedleAss/spider-webite-dev/actions/runs/34694092033)。这些是软件证据，所有 Windows/实物项目仍待现场记录，不能直接标 PASS。

在 Windows PowerShell 中执行；`C:\CareRover` 应为尚不存在的新目录：

```powershell
git clone --branch feat/main-wireless --single-branch https://github.com/NeedleAss/spider-webite-dev.git C:\CareRover
Set-Location C:\CareRover
git status --short
git rev-parse HEAD
```

若已有该仓库，先查看 `git status` 和当前分支，保存本机未提交修改及私有配置，再决定更新方式。不要覆盖旧工程、强制重置或盲目执行上述 clone 到已使用的目录。

## 2. 要转交的资源

| 资源 | Git 是否包含 | Windows 如何取得 |
|---|---|---|
| 当前网页、主控/CAM 固件、驱动、模拟器、测试、工具 | 是 | 拉取上述分支 |
| 接线与架构说明、通信协议、校准空模板、工具版本锁 | 是 | 本文及同目录 tracking-development、ultrasonic-development、protocol 文档 |
| HC-SR04 说明书、实际两板照片/型号、当前接线照片、场地/障碍尺寸 | 说明书和现场材料不在 Git | 把这次使用的 PDF 和现场照片另传给 Windows；放在 Git 忽略的 `output/windows/reference/` 即可 |
| `firmware/main_wireless/wifi_secrets.h` | 否 | 如果原电脑已配好，私下复制；否则从 `.example.h` 新建并设置设备密码 |
| `config/board.local.json`、`config/cam-board.local.json` | 否 | 有已验证的同一设备配置可私下转交；Windows 仍需核对实际板型、串口、容量和工具版本 |
| `firmware/main_wireless/front_config.local.h` | 否；当前未提供已标定文件 | Windows 核对接线并测量后填写；不要把模拟参数复制进去 |
| 轮子/超声波实测记录、备份 Flash/NVS、实物日志/录像 | 否 | 若存在则私下转交；目前不能假定已经存在，现场也要对当前两板重新备份 |
| 模型、ESP-IDF、Arduino core 与 Python 库 | 版本锁在 Git，大依赖不在 Git | 在 Windows 按锁定版本下载安装 |
| Mac `.venv`、编译缓存、本地原始参考包 | 否 | 当前源码可独立构建；Windows 重建环境，原始参考包只在需要查历史资料时另传 |

资料不足时先进行软件环境和 Mock 验证。真实引脚、电平、停车距离缺失时保留未知，不填写猜测值。

## 3. Windows 先跑通软件

前置工具：Git、Python 3.12（`py -3.12`）、Node.js 22 或兼容版本。主控后续需要 Arduino CLI 1.5.1 / ESP32 core 3.3.10，CAM 使用 ESP-IDF 5.3.4 / ESP-DL 3.3.11；不要未经验证升级版本。

```powershell
Set-Location C:\CareRover
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r tools\requirements.txt -r hardware\requirements.txt -r mock\requirements.txt
$careRoverTests = Get-ChildItem .\tests -Filter *.test.js | ForEach-Object { $_.FullName }
node --test $careRoverTests
.\.venv\Scripts\python.exe -m unittest discover -s tests -p "test_*.py" -v
.\.venv\Scripts\python.exe mock\server.py --host 127.0.0.1 --port 8080
```

浏览器打开 `http://127.0.0.1:8080/?transport=ws&video=canvas`。选择跟随，再勾选“演示绕障 · 固定向右”，观察停车、右移、余量、直行、重新确认目标、恢复跟随；再次启动绕障时测试急停。终端按 Ctrl+C 结束 Mock。

模拟正常只能记为 Mock PASS。若安装了可执行文件名为 `c++` 的 C++17 编译器，可再运行：

```powershell
.\.venv\Scripts\python.exe tools\check_firmware.py
```

该脚本使用 GCC/Clang 风格参数，不能直接把 `CXX` 指向 MSVC `cl.exe`。未装兼容编译器时记 Windows C++ 检查 NOT RUN，参考 GitHub Linux CI，并继续 Arduino 实际编译；不要伪造本机通过。

## 4. 构建命令和本地配置

以下只编译，不烧录。先确保 `arduino-cli` 在 PATH，或设置项目工具支持的 `ARDUINO_CLI` 为实际可执行文件完整路径。

```powershell
arduino-cli version
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.10 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
.\tools\windows.cmd environment
.\tools\windows.cmd build --profile config\tracking-development.json --stage 5 --integration observe
.\tools\windows.cmd build --profile config\tracking-development.json --stage 5 --integration follow
```

命令会打印实际包目录；`compile_only` 包禁止用于真机烧录。主控配置模板为 16 MB / OPI PSRAM，CAM 为 8 MB / OPI PSRAM，必须核对真实设备。

CAM 依赖准备在 PowerShell 执行：

```powershell
.\.venv\Scripts\python.exe tools\tracking.py prepare --idf
```

然后**打开 CMD**，激活这份 IDF 的 Python 环境再编译：

```bat
cd /d C:\CareRover
call build\toolchains\esp-idf-5.3.4\install.bat esp32s3
call build\toolchains\esp-idf-5.3.4\export.bat
python tools\tracking.py cam-build --variant gesture
python tools\tracking.py cam-build --variant vision
python tools\tracking.py cam-build --variant stream
```

CAM 编译使用激活后的 IDF Python；主控工具、串口捕获和烧录工具使用项目 `.venv\Scripts\python.exe`。不要在 PowerShell 中原样粘贴 CMD 的 `call` 命令。

完成现场核对后，在不存在对应文件时才复制以下模板：

- 主控：`config/tracking-development.json` → `config/board.local.json`，核对 FQBN、容量、PSRAM、core/库版本，实际 Windows 编译与板型确认后才将 verification 改为 `windows_baseline_confirmed`。
- CAM：`config/cam-board.example.json` → `config/cam-board.local.json`，核对容量和 PSRAM，填写主控实际热点 SSID/密码，确认后使用设备 verification。
- 主控 Wi-Fi：`firmware/main_wireless/wifi_secrets.example.h` → 同目录 `wifi_secrets.h`。
- 超声波：`firmware/main_wireless/front_config.example.h` → 同目录 `front_config.local.h`。

现有文件有真实值时保留并核对。`config/calibration/*.json` 是记录模板，不会自动加载到固件。超声波实际编译参数来自 `front_config.local.h`；轮子参数保存在主控 NVS，必须由校准流程写入。

核实设备、供电接线和操作范围后，按 [tracking-development.md](tracking-development.md) 的设备构建、备份、verify、flash 流程推进。串口可用 `.\.venv\Scripts\python.exe -m serial.tools.list_ports -v` 枚举；COM6 仅是历史记录，不是默认值。独立舵机校准工程通过 IDE 上传前，也要先保留当前固件与 NVS 备份。

## 5. 超声波如何结合其他模块

```text
CAM：人脸 / 手势 + MJPEG
          │ UART（只传视觉结果）
          ▼
主 ESP32-S3：网页 / WebSocket + 健康采样 + OLED
          │
网页速度 / 人脸跟随 → 模式与安全仲裁 ← MPU6050、HC-SR04
                              │
                      四全向轮运动学 / 舵机输出
```

| 模块 | 交接约束 |
|---|---|
| HC-SR04 | 接主控，70 ms 周期、10 µs 触发；Echo 降至 3.3 V 电平；GPIO 未定，先查占用和实际引出 |
| 全向轮 | vx 正为前进、vy 正为向右、wz 正为顺时针；FL/FR/RL/RR 为 GPIO10/12/13/11（0915 正交布局）；每轮中值/极性/span 和 NVS verified 先实测 |
| CAM | 与主控 UART GPIO18 RX / GPIO17 TX；`@G`/`@P` CRC 协议；人脸检测跟随，无身份识别和自动搜人；视频不经主控 UART |
| 网络/网页 | 主控热点与网页 `192.168.4.1`，CAM STA/视频 `192.168.4.2`；网页保留 `/ws`、速度命令与急停，新增 front 遥测和 set_demo_bypass |
| MPU6050 | GPIO6/7 软件 I²C；校准、倾斜保护和新鲜度门禁全程保留；绕障日志记录 yaw，不将其当里程计 |
| MAX30102 | GPIO8/9；原有健康算法和 PPG 继续工作；健康模式保持停车 |
| OLED | GPIO4/5；交替显示健康/手势与前方距离、绕障阶段、停车原因 |

前方数据对手动和跟随都生效。手动近障停车后必须释放摇杆；跟随只有显式开启演示时才向右绕行。绕障期间覆盖视觉运动输出，但不绕过目标丢失、保活超时、IMU、急停、断网规则。详见 [协议](protocol.md) 和 [超声波状态机与标定](ultrasonic-development.md)。

## 6. 真机推进顺序与出口

1. **W0 环境与资料**：记录 Git SHA、版本、两板型号/容量、串口、接线照片；先运行软件检查和 Mock；备份当前两板固件与 NVS。
2. **W1 CAM**：手势 → 人脸 → 视频。保留实际帧率、UART 原始日志和画面证据。
3. **W2 主控 observe**：验证视觉来源、IMU、MAX30102、OLED、网页；接入探头后以 `enabled=true, verified=false` 静止测距，校核电平、安装及回波。
4. **W3 底盘校准**：先架空测试每轮中值/极性，再测试受控低速方向；实测停止输出延迟、机械滑行、车身尺寸和演示速度。未校准的超声波会禁止聚合运动，基线运动测量可用已有独立校准工程，在操作者控制的空场完成；不能通过把未知参数标 verified 来解锁。
5. **W4 前方保护参数与验收**：根据上述实测值确定 stop/slow/warn/release。安装和参数有真实依据后才设 `verified=true`，构建设备固件，进行限速、停车、释放、无回波/断开测试。该标记确认参数，不代表测试已 PASS；实测失败应撤销标记并修正参数。
6. **W5 原有人脸跟随**：先关闭演示绕障，验证跟随、丢目标和近障停车，确认现有健康/视频/OLED 并行正常。
7. **W6 演示绕障**：在操作者控制下，先用单独的低速动作测出右移余量、越过时间、停车等待时间与超时上限，再填写参数并设置 `bypassVerified=true`，开放集成验收。测试完整流程，以及每阶段急停、丢目标、失联、再次受阻、超时退出；失败撤销标记。不要用模拟器数值替代测量。
8. **W7 整机演示**：外部供电、不接 USB 连续运行十分钟，保留视频、日志、版本和结果；软件停车时间与实际轮子/车身停止时间分别记录。

右侧和绕行后的通道由场地布置保证；人脸应持续可见。探头刚看不到障碍不代表车身完全让开，必须有额外余量。绕开后的前方还需有量程内、超过 release 距离的有效背景回波；无回波按未知停车，不当作通畅。

## 7. 直接交给 Windows Agent 的提示词

复制下面全文，在 Windows 的 `C:\CareRover` 项目目录中开始任务：

```text
你正在接手 CareRover 双 ESP32-S3 全向机器人，仓库分支为 feat/main-wireless。当前源码已经实现人脸跟随、IMU/健康/OLED/网页集成，以及 HC-SR04 前方保护和固定向右单障碍演示绕行。你的任务是在 Windows 上复现环境并继续真机联调，不要重写现有软件。

先阅读 AGENTS.md、WINDOWS_START_HERE.md、docs/WINDOWS_ULTRASONIC_HANDOFF.md、docs/tracking-development.md、docs/ultrasonic-development.md、docs/protocol.md，以及两份 acceptance 文档。以当前 Git 源码为准；docs/WINDOWS_AI_HANDOFF.md 是较早集成快照，旧提交号和“未上传”记录不能覆盖当前状态。原始本地参考包不是构建依赖，不要因为 Git 没有它而停止，也不要用旧代码覆盖当前 firmware。

先检查 git status/HEAD、Windows 工具版本、已存在的私有配置和实际串口；运行可执行的软件测试与 Mock 演示，记录 PASS/FAIL/NOT RUN，再核对板型、Flash/PSRAM、接线和供电，备份两板固件与 NVS。按 CAM → 主控 observe 与静止超声测距 → 架空/底盘校准 → 手动前方保护 → 原有人脸跟随 → 向右绕障 → 无 USB 十分钟的顺序推进。

已有 Wi-Fi/板型/校准文件应保留并核对。未知的 HC-SR04 引脚、车身尺寸、停车距离、横移余量和越过时间保持未知；先测量再写入 front_config.local.h。校准 JSON 只是记录，不会自动生效。不要把模板 null、模拟值或归一化速度写成实测结果；不要为了通过测试关闭 watchdog、IMU、急停、所有者、新鲜度或 NVS/超声波验证门禁。verified/bypassVerified 只在参数取得真实测量依据后用于开放对应验收，不能代替验收结果。

遵循本文模块分工与引脚占用。超声波接主控，5 V 供电、Echo 做 3.3 V 电平转换。单探头不探测侧后方；演示固定向右，右侧和出口由现场保证，前方无回波必须视为未知。绕障仍保留目标丢失/断链/倾斜/急停停车，不自动重试或恢复旧动作。

能独立完成的软件盘点、依赖准备、编译和问题修复请直接推进；需要人手接线、架空轮子、摆放障碍或确认真实设备时，说明具体缺少什么并继续其他独立工作。不要自动选择历史 COM6，不要在设备/接线未确认时烧录或让轮子运动。阶段结果持续写入 docs/windows-progress.md，原始证据放 output/windows/。修改后运行对应回归测试，回交修改摘要、Git SHA、配置/备份索引、日志、实物录像及未完成项；私有密码和备份不要上传公共仓库。

第一条进展请给出：当前 Git SHA、环境/设备盘点、已完成的软件检查、当前验收阶段、缺失的现场信息和下一步具体动作。
```

## 8. 最后回交什么

- 代码修改及对应 Git SHA，避免仅发送一个来源不明的固件文件。
- 更新后的 `docs/windows-progress.md`：每阶段 PASS/FAIL/NOT RUN、复现步骤、问题和修复。
- 私下转交本机设备配置/安装参数、校准记录、Flash/NVS 备份位置与哈希；区别哪些参数属于哪块板。
- `output/windows/` 中原始串口日志、停车与绕障录像、实际测量数据和固件版本。不能只交网页截图证明车身已经停止。

本交接命令根据仓库现有 CLI、配置和 CI 核对，未在本次 Mac 会话中运行 Windows/实物命令。Git 克隆没有导出清单时不要运行 `verify_handoff.py`；只有实际带清单的离线导出包才使用该完整性检查。
