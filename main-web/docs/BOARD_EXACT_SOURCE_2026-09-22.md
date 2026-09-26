# CareRover 两块实机的源码与烧录包基线（2026-09-22）

## 结论

`C:\CareRover` 已按 2026-09-21 从两块实机读取的完整 Flash 备份完成同步。当前标准烧录包中的 8 个实际写入分区文件与实机备份对应区域逐字节一致：主开发板 5/5，CAM 板 3/3。

本次没有打开串口、没有擦除 NVS、没有向设备烧录。这里的“实机一致”指：

1. 主控完整编译输入集合由源码版本 ID 固定，CAM 的 13 个编译输入及两项主控例外文件均有逐文件 SHA-256 门禁；
2. 标准烧录包与实机 Flash 备份的对应区域逐字节一致；
3. 主控功能载荷可从当前源码重新编译复现；
4. CAM 的应用、分区表与 bootloader 可从当前源码和锁定环境逐字节复现。

## 权威证据与位置

### 实机完整 Flash 备份

目录：

`C:\Users\new20\Desktop\硬设资料\ESP32-S3开发\CareRover_Flash_Backup_2026-09-21_pre_update`

| 设备 | 容量 | 完整 Flash SHA-256 |
|---|---:|---|
| 主开发板 | 16 MiB | `398FF2C941F84CE333E4C585E08672A1CC0DC760CA1AC385013341D15DE48F73` |
| CAM | 8 MiB | `6CE2830BDDE46F821E2758B4F8C8D156AE2790CE900FBE525F6DD16B8076B680` |

### 当前标准烧录包

- 主开发板：`C:\CareRover\build\stage5-follow-0569bb8f3f3c66d2-device`
- CAM：`C:\CareRover\build\cam-stream-demo_balanced-windows_baseline_confirmed`

### 原始构建归档

为避免后续构建覆盖现场证据，原构建目录已保留为：

- 主开发板：`C:\CareRover\build\stage5-follow-0569bb8f3f3c66d2-device-board-archive-20260921`
- CAM：`C:\CareRover\build\cam-stream-demo_balanced-windows_baseline_confirmed-board-archive-20260921`

### 同步前本地差异备份

目录：

`C:\Users\new20\Desktop\硬设资料\ESP32-S3开发\CareRover_PreSync_Backup_2026-09-22`

其中保存了同步前、但并非两块板当前已烧录版本的 `front_guard.h` 与 `tracking.py`，用于追溯，不应当作实机源码提交。

## 实机源码对应关系

### 主开发板

主开发板源码入口为：

`C:\CareRover\firmware\main_wireless`

已恢复实机版本的关键文件：

| 文件 | SHA-256 |
|---|---|
| `firmware/main_wireless/front_guard.h` | `742ac674aacfeaf122135f77ff53d3df333ce02548e7d869cb23d64ad5eaf32e` |
| `firmware/main_wireless/build_version.h` | `93178d352ea307f98b3847f46f3d4c1558156be2bbb2dabdcf65fc3d4bfbe95d` |

`build_version.h` 固定了实机上报版本：`0569bb8f3f3c66d2-s5-follow`、stage 5、integration follow、DEMO_BALANCED。

### CAM

CAM 源码入口为：

`C:\CareRover\firmware\cam_tracking`

CAM 当时编译使用的三份共享头文件与当前主控所需版本并不完全相同，因此不能继续让两块板直接共用同一份可变头文件。实机精确版本已独立固定在：

`C:\CareRover\firmware\cam_tracking\board_exact_shared`

| 文件 | SHA-256 |
|---|---|
| `vision_protocol.h` | `fc815f36cce0ef984abdb4809c25488e09a24c04ac305b4e74cff9775276fbb9` |
| `box_track.h` | `fd465bffb1aa5f1e2f594462032fe2df983ce3bbcf31bfb5730bb5bba9c9385e` |
| `demo_tuning.h` | `bb87f76c5eb8acd96ba06b0200afe7e161daf0fa5dfd24f648408f9d9d19fa9f` |

