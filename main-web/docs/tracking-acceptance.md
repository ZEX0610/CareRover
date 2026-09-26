# CareRover 本机软件交付与实物验收记录

日期：2026-09-12。开发基线：`feat/main-wireless` / `115898b`，本次改动尚未提交。原交接快照保持不变。

本记录区分主机证据和实物证据。软件编译成功不代表电源、引脚、轮向、采样频率或停车延迟已经实测。完整 Windows 操作见 [开发与分阶段验收指南](tracking-development.md)。发布 ZIP 中 `evidence/` 保存本轮测试和构建记录；`SOURCE_MANIFEST.json`、`EVIDENCE_MANIFEST.json` 和 ZIP 外部 SHA-256 分别核验源码、证据和压缩包。

## 本机结果

| 项目 | 结果 | 证据与范围 |
|---|---|---|
| 网页 Node 测试 | PASS | 14/14；`evidence/node-tests.txt` |
| Python 工具测试 | PASS | 20/20；`evidence/python-tests.txt` |
| 固件纯 C++ 测试 | PASS | 安全状态机、协议/跟随/IMU/缓存、实际舵机驱动三套；`evidence/cpp-tests.txt` |
| Address/UndefinedBehavior Sanitizer | PASS | 协议/跟随/IMU/缓存主机测试；`evidence/sanitizers.txt` |
| 桌面与手机浏览器 | PASS | 1280×1000、390×844；注入 WS 遥测与本机 Mock 视频，验证跟随保活、过期隐框、IMU 故障、动态视频地址与能力控制；`evidence/browser-*.txt`。这是浏览器测试，不是 CAM 实物演示 |
| 主控 observe/manual/follow/legacy | PASS | Arduino-ESP32 3.3.10；stage 5；精确版本、固件大小、SHA-256 见 `evidence/build-summary.json` 与 `evidence/builds/main-*.json` |
| CAM gesture/vision/stream/pico | PASS | ESP-IDF 5.3.4、锁定 ESP-DL 与组件；`evidence/builds/cam-*.json` |
| 独立校准工程 | PASS | 编译记录 `evidence/builds/calibration.txt`；未连接舵机 |
| FFat/分区/构建产物核验 | PASS | 主控构建工具执行容量、内容及 SHA-256 核验；CAM 独立 8 MB 布局 |
| 原交接关键文件 SHA-256 | PASS | 9/9；`evidence/handoff-checksums.json` |
| 原 v4 ZIP 校验 | NOT RUN | 本机无对应 ZIP；外部 TXT 是该 ZIP 指纹，不能代替解压目录校验 |

所有固件构建使用 `compile_only` 配置，不能直接作为设备已验证配置烧录。Windows 应核实实际板型、容量、PSRAM、串口和无线凭据，备份后重新构建设备包。源码压缩包不包含工具链、下载组件或本机生成的固件二进制；构建日志和二进制哈希已随包保存，可按指南重建。

## 已实现的控制约束

- CAM 固定 AP 内 STA `192.168.4.2`，MJPEG `/stream`，单观看者；手势与人脸交替使用相机帧，UART `@G`/`@P` 使用同一 CRC8 编码。
- 主控聚合独立的人脸/手势来源年龄；坏包和重复包不续期。软件 I²C 读取 GPIO6/7 的 MPU6050，健康与 OLED 保留原路径。
- 跟随需连续三帧合格匹配目标，保存面积中位数；横移和转向有滞回，目标缺失立即归零，超时或匹配中断退出。
- 运动仅单任务输出 PWM；NVS 校准未经操作者确认时不安装运动输出。急停、故障、释放和超时绕过缓动。
- 浏览器所有者保活、控制输出和手动命令分别检查 240 ms 有效期；CAM/人物来源内部阈值 490 ms；IMU 100 ms。安全任务目标周期 5 ms。上述配置是实现值，实际停车延迟仍待测量。
- 故障恢复、断线、重启和解除急停不会自动恢复运动。低压故障接口可注入测试，但没有电压采样电路，界面电量保持未知。

## Windows 顺序验收表

必须逐行记录真实结果与日志路径；失败时停在该阶段，不用后续软件测试替代实物证据。

| 顺序 | 现场项目与门槛 | 结果 | 证据 |
|---|---|---|---|
| 1 | 记录 Core/库/FQBN、两板 Flash/PSRAM、实际串口 | NOT RUN | 待填写 |
| 2 | 备份两板完整 Flash 与主控 NVS；验证恢复镜像 | NOT RUN | 待填写 |
| 3 | CAM 手势→人脸→视频，至少两分钟 UART，记录三类 FPS、推理耗时、丢帧、最低内存 | NOT RUN | 待填写 |
| 4 | observe 验证 AP/WS/健康/OLED/人物框/IMU 静置校准和姿态 | NOT RUN | 待填写 |
| 5 | 四轮架空逐轮 neutral/invert/span 校准，保存/确认，断电复测；核实电源峰值 | NOT RUN | 使用 `config/calibration/template.json`，禁止填写猜测值 |
| 6 | 手动六方向各 2 秒；松手、关页、断网、CAM 失联、倾斜、ESTOP | NOT RUN | 命令周期 ≤100 ms；释放/控制断链停车 ≤250 ms；CAM/目标失效停车 ≤500 ms |
| 7 | 手动通过后跟随静止/前后/横移/缓行/离开，验证无自动续跑 | NOT RUN | 待填写 |
| 8 | 完全脱离 USB，同时运行全部功能至少 10 分钟，无崩溃且保存无线演示 | NOT RUN | 待填写 |
| 9 | 真实低压保护 | NOT RUN | 未提供采样电路，不能据注入测试判定通过 |

停车记录应同时保留请求/故障发生时刻、固件停止时刻和可观察的实际轮子停止证据。固件日志的停止时间是控制决策时间，不是实测机械停车时间。

## 尚待实测的限制

MG90S 360°无编码器反馈，速度遥测是控制输出，不是轮速闭环；人脸面积只是距离代理。未加入身份识别、自动搜人或手势驱动车轮。默认 MSR+MNP，Pico 仅为可编译对比方案，未依据虚构 FPS 替换模型。软件 I²C 的实际 100 Hz、视频 5 FPS、线程调度延迟、电源能力和十分钟稳定性均待实物验证。人物框和视频采用同一空间坐标，网页未承诺每个 JPEG 与检测结果逐帧同步。
