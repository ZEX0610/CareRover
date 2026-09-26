# 0915 Windows 续作交接

## 1. 当前成果与接手目标

仓库 `https://github.com/NeedleAss/spider-webite-dev.git`，分支 `feat/main-wireless`。取分支最新版本，不回退到旧交接提交。

- 软件开发基线：`ad633bc7161f5dd2b12723e455aba7d4cf4246ac`。已合并 0915 现场修复、保留超声波保护/绕障，加入 SAFE_BASELINE / DEMO_BALANCED / DIAGNOSTIC_RAW 三档。
- 该基线 [CI 10/10 通过](https://github.com/NeedleAss/spider-webite-dev/actions/runs/34924254063)。Mac 主控/CAM 构建、主机测试和浏览器验证见 [开发记录](0915-demo-development.md)。这些不能替代本轮 Windows 或实物验收。
- Windows 下一步：复现构建，核对并备份设备，做 safe/balanced 实物对比，改善真实 PPG 首次出值延迟，补人物框/手势/停车证据，最后完成整机验收和 Git 回传。
- 默认仍为 `SAFE_BASELINE`；`DEMO_BALANCED` 尚未实物验收。原包最后烧录版本、串口和断电状态都是历史信息，接手时重新核对。

## 2. Windows 获取代码

在 PowerShell 执行，目标必须是新目录；如果 `C:\CareRover` 已存在，另选新的纯英文目录。

```powershell
git clone --branch feat/main-wireless --single-branch https://github.com/NeedleAss/spider-webite-dev.git C:\CareRover
Set-Location C:\CareRover
git status --short --branch
git rev-parse HEAD
git merge-base --is-ancestor ad633bc7161f5dd2b12723e455aba7d4cf4246ac HEAD
if ($LASTEXITCODE -ne 0) { throw '当前代码不包含 0915 开发基线' }
```

后续交接文档提交会让 HEAD 与上述开发基线不同，包含该基线即可。旧电脑有未提交改动时先保存；不要覆盖旧工程或强制重置。若仓库需要认证，使用有权限的 GitHub 账号，不把访问令牌写入仓库。

在 Windows Agent 中打开这个目录，将第 7 节提示词作为新任务输入。工作上下文由本文件、开发记录和源码恢复。

## 3. 哪些资料需要另传

| 资料 | 获取方式 |
|---|---|
| 当前源码、网页、测试、工具、版本锁、开发与回放记录 | Git 已包含 |
| `CareRover_Demo_Signal_Optimization_Handoff_2026-09-15_clean(1)/` | Git 已完整包含；617 个清单文件已核验，无需再单独发送 |
| 对应 `.zip.sha256` | Git 已包含；原始 ZIP 未收到，不能宣称已验证外层 ZIP |
| `firmware/main_wireless/wifi_secrets.h` | 若旧电脑已有，私下复制；缺失则按 example 配置 |
| `config/board.local.json`、`config/cam-board.local.json` | 若已有，私下复制；仅在重新确认同一设备和版本后沿用 |
| `firmware/main_wireless/front_config.local.h` | 仅传真实配置；缺失/未标定不填猜测值 |
| 当前两板 Flash/NVS 备份、哈希、轮子/超声波实测记录 | 从原现场电脑转交已有文件，并在新操作前备份当前设备 |
| 未归档日志、视频、接线照片、实际板型/电源信息、HC-SR04 说明书 | 另传到 `output/windows/reference/` 等忽略目录 |

同一物理主控的 NVS 留在板上；换电脑不需要擦除或覆盖 NVS。旧轮子校准只有匹配当前布局才可使用。不要复制 Mac `.venv`、编译缓存和工具链作为 Windows 环境。早期 0911 参考目录未随 Git 提交，当前构建不依赖它；不要与已入库的 0915 归档混淆。

原始 0915 文件夹保持不变，实际开发入口为根目录 `firmware/`、`js/`、`tools/`。普通 Git checkout 不运行要求 SOURCE_MANIFEST/EVIDENCE_MANIFEST 的 `verify_handoff.py`；此处用下面的 0915 专用验证器。

## 4. Windows 软件复现

准备 Git、Python 3.12、Node.js 22，主控 Arduino CLI 1.5.1 / ESP32 core 3.3.10，CAM ESP-IDF 5.3.4 / ESP-DL 3.3.11。先核对现有安装再补齐，不随意升级锁定依赖。

PowerShell：

```powershell
Set-Location C:\CareRover
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r tools\requirements.txt -r hardware\requirements.txt -r mock\requirements.txt
.\.venv\Scripts\python.exe tools\verify_demo_handoff.py 'CareRover_Demo_Signal_Optimization_Handoff_2026-09-15_clean(1)'
$careRoverTests = Get-ChildItem .\tests -Filter *.test.js | ForEach-Object { $_.FullName }
node --test $careRoverTests
.\.venv\Scripts\python.exe -m unittest discover -s tests -p 'test_*.py' -v
```

如果已有该项目虚拟环境，先核对解释器再决定是否新建。主控和 CAM 构建会使用系统临时目录；Windows 用户名包含中文时，默认 TEMP 可能不符合纯英文要求。构建前在**当前 PowerShell 会话**设置：

```powershell
New-Item -ItemType Directory -Force C:\CareRoverTemp | Out-Null
$env:TEMP = 'C:\CareRoverTemp'
$env:TMP = 'C:\CareRoverTemp'
$env:PYTHONUTF8 = '1'
```

主控只编译（arduino-cli 应在 PATH，或为项目工具设置 ARDUINO_CLI）：

```powershell
arduino-cli version
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.10 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
.\tools\windows.cmd environment
.\tools\windows.cmd build --profile config\tracking-development.json --stage 5 --integration observe --tuning-profile SAFE_BASELINE
.\tools\windows.cmd build --profile config\tracking-development.json --stage 5 --integration follow --tuning-profile SAFE_BASELINE
.\tools\windows.cmd build --profile config\tracking-development.json --stage 5 --integration follow --tuning-profile DEMO_BALANCED
.\.venv\Scripts\python.exe tools\tracking.py prepare --idf
```

CAM 另开 **CMD**，使用 IDF 自己的 Python 环境：

```bat
cd /d C:\CareRover
set TEMP=C:\CareRoverTemp
set TMP=C:\CareRoverTemp
set PYTHONUTF8=1
call build\toolchains\esp-idf-5.3.4\install.bat esp32s3
call build\toolchains\esp-idf-5.3.4\export.bat
python tools\tracking.py cam-build --variant stream --tuning-profile SAFE_BASELINE
python tools\tracking.py cam-build --variant stream --tuning-profile DEMO_BALANCED
```

这些模板生成 `compile_only` 包，不能烧录。设备包必须使用经现场核对的 local profile、实际 Wi-Fi 配置和 `windows_baseline_confirmed`，不能只改标记来绕过核实。

安装了名为 `c++` 的 GCC/Clang 风格 C++17 编译器时，PowerShell 继续：

```powershell
.\.venv\Scripts\python.exe tools\check_firmware.py
.\.venv\Scripts\python.exe tools\replay_demo.py 'CareRover_Demo_Signal_Optimization_Handoff_2026-09-15_clean(1)/evidence' --output output/windows/0915-replay.json
```

MSVC `cl.exe` 不能直接替代此处编译器。缺少兼容编译器则记录 Windows C++ 检查 NOT RUN，引用已有 CI，继续可执行的检查。网页 Mock 命令和操作见 [整机/超声波交接第 3 节](WINDOWS_ULTRASONIC_HANDOFF.md#3-windows-先跑通软件)。

## 5. 设备核对与现场顺序

**当前轮位 FL/FR/RL/RR 为 GPIO 10/12/13/11，正交布局**；以 `firmware/main_wireless/motion_layout.h` 为准。旧文档的 10/11/12/13 表和 X-drive 不能用于当前校准。主控模板 16 MB / OPI，CAM 模板 8 MB / OPI；实物不匹配时停止依赖该假设的设备操作并查明原因。

- 主控 UART RX18/TX17；CAM TX47/RX48，115200，共地。
- MPU SDA6/SCL7，软件 I²C，探测 0x68/0x69，供电主控 3.3 V；MAX30102 8/9，OLED 4/5。
- 先用 `.\.venv\Scripts\python.exe -m serial.tools.list_ports -v` 枚举，核对板型与容量；不默认 COM3/COM6。
- 已授权且设备/供电条件明确后，按 [构建与联调](tracking-development.md) 和 [超声波阶段门槛](WINDOWS_ULTRASONIC_HANDOFF.md#6-真机推进顺序与出口) 执行备份、设备构建、verify、flash。主控 flash 工具会先读回完整 16 MB 备份；独立标定工程上传前也必须保留备份。
- 本次网页也有变动：部署时核对并同步 FFat 网页版本，不能沿用此前“网页未改，只刷 firmware”的结论。使用经过 verify 的配套包和显式分区写入，保留 NVS。

推进顺序：

1. SAFE_BASELINE CAM / 主控 observe，验证启动、视觉、IMU、健康、OLED、网页。observe 不初始化运动输出，也不接受动作手势；动作验收要到 follow。
2. 舵机 5 V 断开、底盘架空，比较两档显示和遥测；每次记录主控、CAM、网页各自版本与配置，不能混用后误判效果。
3. 补采真人手势每类至少十次、空背景五分钟、手指原始 PPG 60–90 秒及多轮首次出值、包含 bbox 的单人/多人序列。真实旧 PPG 首次有效 HR 为 **14.35 秒**，低延迟目标尚未达成；现有摘要日志无法证明人物框真实误差或多人连续性。
4. 核对轮子中值/极性/span 与 NVS 布局；按现场条件逐步架空低速 → manual → LIKE/follow → DISLIKE/TWO/OK → 超声波故障注入 → 落地 → 无 USB 十分钟。
5. 保留输出租约 240 ms、人物/CAM 490 ms、IMU 100 ms、倾倒 40°/200 ms 及所有急停/控制权/标定门。预测框不驱动车轮；普通跟随漏检立即零输出，450 ms 是模式宽限，绕障丢失有效人物立即取消。

历史最终 450 ms 漏检修复尚缺连续真人跟随验收；本次新包不能继承旧 PASS。默认保留 SAFE_BASELINE，是否切换比赛配置由当前实物证据决定。

## 6. 如何回传到这台电脑

每阶段更新 `docs/windows-progress.md`，记录 PASS / FAIL / NOT RUN、Git SHA、两板和网页版本、配置、命令、原始证据路径、现象和剩余问题。代码及可公开的小型摘要提交到同一分支；Wi-Fi 密码、local 配置、设备备份和大录像通过私下文件传输返回。

提交前检查 diff 和暂存清单，避免包含凭据或大备份；推送前 fetch 核对远端，保留两端修改，禁止强推。完成后返回提交 SHA、测试/构建结果和下一步。Mac 接回时先检查工作区，再 fetch 和快进更新；若两端都改过，先整合提交。

## 7. 给 Windows Agent 的完整提示词

```text
请接手 C:\CareRover 的 CareRover 项目，分支 feat/main-wireless。先阅读 AGENTS.md、WINDOWS_START_HERE.md、docs/WINDOWS_0915_HANDOFF.md、docs/0915-demo-development.md，以及原始 0915 归档的 01_CURRENT_LINKAGE_AND_THRESHOLDS.md 和 02_DEMO_OPTIMIZATION_IMPLEMENTATION_PLAN.md，再按需阅读构建、超声波、协议和验收文档。

理解后设置一个 Goal：在 Windows 复现 0915 开发基线，完成可执行的软件验证和后续修复，结合现场条件继续 safe/balanced 对比与整机验收，补齐真实 PPG 延迟、手势、人物框和停车证据，更新交接记录，最后提交并 git push。请直接持续推进已具备条件的软件工作，不停在计划或总结。

取分支最新版本并验证包含 ad633bc7161f5dd2b12723e455aba7d4cf4246ac；根目录才是当前源码，0915 原包保持不变。现有 Mac/CI PASS 不是本轮 Windows 或实物 PASS。先检查环境、Git 状态、私有配置、实际设备与备份，不猜串口、接线、校准值或电源状态。烧录和运动按本次会话实际授权及已核实现场条件推进；需要我接线、供电、架空或提供缺失资料时指出具体事项，同时继续独立软件工作。

重点：轮位 GPIO10/12/13/11，正交布局与 NVS layout 门；默认 SAFE_BASELINE，DEMO_BALANCED 待实测；真实旧 PPG 首次 HR 14.35 秒，仍需改善；旧人物日志缺 bbox；最终 450 ms 漏检版本缺连续真人跟随验收。保护超声波、急停、IMU、控制权、来源新鲜度与输出租约，不通过放宽门槛伪造通过。observe 不能用于动作手势验收，本次部署需要核对配套 FFat 网页版本。

每阶段写入 docs/windows-progress.md，区分 PASS/FAIL/NOT RUN，保留版本、日志和证据。最后检查提交内容，代码和可公开记录推送 feat/main-wireless，私有配置、备份和大视频另行移交，不强推。未完成的实物项明确交接，不把整个目标提前标成完成。
```
