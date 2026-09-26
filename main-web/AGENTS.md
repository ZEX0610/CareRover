# CareRover Windows 续作

这是已有集成源码，不是空白项目。先读 `docs/0915-demo-development.md` 了解 0915 合并、配置选择与证据边界；原始 0915 文件夹同样保持不变。开始工作先读 `WINDOWS_START_HERE.md`、`docs/WINDOWS_0915_HANDOFF.md`（当前入口），按需参考 `docs/WINDOWS_ULTRASONIC_HANDOFF.md` 的现场流程，再读 `docs/tracking-development.md`、`docs/ultrasonic-development.md` 和对应验收记录。`docs/WINDOWS_AI_HANDOFF.md` 是早期改动索引，其历史未提交状态不代表当前 Git。本文件适用于此工程；原始参考目录保持不变。

- 当前根目录 `firmware/`、`js/`、`tools/` 是开发代码；`CareRover_Tracking_Motion_Handoff_2026-09-11/` 是原始参考，不能整目录覆盖回来。
- 本机软件构建/主机测试通过，本轮新代码尚无 Windows/实物证据；0915 原包含历史现场记录，不能转算为本轮新代码验收。不要把 Mac PASS、模拟遥测或控制输出当作实物验收。
- Git checkout 先核验分支与提交；仅带清单的导出包才核验清单。记录 Windows 环境与设备、备份，再按 CAM → observe → 架空校准 → manual → follow → 无 USB 十分钟的顺序推进。详细阶段门槛见交接文档。
- 保留协议、坐标、安全约束；不得为通过测试而关闭 watchdog、急停、IMU、来源新鲜度或 NVS 校准门禁。
- 使用实际枚举的串口，不假定 COM6。设备不能唯一识别、接线/供电不明或需要人手操作时，说明具体缺失条件，继续可独立执行的软件工作。
- 本项目不包含身份识别、自动搜人、编码器轮速闭环或真实低压采样。扩大范围应由用户决定。
- 每阶段保留 PASS/FAIL/NOT RUN、构建版本、原始日志、故障与修改；更新 `docs/windows-progress.md`。本包校准模板的 null 不得替换成猜测值。
- 不因看到本文件就自动烧录或让轮子运动；按当前用户授权和现场前置条件操作。本文件不额外要求重复审批已明确授权、条件具备的工作。
