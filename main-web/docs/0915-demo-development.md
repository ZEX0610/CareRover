# 0915 交接接收与 Demo 优化开发

本轮从 `feat/main-wireless` 的 `e27c67e` 出发，合并 0915 Windows 现场修复，保留已有 HC-SR04 保护/绕障。仓库根目录是后续开发源；0915 原包是不可变参考。新代码尚未烧入实物。

## 来源与完整性

- 收到 `CareRover_Demo_Signal_Optimization_Handoff_2026-09-15_clean(1)/` 和对应 `.zip.sha256`。内层 `MANIFEST.sha256` 的 **617/617 文件匹配**。
- 外层期望 ZIP SHA-256：`6a7a21ceceac55b3163695b5495cf94dfe50da312da55ae7f08b247a8662d1d2`。未收到原始 ZIP，**未核验外层 ZIP**；解压目录不能替代原 ZIP 的哈希。
- 原包的 `.gitattributes` 禁用换行转换，保留原始 manifest 可验证性。原包包含重复的 `resources/` 镜像和受管依赖，作为收到的完整归档保存，不是根目录的构建依赖。
- 先读原包 [现状与阈值](../CareRover_Demo_Signal_Optimization_Handoff_2026-09-15_clean(1)/01_CURRENT_LINKAGE_AND_THRESHOLDS.md)，再读 [优化方案](../CareRover_Demo_Signal_Optimization_Handoff_2026-09-15_clean(1)/02_DEMO_OPTIMIZATION_IMPLEMENTATION_PLAN.md)。前者记录现场基线，后者的建议不等于已经实物验收。

```sh
python3 tools/verify_demo_handoff.py 'CareRover_Demo_Signal_Optimization_Handoff_2026-09-15_clean(1)'
```

## 合并后的基线

- 实际 FL/FR/RL/RR 为 GPIO **10/12/13/11**，正交布局，混控依次为 `vx+wz, vy+wz, vy-wz, vx-wz`。主控与独立标定工具共用 `motion_layout.h`，NVS 必须匹配布局 ID；旧布局校准不会被错误复用。
- 引入现场手势动作、近景框门、HR 自相关估计及独立保持、MPU 双地址探测、40°/200 ms 持续倾倒判断、人物 450 ms 漏检模式宽限，以及 Windows Unicode 路径构建修复。
- 人物单次不合格帧立即清零目标，普通跟随可短时保留模式；**绕障中的不合格人物帧立即取消绕障**。人物/CAM 490 ms、IMU 100 ms、输出租约 240 ms 保留。
- LIKE 和旋转动作使用同一控制权、标定、网络、IMU 和前方传感器就绪检查。仅 integration=follow 且 stage≥4 接受动作。选择网页手势模式不会自行启动旋转。转角用展开后的净 yaw 变化幅度累计，左右摆动不会累加成一圈，也不假定 MPU 安装后的 yaw 正负方向。
- 本次同时修复了源码分支交叉处的缺口：保留 `set_demo_bypass` 命令入口；标定工具也写布局 ID；待执行 LIKE 在发生停车事件后失效；PPG 样本超时后 OLED/网页同步清空；Mac CAM 构建不再把文件复制到自身。

## 三档参数

| 配置 | 行为 |
|---|---|
| `SAFE_BASELINE`（默认） | 0915 现场检测/滤波参数，叠加当前超声波集成和上述安全修复 |
| `DEMO_BALANCED` | 启用以下离线优化；需实物验证后再用于比赛 |
| `DIAGNOSTIC_RAW` | 使用安全基线参数，发布新增的质量、保持、样本拒绝等只读诊断字段 |

配置名进入构建标识、manifest 和 telemetry。更换配置会生成不同主控包，不会共用同一来源版本号。`SAFE_BASELINE` 保留原检测参数和老回归；由于新增动作仲裁及超声波集成，它不是原设备二进制的逐字节复制。

### DEMO_BALANCED 实现

- **手势**：显示采用 2-of-3 时间窗口，进入/保持分数 0.35/0.18；分类缺失最多保持 1800 ms，无手最多 900 ms。动作单独使用原始 ≥0.45、近景框和非 holding 帧；LIKE 3-of-4，DISLIKE 2-of-3，TWO/OK 4-of-5，窗口≤1500 ms，持续标签只触发一次，三帧释放才重置。
- **PPG**：25 Hz 输入，流式 1.8 s DC EMA、三点中值抗脉冲；HR 150 点/约 6 s，每 13 点更新；SpO₂ 200 点/约 8 s，约 1 s 更新。候选由自相关、频谱 SNR、AC/DC、双通道相关性、接触强度组成软质量分，同时保留采样率/数值范围等硬界。最近三窗中的两个相容候选建立结果，之后按质量平滑与限速；采用轻量平滑，未引入 Kalman。两项保持期限分别为 30 s/12 s，按时间而不是报告窗数计；从未取得的值始终无效。手指进入 35k/3 样本，退出 20k/8 样本。
- **人物**：CAM 降低候选检测门，使用 IoU、归一化中心距离、面积比例的关联代价；最佳/次佳代价接近时拒绝更新。主控和显示使用 alpha-beta 框滤波；预测框最多 700 ms，保留最后实测 seq/时间，网页明确显示 `PREDICTED` 虚线。预测不驱动车轮，运动接受分数仍保持 0.45。中心/距离使用迟滞死区，非零命令有加速度限制，停车直接归零。
- **IMU**：拒绝不可能加速度、突变和陀螺饱和帧，短拒绝保持上次姿态，**不更新时间戳**；连续坏样本达到 100 ms 失效停车。互补滤波按加速度模长动态加权，倾倒阈值仍保持 40°/200 ms。
- **显示/协议**：HR、SpO₂ 各有 valid/held/age，OLED 和网页用 `~` 标识保持；手势 stable 不再被网页置信度二次否决；预测框与原始框区别展示。旧协议字段保留，旧消息也能清除先前的扩展状态，断线后重置。

