# Windows 接手入口

**先读 [0915 Windows 续作交接](docs/WINDOWS_0915_HANDOFF.md)。** 包含新电脑获取代码、另传资料、Windows 构建命令、现场验收顺序和第 7 节可直接复制给 Agent 的提示词。

当前分支 `feat/main-wireless`；软件开发基线 `ad633bc7161f5dd2b12723e455aba7d4cf4246ac`，其 [CI 10/10 通过](https://github.com/NeedleAss/spider-webite-dev/actions/runs/34924254063)。取分支最新提交，后续文档提交也包含该基线。本轮新代码尚未实物验收。

## 在新 Windows 电脑获取

PowerShell，`C:\CareRover` 必须是新目录：

```powershell
git clone --branch feat/main-wireless --single-branch https://github.com/NeedleAss/spider-webite-dev.git C:\CareRover
Set-Location C:\CareRover
git status --short --branch
git rev-parse HEAD
```

已有工程先保存未提交修改及私有配置；不要覆盖或强制重置。打开此目录，把交接文档第 7 节提示词交给 Windows Agent。

## 阅读顺序

1. [0915 当前交接与提示词](docs/WINDOWS_0915_HANDOFF.md)
2. [0915 开发内容、验证与剩余问题](docs/0915-demo-development.md)
3. [构建、校准与联调](docs/tracking-development.md)
4. [整机/超声波现场流程](docs/WINDOWS_ULTRASONIC_HANDOFF.md)、[超声波参数](docs/ultrasonic-development.md)、[通信协议](docs/protocol.md)
5. [Windows 进度记录](docs/windows-progress.md)、[原集成验收](docs/tracking-acceptance.md)、[超声波验收](docs/ultrasonic-acceptance.md)

旧交接的版本、引脚、现场状态如与 0915 文档冲突，以当前源码和 0915 说明为准。当前轮位 FL/FR/RL/RR = GPIO10/12/13/11。

## 文件移交

0915 原始归档、当前源码和测试记录已在 Git，无需重复发送。另传已有的私有配置、设备备份、实测校准记录以及未归档现场材料，详见新交接第 3 节。Windows 重新建立环境，不复制 Mac `.venv` 和编译缓存。

原始 0915 目录使用 `tools/verify_demo_handoff.py` 核验。只有收到带 SOURCE_MANIFEST.json / EVIDENCE_MANIFEST.json 的离线导出包时才运行 `tools/verify_handoff.py`；普通 Git checkout 不适用。外层原 ZIP 未收到，不能用目录哈希代替 ZIP 校验。