`tools/tracking.py` 在 Windows CAM 构建暂存阶段会把这三份固定头文件放到与原构建完全相同的相对路径，避免主控后续调整影响 CAM 的实机复现。

## 八个实机烧录分区文件

### 主开发板（5/5）

| 文件 | SHA-256 |
|---|---|
| `main_wireless.ino.bootloader.bin` | `b41be55ae9a52aeeb21645c51b86c14027f84c2c91bf67bee6aa0e1b15d18e8b` |
| `main_wireless.ino.partitions.bin` | `ace02503447d0f470692e65fa76002f2d77a92dc81cd3813d8aa66718d716da9` |
| `main_wireless.ino.bin` | `1f8d8d09b17313f448af68368db931f5e793a87f9a4639ccafbbd31255e88aed` |
| `ffat.bin` | `19d5d98517ac7ab7de4c7380a75567a44fa644b00bb71f6bf2d048babb535f54` |
| `boot_app0.bin` | `f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0` |

### CAM（3/3）

| 文件 | SHA-256 |
|---|---|
| `bootloader/bootloader.bin` | `f51065c5e8a35e13890669a8f71e23b837ec4ef6cc6abf5144f3896c4fb89e87` |
| `carerover_cam_tracking.bin` | `1978648c69452163589f01907eb19ccf8dd4d7bdd06deb341f4b41255c33573f` |
| `partition_table/partition-table.bin` | `790756bb2d460ac6007db512e1e8811da2b872d1632d787ab90a6d6bd45c06b1` |

## 无设备核验

在 PowerShell 中执行：

```powershell
Set-Location C:\CareRover
D:\Anaconda\python.exe tools\rebuild_board_exact.py verify
```

预期输出：

```text
PASS: source snapshots and all 8 programmed image segments match the installed boards.
NO DEVICE WAS FLASHED.
```

该命令只读源码与二进制，不连接串口。它会核对主控全部固件/网页运行时输入的聚合源码版本必须为 `0569bb8f3f3c66d2`，并逐项核对 CAM 的全部 13 个构建输入、主控实机 `front_guard.h`、生成的 `build_version.h` 和 8 个烧录分区文件。

## 重建方式

先进入项目锁定的 ESP-IDF 5.3.4 环境，再执行：

```powershell
Set-Location C:\CareRover
D:\Anaconda\python.exe tools\rebuild_board_exact.py build
```

重建脚本使用现场原构建条件：

- 主开发板：Arduino ESP32 Core 3.3.10，临时目录 `C:\CareRoverTemp`，`SOURCE_DATE_EPOCH=1789768863`；
- CAM：ESP-IDF 5.3.4、ESP-DL 3.3.11，临时目录 `C:\CareRoverTemp2`；应用 epoch `1789518943`，bootloader epoch `1789518976`。

脚本本身不调用 flash 命令，不打开串口。

## 主开发板 65 字节构建元数据说明

主开发板用完全相同的源码、工具链、临时路径和时间重编译后，功能载荷、大小与布局一致。新生成 ELF 会导致应用镜像中 65 个元数据字节变化：

- 偏移 176–207：ESP32 应用描述中的 ELF SHA；
- 镜像末尾 33 字节：校验字节及验证 SHA。

除此之外没有任何差异。`rebuild_board_exact.py build` 会先严格确认差异集合只能是这 65 个元数据字节，然后把已从实机核验过的应用镜像放回标准烧录包。因此标准包仍保持与实机逐字节一致，同时避免把真正的代码差异误认为构建元数据。

CAM 在锁定路径和 epoch 后，应用、分区表、bootloader 均可逐字节重建一致。

## 提交与烧录前检查

1. 运行 `tools\rebuild_board_exact.py verify`，必须 PASS。
2. 运行项目 Node、Python、固件主机回归。
3. 审查 Git diff，不能把本地密码、串口日志、整片 Flash 或同步前备份提交。
4. 需要烧录时仍按主板/CAM 各自 manifest 和现场端口执行；本记录不把软件核验等同于再次实物烧录验收。