## 本机验证

| 项目 | 结果/范围 |
|---|---|
| 原包读取前测试 | C++ 四套通过；Node 14/14、Python 28/28 |
| 合并根目录测试 | C++ safety/tracking/drive/front/ultrasonic/tuning/demo 双配置通过；Node 21/21、Python 33/33 |
| 绕障安全 | 保留原有 40 个故障/阶段案例；新增两配置各 35 个故障/阶段案例，覆盖优化后渐进的跟随响应 |
| 主控构建 | SAFE_BASELINE follow、DEMO_BALANCED follow、DIAGNOSTIC_RAW observe 均编译通过；仅 compile_only |
| CAM 构建 | ESP-IDF 5.3.4，DEMO_BALANCED stream 编译通过，约 43% app 分区空闲 |
| 独立标定工程 | 共用新布局源码，编译通过；未上传 |
| 浏览器 | Playwright：1280 px 桌面及 390 px 手机，保持/独立空值/预测框展示及遥测中断清空通过；无横向溢出 |
| 实物烧录/运动 | 本轮 NOT RUN，无串口设备操作 |

曾将原 `front_test.cpp` 强制用 balanced 参数运行，其“单个缩框后立刻前进”断言失败：新滤波/迟滞会延迟该响应。原 baseline 测试和断言保持不变，新增测试使用逐步形成的前进目标，再对两档配置验证每个绕障阶段的同一组停车条件。

```sh
python3 tools/check_firmware.py
npm test
.venv/bin/python -m unittest discover -s tests -p 'test_*.py'
python3 tools/replay_demo.py 'CareRover_Demo_Signal_Optimization_Handoff_2026-09-15_clean(1)/evidence' --output output/0915/replay.json
./tools/mac.sh build --profile config/tracking-development.json --stage 5 --integration follow --tuning-profile SAFE_BASELINE
./tools/mac.sh build --profile config/tracking-development.json --stage 5 --integration follow --tuning-profile DEMO_BALANCED
```

Windows 使用 `tools/windows.cmd` 或项目 Python 执行同一命令。CAM 在激活 IDF 环境后运行 `python tools/tracking.py cam-build --variant stream --tuning-profile DEMO_BALANCED`。本机需把项目 `.venv/bin` 加入 PATH 以找到 CMake/Ninja，但调用 CAM 工具时仍使用 IDF 环境自己的 Python。

## 离线证据与未达成项

完整机器可读结果见 [回放报告](evidence/0915-demo-replay.json)。这些输出来自 C++ 算法回放，不是本轮实物动作。

- 静态背景 60 s：229 个手势帧，动作候选 0。
- LIKE 修复后的 30 s 序列：98 个手势帧，LIKE 候选 1；人物 97 帧/94 found。旧 40 s 保持—释放—重新保持序列产生 3 个 LIKE 候选，不等于实际执行了三次运动。
- 原始 PPG 749 样本：47 个窗口，已建立 HR 为 75–91 BPM，没有 40 BPM 边界假值；**首次有效 HR 14.35 s**。理想合成 75 BPM 波形满足 HR <10 s/SpO₂ <12 s，但这份真实漂移记录尚未达到方案的低延迟目标，不能称为真人 P90 PASS。
- 已有健康候选的 60 s 回放取得 43 个有效 HR 报告；这只验证候选状态机，不能替代原始波形精度验证。
- 三份 JSONL 开头各含一条不完整串口文本，统计中明确记录并跳过，没有改写源文件。
- 人物日志只有 found/confidence 摘要，没有完整 bbox；不能据此宣称完成真实框误差、多人身份连续性或重新居中速度验收。相关算法目前仅有合成几何测试和构建证据。

### 现场下一步

1. 核对两板、实际串口、GPIO10/12/13/11 轮位与当前 NVS 布局，保留 Flash/NVS 备份及现有私有配置。超声波未知实测值继续留空。
2. 先使用 safe 配置和 observe 完成传感器/OLED/网页基线；舵机断电时比较 balanced 的显示、PPG 和框输出。
3. 补采真人手势各十次、空背景五分钟、手指 60–90 s 原始波形、含 bbox 的单人/多人序列；统计首次出值和 P90，继续改善真实漂移场景的 HR 延迟。
4. 保持 40°/200 ms，验证 MPU 拔线/坏帧停车和持续倾斜。45–50°/300–350 ms 必须由实际底盘 A/B 决定，本轮未放宽。
5. 架空低速 → manual → LIKE/follow → DISLIKE/TWO/OK → 超声波绕障故障注入 → 落地 → 无 USB 十分钟。最终 450 ms 漏检版本本就缺连续真人跟随验收，本轮新包更不能继承旧 PASS。

保留现有三帧跟随参考面积初始化、0.45 运动置信度、固定参考面积及立即停车。方案中的降低运动门、慢速自适应参考面积、预测驱动和倾角放宽没有启用；需先取得上述实物/多人证据。`DEMO_BALANCED` 尚不是比赛默认配置。
