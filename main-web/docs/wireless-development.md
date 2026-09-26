> 本文描述原 `integration=legacy` 无线阶段。新人物/运动集成见 [跟随开发说明](tracking-development.md)；软件验证与真机验收分别记录。

# CareRover 无线开发与 Windows 联调

## 当前交付状态

本分支提供独立 Arduino 主控工程、FFat 网页打包、同源 HTTP/WebSocket、真实传感器映射与测试目标安全状态机。**没有实际执行器输出，也尚未取得 Windows 原工程配置或真机验收结果。** 本机 Core 3.3.10 的编译成功不代表板型已核实或固件已烧录。

`DELIVERY` 是本地不可修改的交接快照，不纳入开发提交。主控开发入口是 `firmware/main_wireless/main_wireless.ino`。原 CAM 源码、手势算法、接线、传感器算法、OLED 动画及原有分区保持现有基线；新增服务读取内部状态，不解析自己的串口 JSON。

## 双机同步

Mac 负责开发、测试和热点网页调试；Windows 使用已有成功环境烧录。Mac 无需先获得烧录能力。

```sh
git fetch origin
git switch feat/main-wireless
git pull --ff-only
```

不要在有本地改动时强制重置或整目录覆盖。对照验收记录确认提交。项目无需前端构建；网页源码只维护仓库根目录的 HTML/CSS/JS，`build/*/webroot` 是生成物。

## 先取得 Windows 原配置

在原工程仍能正常工作的 Windows 电脑操作：

1. 打开原主控 Arduino 工程，记录完整板型及工具菜单中的 Flash Size、PSRAM、USB CDC、USB Mode、Partition Scheme、CPU Frequency、Flash Mode/Frequency；打开详细编译输出，保存实际 FQBN。
2. 记录 Arduino ESP32 Core、SparkFun MAX3010x 库版本，并对原主控重新执行“验证/编译”。保存结果；先不要升级核心或改变分区。
3. 如能使用 CLI，运行下面的 `environment` 导出已安装 Core/库列表。它不能替代实际工程菜单配置，也不能自动推断板子 PSRAM。
4. 把 `config/board.example.json` 复制为 `config/board.local.json`，填入已核实的 FQBN 和版本。原主控编译通过、板型参数核实后，才把 verification 改成 `windows_baseline_confirmed`。将不含密码的配置提供给开发端审阅，后续可固定为受版本控制的板型配置。

当前 `config/development.json` 的 PSRAM=disabled **只用于编译检查，不是对物理板型的判断**。工具拒绝烧录此配置生成的包。当前已锁定可下载的传感器库是 SparkFun 1.1.2；若 Windows 使用其他版本，先增加对应的已审核 URL/SHA-256 锁定条目，不静默替换它。

## Windows 准备

```powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r tools\requirements.txt -r hardware\requirements.txt
.\tools\windows.cmd environment
```

工具会查找 PATH 或 Arduino IDE 自带的 `arduino-cli.exe`。如果未找到，设置 `ARDUINO_CLI` 为该可执行文件的完整路径。原 IDE 的 Core 版本须与配置相同；工具发现不同版本会停止，不自动修改成功环境。

首次打包会按 SHA-256 下载固定的 Espressif FATFS 生成器和传感器库到 `build/`，需要联网；部署后的机器人运行不需要互联网。使用 Espressif `wl_fatfsgen` 生成带 wear levelling 的 FATFS，4096 字节扇区、长文件名，大小严格为 `0x9E0000`。生成器来源及哈希在 `config/tools.lock.json`；Python 包版本在 `tools/requirements.txt`。参考 [Espressif FATFS 文档](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32h2/api-reference/storage/fatfs.html)。

将 `firmware/main_wireless/wifi_secrets.example.h` 复制为 `wifi_secrets.h`，把密码设成自己的 8–63 字符 WPA2 密码，保留 C 字符串转义格式。真实密码头文件和含密码的编译产物只保存在本地，不上传公共仓库或公开 Release。

## 分阶段构建与烧录

```powershell
.\tools\windows.cmd build --profile config\board.local.json --stage 1
```

stage 1=AP，2=AP+网页，3=只读 WS，4=测试目标控制，5=完整安全验收版本。安全任务从 stage 1 就存在；stage 4 的测试控制也受全部安全约束，不能通过阶段开关关闭安全保护。

构建结束会打印 `build/stageN-<内容哈希>-device` 路径，里面有：

- `manifest.json`：FQBN、Core、库、CLI、提交及工作区状态、固件和网页版本、烧录地址、各产物 SHA-256。
- `binaries/`：主控分段固件、`boot_app0.bin`、FFat 镜像。
- `webroot/`：本次网页载荷，含版本标识和通信协议。
- `compile.log`：详细编译结果。构建目录包含本地密码，应按私有文件处理。

工具比较**实际生成的二进制分区表**与原基线：NVS `0x9000`、OTA data `0xE000`、app0 `0x10000/0x300000`、app1 `0x310000/0x300000`、FFat `0x610000/0x9E0000`、coredump `0xFF0000/0x10000`。

替换下列包路径及实际 COM 口，关闭其他占用该串口的程序，确认接的是主控而非 CAM：

```powershell
.\tools\windows.cmd verify build\stage1-<内容哈希>-device
.\tools\windows.cmd flash build\stage1-<内容哈希>-device --port COM7
```

