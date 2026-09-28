# 手表到小车主板的只读联调记录（2026-09-27）

## 本轮范围

- 手表 ESP32-S3 Zero：MPU6050 使用独立 I²C 的 GPIO4/5；MAX30102 的 D/C 已焊至 GPIO6/7，但本轮外部 5 V 关闭，`WATCH_ENABLE_PPG=0`，GPIO6/7 保持输入态。手表只由电脑 USB 供电。
- 手表作为小车 AP 的 STA，向 `192.168.4.1:45670/udp` 以 20 Hz 发送 `watch_v1`；小车主板另开低优先级 UDP 接收任务。这条链路不占 CAM 的 UART、视频 HTTP、音频 WebSocket 或手动控制接口。
- 主板只将手表状态加入遥测顶层 `watch` 对象并每秒在串口输出 `watch_status`。**本轮不新增 WATCH 驾驶模式，不将手表 `vx/vy/wz` 写入电机或现有控制仲裁。** 当前心率、血氧均为无效，不可作为测量结果展示。

## 可供前端使用的遥测契约

`watch` 包含 `online`、`calibrated`、`rolling`、`seq`、`age_ms`、`vx`、`vy`、`wz`、`contact`、`hr_valid`、`spo2_valid`、`hr_held`、`spo2_held`、`hr_bpm`、`spo2_pct`、`sqi`。离线超过 350 ms 时，运动值置零，健康有效位为假，`hr_bpm/spo2_pct` 为 `null`。`vx` 正为前、`vy` 正为右、`wz` 正为顺时针，这只是协议约定，具体佩戴方向仍需现场逐向校正。

接收端只接受小车 AP 子网中的严格格式包；检查有限数值、范围、同一启动周期内递增序号，重复/旧包不能延长在线时间。这个处理不等于完成网络身份鉴权，因此不能直接作为运动指令授权。

## 已完成的本机与实物核对

- 主板 COM3 为 16 MB ESP32-S3，MAC `68:ee:8f:60:68:24`；手表 COM7 为 8 MB ESP32-S3-PICO-1，MAC `ac:27:6e:d2:c7:ec`。
- 更新前主板 16 MB 全片备份：`build/backups/main-before-watch-rx-20260927.bin`，SHA-256 `18D444010F630965FA613C15F119B60CA2BBDD76DC908DE200527248379B92A2`。其中旧 app0 前 1,129,504 字节 SHA-256 为 `1A88EDE9CB47C9EFF111902768EF35CC61001646579E9C68316D40D736BDA7A0`，与 `build/ap_only_audio_20260927/main_wireless.ino.bin` 一致；此文件可单独恢复原应用。
- 主板新增接收版编译通过（程序占用 1,141,234 字节）；仅写 COM3 app0 `0x10000` 的 1,141,376 字节，SHA-256 `D243233E177BD13E99F0A366A65EF0BD4A714BA3F05BE06BA678BA7AF1483EAD`，esptool 写后哈希校验通过。CAM、NVS、FFat 未写入。启动版本为 `4e480f5e21e63c05-s5-follow-apdiag-audio2-watchrx1`，模式 IDLE、无急停/故障。
- 手表程序以 ESP32-S3 Zero 的 N8R8 配置编译通过，最终 app0 镜像 919,504 字节，SHA-256 `E9476628F154BFFD5114C73910A6DAB619BDA0D132EE53AD71199481857DC1D3`；仅写 COM7 app0 并校验通过。曾发现每帧向 USB CDC 打印会在无人打开串口时阻塞发送；移除运行时逐包打印后，主板连续 18 秒每秒收到约 20 个新序号，包龄约 0–40 ms，全部报告 `online=true`。
- 用户以手背佩戴姿态做动作时，主板连续约 38 秒收到序号 1928→2670，全部在线、已校准；`vx/vy/wz` 均出现非零值。当前记录不能精确对齐每段动作的开始时间，左右/上下符号和回正漂移尚未逐向验收。
- 用户确认 iPad 本地视频正常。继续观察时，主板 `vision_link.valid` 在约 7 秒内由 2224 增至 2268，CAM UART 在手表 20 Hz 并发期间仍有有效帧；`bad` 由 1 增至 2、`resync` 由 3 增至 4，需继续关注长期误包率。开机初期读到的 `valid=0` 不能作为 CAM 链路故障结论。
- 用户让人入镜并展示一根手指后，确认网页人物框和手势均持续更新。动作结束后手表静止时曾持续输出 `vy=-0.45`；在同一姿态按 B 归中后，连续 15 秒 `vy/wz=0`、`vx≈-0.013`，无线始终在线且序号每秒增加约 20。说明此轮满幅横移是累积的相对航向偏移；静止段未见继续发散，但仅靠六轴无法永久确定绝对航向。驾驶模式需要明确归中交互/漂移处置并再次逐向验收。
- 接收状态机的本机 C++14 单元测试通过；测试了失联、重复/倒序序号、不同启动周期、非数值及超范围值。`git diff --check` 通过。
- 最终 8 秒只读复查：手表 `online=true`、`cal=1`、包龄 39 ms、归中后 `vx=-0.015, vy=0, wz=0`；主板 `mode=IDLE`、无急停/故障，CAM 有效帧 4838→4894，bad 保持 2、resync 保持 4。Python 39/39、网页 Node 24/24 回归通过。

## 尚未通过的项目

- 视频、人物框、手势与 CAM UART 已短时并发复验；音频及长时间并发性能尚未完成本轮复验。
- 没有进行手表驱动电机、心率/血氧实测、外部电池供电、长时间无线压力测试，也没有修改小车 OLED/网页对手表字段的显示。
- MAX30102 板外侧 I²C 电平尚未实测，GPIO6/7 已焊死；Zero 的 USB 5 V 与外部 5 V 并接风险也未排除。用户暂时不能返工且无万用表。**不要开启手表外部 5 V，也不要拔掉手表 USB 改外部电源。** 完成电平转换、电源隔离与测量后再启用 PPG。

## 源码与接续

- 主板：`firmware/main_wireless/watch_link_state.h`、`watch_link.h/.cpp`、`wireless_runtime.cpp`、`build_version.h`；测试 `tests/firmware/watch_link_test.cpp`。
- 手表：`C:\Users\new20\Desktop\硬设资料\ESP32-S3开发\CareRover_Watch_Prototype_2026-09-24\firmware\CareRoverWatch`。本地 `watch_secrets.h` 被忽略，不应提交 Wi-Fi 密码。
- 下一步先确认 CAM 是否供电，再同步录制逐向动作，检查正负号、回正漂移及对原网页/音视频性能的影响。硬件电气问题解决后，才可继续 PPG、独立供电和经单独审阅的手表驾驶模式。
