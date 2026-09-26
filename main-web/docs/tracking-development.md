# CareRover 人脸检测跟随集成：开发与 Windows 联调

本实现以 `115898b` 的无线主控为基线，按原交接包 docs/04 的 0→1→2→3→4→5 顺序集成。原始交接包保持不变。当前软件具备编译与主机测试证据，全部实物校准、串口、运动、视频性能及整机验收仍为 **NOT RUN**。

## 已实现的范围

主控 AP `192.168.4.1` 托管 FFat 网页、`/ws`、10 Hz 遥测；DHCP 从 `.3` 开始。CAM STA 固定 `.2`，端口 80 `/stream` 为 MJPEG。视频首版单观看者，其他客户端返回 503；遥测最多四个客户端。固件不把 JPEG 或原始 PPG 塞入 telemetry，原有 PPG batch 保持独立。

人脸检测使用 MSR+MNP，手势与人脸交替处理同一相机帧来源。人物为人脸框，不识别身份，不保证多人交叉遮挡后的身份连续性。首次选择最大合格脸，后续按 IoU 匹配；匹配歧义时输出无目标并停止跟随。可单独构建 Pico 224 对比，不能根据名称假定更快。

主控 UART 为 RX18/TX17，CAM 为 TX47/RX48，115200 8N1，GND 共地。GPIO6/7 的 MPU6050 使用独立软件 I²C，保留 MAX30102 GPIO8/9、OLED GPIO4/5 两路硬件 I²C。IMU 默认 0x68；AD0 高时修改 Mpu6050Soft 初始化地址为 0x69。内部目标 100 Hz；开机静置五秒校准，相对 yaw 不用于绝对导航。

四轮 FL/FR/RL/RR 为 GPIO10/12/13/11（0915 正交布局，见 `motion_layout.h`）；NVS 校准须匹配布局 ID。当前接手顺序见 [0915 Windows 交接](WINDOWS_0915_HANDOFF.md)。LEDC 50 Hz、14 bit；360°舵机没有轮速反馈，网页速度为控制量。四舵机使用独立稳定 5 V 电源并共地；交接图片不能证明所示电源可提供所需峰值电流，现场测量后再通电测试。

## 主控构建

依赖沿用项目 `.venv`、Arduino CLI、Arduino-ESP32 3.3.10 和 SparkFun MAX3010x 1.1.2。开发配置 `config/tracking-development.json` 含 16 MB Flash、OPI PSRAM，仅允许编译。

```sh
./tools/mac.sh build --profile config/tracking-development.json --stage 5 --integration observe
./tools/mac.sh build --profile config/tracking-development.json --stage 5 --integration manual
./tools/mac.sh build --profile config/tracking-development.json --stage 5 --integration follow
```

保留原 stage 1–5 语义；新的 integration 与其正交：

| integration | 运行内容 |
|---|---|
| legacy（默认） | 原无线测试目标，无 PWM；不开放跟随 |
| observe | 人物/IMU/健康/视频地址聚合，只读，不初始化 PWM |
| manual | 增加真实 PWM，仅手动 |
| follow | 增加人物跟随，仍经同一安全控制器 |

所有配置均启动安全任务；stage <4 继续拒绝非零控制。设备 profile 必须由 Windows 成功配置核实并设 `windows_baseline_confirmed`，私有 Wi-Fi 密码仍放在 `firmware/main_wireless/wifi_secrets.h`。

Windows 命令：

```powershell
.\tools\windows.cmd environment
.\tools\windows.cmd build --profile config\board.local.json --stage 5 --integration observe
.\tools\windows.cmd verify build\<上一步返回的包>
.\tools\windows.cmd flash build\<上一步返回的包> --port <实际主控COM口>
```

`flash` 验证 16 MB Flash 并完整备份后写指定分区，保留 NVS。不得把 CAM 的 8 MB 包交给主控，也不得使用历史 COM6 推断当前连接。

## CAM 依赖和构建

`config/cam-toolchain.lock.json` 固定 ESP-IDF 5.3.4 和 ESP-DL 3.3.11 的 Git commit，后者提供手势 0.2.0、人脸检测 0.5.0。`config/cam-dependencies.lock` 固定注册表组件版本与哈希，`${PROJECT_ROOT}` 在构建时替换为本机路径。

```sh
python tools/tracking.py prepare --idf
```

第一次在 Mac 安装和激活工具链：

```sh
bash build/toolchains/esp-idf-5.3.4/install.sh esp32s3
.venv/bin/python -m pip install cmake==3.30.5 ninja==1.11.1.3
export PATH="$PWD/.venv/bin:$PATH"
source build/toolchains/esp-idf-5.3.4/export.sh
python tools/tracking.py cam-build --variant gesture
python tools/tracking.py cam-build --variant vision
python tools/tracking.py cam-build --variant stream
python tools/tracking.py cam-build --variant pico
```

Windows 使用 Espressif 5.3.4 命令环境，或在 CMD 依次执行下载目录内 `install.bat esp32s3`、`export.bat`；在激活后的终端运行同样的 `python tools/tracking.py cam-build`。CAM 构建需要该环境的 Python。

| variant | 手势 | 人脸 | 视频 |
|---|---|---|---|
| gesture | 开 | 关 | 关 |
| vision | 开 | MSR+MNP | 关 |
| stream | 开 | MSR+MNP | 开，目标 5 FPS |
| pico | 开 | Pico 224 | 开，性能对比 |

将 `config/cam-board.example.json` 复制为 `config/cam-board.local.json`。现场核实 8 MB Flash / OPI PSRAM，填入主控串口显示的实际 SSID 和相同 WPA2 密码，将 verification 设为 `windows_baseline_confirmed`。默认示例密码只是编译占位。