`flash` 是唯一写设备的命令。它先验证包和板型配置，再检查连接的芯片及 16 MB Flash，并完整读取现有 Flash 到 `build/backups/`，备份成功才写指定分区。没有整片 erase 操作，也不写 NVS。后续只更新网页可加 `--only ffat`，只更新固件可加 `--only firmware`；固件更新包含 bootloader/分区表/OTA data/app0，确保从 app0 启动。独立更新时需核对网页与固件版本，首次部署使用默认全部分区。

原交接包的主控完整镜像仍可用于恢复：主控 merged.bin 写 `0x0`。优先使用本机新备份恢复最新 NVS/参数。恢复整片镜像会恢复其中的所有分区，仅在确认要回退后执行。CAM 无需重新烧录。

## Mac 与 CI

```sh
.venv/bin/python -m pip install -r tools/requirements.txt
./tools/mac.sh build --profile config/development.json --stage 5
npm test
.venv/bin/python -m unittest discover -s tests -p 'test_*.py' -v
c++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/main_wireless tests/firmware/safety_test.cpp -o /tmp/carerover-safety-test
/tmp/carerover-safety-test
```

主机 C++ 测试直接使用固件的 `SafetyController` 和帧保护逻辑，并非 JS/Python 复刻状态机。CI 运行网页、Python、C++ 测试及 ESP32 stage 3/5 编译；CI 使用明确的编译检查密码，不发布可烧录的设备版本。

## 连接、数据和版本

连接热点 `CareRover-XXXX` 后访问 `http://192.168.4.1/`，自动跳到 `/?transport=ws&video=canvas`。此时合成画面不是 CAM 实景视频，但手势与健康数据来自主控。网页 footer 显示固件和网页版本；`/version.json` 提供页面版本，`STATUS` 额外打印 `wireless_status`。

- 遥测 10 Hz，传感器块只在结果改变/新报告或失效时发送。PPG 25 Hz，每 5 点一批；超过 100 ms 的采样间隙丢弃未满批，避免把旧点拼入新批。
- CAM 按最后一帧有效包独立计时，1000 ms 失效；健康按最新样本 250 ms 和最新报告 2500 ms 失效。主控旧缓存不会通过重复发送“续期”。HR/SpO2 独立失效时传 null。
- 初次连接先发 ping，建立该浏览器会话的估计 Unix 时间；watchdog 和来源新鲜度只用设备单调时间。
- 页面显示“运动输出未接入”；所有非零速度都是测试目标，没有 PWM、驱动器调用或逆运动学。

## 故障、调度与协议边界

ESTOP 高于 FAULT；非控制者只读但可以急停。所有者断连释放控制权，未解除急停仍保持；恢复只回 IDLE。CAM 掉线或任一 AP 客户端 Wi-Fi 断开都会保守地清零并回安全状态；其他浏览器 WebSocket 关闭只有在它是所有者时才清零。AP 客户端断开采用保守处理，因为没有将站点 MAC 与所有 WebSocket 会话建立身份映射。

MANUAL 非零命令过期阈值是 **240 ms**，独立任务目标周期 **5 ms**，给 250 ms 上限留调度余量。`device.max_safety_gap_ms`、`last_cmd_ms`、`stopped_at_ms`、`stop_reason` 可用于现场验证；测得间隔超过 10 ms 或实际停止超过 250 ms 视为验收失败。HTTP 任务优先级低于安全任务，I/O 与 JSON 不在安全临界区中。

每次最多四个 WS 客户端，只保留一个待执行发布任务；PPG 保留最新完整批，发送积压丢旧数据。HTTP 单次 socket 收发超时 1 秒，慢端可能降低整个网络服务的刷新速度，但不阻塞传感器/安全任务；对发送失败的客户端关闭连接并清理所有权。持续拥塞时 watchdog 清零，必须重新请求 MANUAL。

入站文本消息上限 64 KiB；拒绝二进制和分片消息、深度超过 8 的 JSON、重复字段、非法数值、超长类型和过多顶层字段。运动、模式及恢复请求还会拒绝较首次 ping 建立的时钟基准落后超过 200 ms 或超前超过 100 ms 的命令，避免执行网络积压中的旧指令。零速度和急停不受该时间窗口限制；浏览器时钟显著改变时重新连接重新建立会话基准。

不匹配 HTTP Host 的浏览器 Origin 被拒绝；无 Origin 的命令行联调允许连接。FFat 挂载失败进入 FAULT 并打印错误，绝不自动格式化；修复需重新上传正确载荷并重启。没有 STA、mDNS、OTA 或自动门户。

## 验收与问题回传

先按照 `docs/wireless-acceptance.md` 完成阶段门槛。自动 probe 默认只读：

```powershell
.\.venv\Scripts\python.exe hardware\wireless_probe.py --seconds 600 --report build\readonly-10min.json
```

stage 4/5、CAM 在线且确认是测试后端时，可执行 20 组目标值异常测试：

```powershell
.\.venv\Scripts\python.exe hardware\wireless_probe.py --seconds 30 --exercise-targets --report build\targets-20.json
```

probe 明确拒绝实际运动后端，结束时释放目标。报告不保存 HR、SpO2 或 PPG 原始值。它不能代替断开 USB、真实 Wi-Fi 干扰、CAM 拔线等人工场景；也不能证明未来电机物理停车能力。

回传：提交号、网页/固件版本、板型配置、编译摘要、probe 报告、现场现象及必要串口诊断。不要回传 Wi-Fi 密码或把新的健康原始记录提交到 GitHub。各阶段在真机通过之前保持“待验收”。