```powershell
python tools/tracking.py cam-build --variant stream --profile config/cam-board.local.json
.\.venv\Scripts\python.exe tools/tracking.py cam-flash build/cam-stream-windows_baseline_confirmed --port <实际CAM口>
```

烧录工具使用项目 `.venv` 的 esptool 5.1，检查 8 MB、校验所有产物、完整备份成功后才写 CAM 的 bootloader、分区表和 app，不自动擦除 NVS。示例 compile_only 包禁止烧录。不要直接执行 IDF 自动打印的烧录命令绕过备份工具。

## 校准与首次运动

当前主控历史固件为独立舵机测试版。完整恢复手势/健康/OLED 功能需烧录新的聚合固件；先保留当前固件和 NVS 备份。

```powershell
.\.venv\Scripts\python.exe tools/tracking.py calibration-build --profile config/board.local.json
```

校准工程源文件放在 `firmware/motion_calibration`；构建工具自动放入唯一维护的主控 PWM/运动学文件，生成完整 sketch 到 `build/calibration/motion_calibration`。现场备份后可用 Arduino IDE 打开生成的 sketch，按已核实板型上传。

轮子架空，校准命令保持 standalone 原契约：`arm/disarm`、`wheel fl 10 200`、`neutral fl 1500`、`invert fl 0`、`span 300`、`save`。新增 `wheelspan fl 300` 分轮调整；FL/FR/RL/RR 依次完成中值、低速正反向和混合动作测试。校准命令只用于该工程，不接入网页运行路径。

`save` 将当前参数写入 NVS 并清除验证标记。全部实测通过、`disarm` 后执行 `confirm_calibration` 才写入 verified 标记；此命令是操作者对实际测试的确认，不是软件自动测量。新主控兼容旧 `cr-motion` 的 n0..n3/d0..d3/global span，并新增 s0..s3。没有 verified 或参数越界时主控拒绝运动。

将实测结果填入 `config/calibration/template.json` 的副本；默认 null 表示未测，禁止当作 1500 µs 校准值。IMU 每次启动重新校准，零偏由串口 `imu_status` 记录并汇入现场校准文件。

## 控制和停止

- 跟随进入后采集三个相互匹配的置信度 ≥0.60 新框，以面积中位数 A0 记住距离。EMA=0.35，水平死区 ±0.10。
- 横移最大 ±0.20；水平误差达到 0.35 时暂停平移、转向，回到 0.20 以内再横移；旋转最大 ±0.30。前后基于 `1-sqrt(A/A0)`，死区 ±0.10、增益 0.5、最大 ±0.25。
- 丢失/低置信度/匹配失败停止并回 IDLE。重新进入跟随重新记录距离；不自动搜索。
- 控制者在跟随模式每 100 ms ping。主控分别检查控制者保活、人物来源和跟随计算输出；只有新鲜递增 ping 可续保活，旁观者无效，ping 不续手动速度。
- 手动/保活/跟随输出 240 ms 过期；安全任务 5 ms，为 250 ms 停止预留调度余量。CAM/人物来源内部 490 ms 失效，为 500 ms 上限预留余量。
- MPU6050 无效/未校准/100 ms 未更新禁止运动；倾角达到 25°故障。恢复到 20°以内稳定一秒后允许重新选择模式，绝不自动恢复运动。
- 松手、关页、隐藏、模式切换、急停、故障直接输出校准中值，不经过加速度缓动。ESTOP 锁定，只能显式解除，解除后回 IDLE。
- 电池采样硬件未提供，电量未知；故障入口可用，但实物低压检测仍未实现/验收。

## 日志与验收

项目虚拟环境安装 `tools/requirements.txt`、`hardware/requirements.txt`。采集工具只读，不发运动命令：

```powershell
.\.venv\Scripts\python.exe tools/tracking.py capture --port <CAM口> --seconds 120 --output output/cam-120s.jsonl
.\.venv\Scripts\python.exe tools/tracking.py capture --port <主控口> --seconds 600 --output output/main-600s.jsonl
.\.venv\Scripts\python.exe tools/tracking.py analyze output/cam-120s.jsonl
```

每行包含采集端 elapsed_ms 与原文；生成 summary 统计 CRC、人物帧间隔和资源日志。日志统计不自动宣布物理停车通过。真实停止时刻需结合 PWM/串口记录和录像，不能只看网页归零。

现场顺序：CAM 手势→人脸两分钟→MJPEG；主控 observe→传感器与 OLED 回归→校准→manual 六方向与故障矩阵→follow 静止/缓慢移动/丢目标→无 USB 十分钟全功能。

通过标准：命令周期 ≤100 ms；控制断链/松手 ≤250 ms 停止；CAM/目标来源失效 ≤500 ms；恢复不自动运动；视频与框可见；十分钟不崩溃并记录 heap/PSRAM 最低值。所有失败保留原始 FAIL 与对应版本，不用新成功记录覆盖。

## 自动检查与回交

```sh
npm test
.venv/bin/python -m unittest discover -s tests -p 'test_*.py' -v
.venv/bin/python tools/check_firmware.py
.venv/bin/python tools/tracking.py release
```

`output/CareRover_Tracking_Software.zip` 包含源码、测试、协议、校准模板与工具，内有逐文件 SOURCE_MANIFEST.json，以及 output/evidence 中本轮证据的 evidence/ 副本和 EVIDENCE_MANIFEST.json，旁边有 ZIP SHA-256。具体结果见 [验收记录](tracking-acceptance.md)。不包含私有密码、构建缓存或声称真机已完成的证据。依赖首次下载需要网络，机器人运行不依赖互联网。
