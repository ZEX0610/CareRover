# CareRover JSON 协议 v1

最终整机中，浏览器只连接主 ESP32-S3 的 `/ws`。所有消息是 JSON 文本，含 `type` 和 `ts`（Unix 毫秒）。视频不进入 WebSocket。USB 串口桥仅用于开发诊断。

## Browser → Robot

| type | 字段 | 语义 |
| --- | --- | --- |
| `cmd_vel` | `vx`, `vy`, `wz` | 有限数，各在 [-1,1]，只允许 MANUAL 非零运动 |
| `set_mode` | `mode`, `request_id` | 请求模式，不表示已经生效 |
| `estop` | 无额外必需字段 | 立即停车并锁定 |
| `clear_estop` | `request_id` | 显式恢复；成功后回到 IDLE |
| `ping` | `id` | 由 pong 回传同一个 id |

`request_id` 是可选的 v1 扩展。本网页为模式和恢复请求生成递增整数；服务端原样回传。旧硬件不含 request_id 的 ACK 仍被支持，但模式最终以 telemetry 为准。

```json
{"type":"cmd_vel","ts":1788940000000,"vx":0.6,"vy":0.3,"wz":0}
```

坐标约定：vx 正为前进，负为后退；vy 正为右移，负为左移；wz 正为顺时针 / 右转，负为逆时针 / 左转。屏幕 y 向下，因此摇杆上拖得到正 vx。二维向量长度不超过 1，再乘用户速度上限。前端不发送单个舵机 PWM，也不做四轮逆运动学。

输入按住时以最多约 20 Hz 发送目标。松手、pointercancel、lostpointercapture、blur、hidden、断线、模式切换、过期遥测时本地输入归零，连接可用时立即发送零速度，并再重复 3 次。零速度和急停不受普通发送节流影响。高频 cmd_vel 不逐包 ACK，通过 telemetry 回显。

## Robot → Browser

聚合 telemetry 推荐 10 Hz。源图像分辨率与画布尺寸是两个坐标系统：

```json
{
  "type":"telemetry","ts":1788940000000,
  "connection":{"camera":true,"main_mcu":true},
  "robot":{"mode":"MANUAL","state":"READY","estop":false,"battery_pct":87,"vx":0,"vy":0,"wz":0},
  "imu":{"yaw_deg":1.2,"pitch_deg":0.1,"roll_deg":-0.2},
  "vision":{
    "image_width":320,"image_height":240,"ai_fps":5.8,
    "person":{"found":true,"x":124,"y":32,"w":76,"h":176,"confidence":0.94},
    "gesture":{"label":"PALM","confidence":0.91,"stable":true}
  },
  "health":{"hr_bpm":74,"spo2_pct":98,"sqi":0.94,"finger_detected":true,"state":"VALID"}
}
```

部分块可省略，省略字段不覆盖上次值；数值 0 是有效上报。**MANUAL 许可要求 mode、estop 的完整机器人回报在 1 秒内收到**，且两个设备链路均在线。仅收到健康数据不会延长运动许可。

Mock 额外上报 `connection.simulated: true`，用于将 WebSocket 模拟设备清楚标注为模拟来源；真实设备可以省略此字段。

- 请求模式：`IDLE`, `MANUAL`, `PERSON_FOLLOW`, `GESTURE_CONTROL`, `HEALTH_CHECK`。
- 系统模式：`ESTOP`, `FAULT`，不可通过 set_mode 请求。
- 状态：`IDLE`, `READY`, `DRIVING`, `TRACKING`, `SEARCHING`, `MEASURING`, `ESTOP`, `FAULT`。
- 手势：`NONE`, `PALM`, `FIST`, `THUMB_UP`, `VICTORY`, `POINT_LEFT`, `POINT_RIGHT`, `ONE`, `TWO`, `THREE`, `FOUR`, `FIVE`, `OK`, `CALL`, `LIKE`, `DISLIKE`, `UNKNOWN`。显示置信度和更新时间；stable 且 confidence ≥ 0.75 显示“已稳定”，网页不会据此自行发运动命令。
- `call:{"requested":boolean,"sequence":number}`：CAM 的稳定 `CALL`（拇指与小指伸出）每次重新展示会切换主板通话请求状态并递增序号。远程 HTTPS 控制台仅按新序号显示来电或挂断；来电仍需家长点击接听以授权浏览器麦克风。保持手势、遥测重发都不会重复拨号。此字段不是电机指令，也不表示家长已接听。

手势去抖语义：一次有效 `CALL` 或 `FIVE` 必须先连续确认，再在画面中实际消失至少 3 帧，下一次重新展示才会再次触发。因此 `CALL → 消失 → CALL` 是取消来电请求；`FIVE → 消失 → FIVE` 是退出手表模式。仅置信度暂降、但标签仍为同一手势，不算消失。`FIVE` 只切换手表模式，不直接驱动车轮；运动仍需手表在线、归中和既有安全仲裁。

## 家长照护事件（2026-09-29）

局域网页从主板遥测独立计算顶部消息：CAM 在线且连续 30 秒明确未检测到人脸时显示“30 秒未识别到人脸”，仅新鲜、非预测的人脸识别结果可解除；CAM 离线不等于无人脸。前方超声有效、主板 IDLE 且状态稳定约 800 ms 时，另行提示“已离座/已入座”。通话期间超声暂停时不推断离座。局域网页没有操作系统后台通知或 APNs。

服务器把相同状态转换成 `care_state` 快照和 `care_event` 实时事件。家长网页在现有 `/ws` 接收；原生 iPhone App 可独立订阅只读 `/events`，不占用遥控席位。事件 `kind` 包括 `face_absent`、`face_restored`、`seat_left`、`seat_returned`、`call_invite`、`call_cancelled`、`call_connected`、`call_ended`。`call_invite` 包含 `sound:"ring"`、`requires_confirmation:true` 和递增的 `sequence`；家长明确接听后才建立 `/audio`。认证、示例 JSON、快照恢复、前后台限制详见 [iPhone 接口说明](../../remote-hub/docs/IOS_CLIENT_API.md)。
- `cliff.installed_mask/edge_mask`：四角位依次为左前 `1`、右前 `2`、右后 `4`、左后 `8`；OUT 高表示该角越界。当前安装掩码为 `15`，仅在手动和手表控制中拦截对应平移方向与两种旋转；跟随/自动手势运动仍不得在桌面边缘无保护运行。
- 健康状态：`NO_FINGER`, `ACQUIRING`, `MEASURING`, `VALID`, `LOW_QUALITY`, `ERROR`。无手指、无效、低质量或过期时不显示为有效 HR / SpO₂。
- person 500 ms 未更新隐藏；gesture 2 秒未更新失效。两个时间戳各自维护。

## PPG

当前真机 25 samples/s，推荐每 200 ms 一批 5 个样本：

```json
{"type":"ppg_batch","ts":1788940000000,"sample_rate_hz":25,"samples":[18342,18480,18900,20110,19420]}
```

ts 表示本批**最后一个样本**时间；其余按采样率反推。前端绘图将批末锚定本地接收时间，避免未同步的设备时钟让波形跑出视野；原始 ts 保留在录制文件中。断开的时间段不补线。

兼容单样本 `{"type":"ppg","ts":1788940000000,"value":18342}`。最大单批 512 样本；无效样本丢弃；采样率接受 (0,2000] Hz。波形只展示最近 8 秒，固定分配 4096 样本缓冲。

## ACK / ERROR / PONG

```json
{"type":"ack","ts":1788940000010,"request_type":"set_mode","request_id":1,"ok":true}
```

set_mode 等待一致 telemetry 才选中模式；1.5 秒未确认提示失败。clear_estop 必须先有成功 ACK，再有 `estop:false, mode:IDLE` 的 telemetry 才解除本地锁。迟到的普通 telemetry 不得解除本地急停。

```json
{"type":"error","ts":1788940000010,"code":"ESTOP_ACTIVE","message":"Motion rejected while emergency stop is active"}
{"type":"pong","ts":1788940000010,"id":1788940000000}
```

错误包括 `ESTOP_ACTIVE`, `NOT_IN_MANUAL`, `INVALID_COMMAND`, `INVALID_MODE`, `UNKNOWN_TYPE`, `INVALID_JSON`, `CONTROL_BUSY`。

## 安全与连接契约

1. 主控最高优先级为 ESTOP，其次 FAULT，再进行模式仲裁。
2. 主控 **>250 ms 未收到有效 MANUAL cmd_vel 必须独立归零**。浏览器断电、系统冻结、网络丢包时仍然成立。
3. 断开控制所有者时停止并回 IDLE。急停锁跨连接持续存在。重连不恢复旧输入。
4. Mock 服务由第一个成功 set_mode / 非零运动 / clear_estop 客户端持有控制权；其他客户端只读，可随时急停。所有者断开释放控制权。固件必须实现等价仲裁。
5. WebSocket 重连退避 0.5 / 1 / 2 / 4 / 5 秒，上限 5 秒；握手超过 5 秒关闭后重试。ping 每 2 秒；超过 4 秒未回 pong 后重连。
6. 本地急停在连接断开期间也锁定 UI，并在重连时补发 estop。离线时不能宣称机器人已经收到急停，机器人端 watchdog 是必要兜底。
7. 入站单帧上限 64 KiB；非法 JSON 不进入状态。WS 发送积压超过 64 KiB 关闭链路，防止排队的旧运动命令。
8. 回放完全隔离命令发送，恢复实时仍等待新的机器人状态。

本协议不定义 ESP-DL 推理、UART 帧、轮子运动学或 PWM 校准。


## 主控无线测试后端扩展

完整部署说明见 `wireless-development.md`。新增可选字段均兼容原网页协议：

- `robot.motion_output_installed:false`：速度回显仅为测试目标，无实际执行器输出。
- `robot.control_allowed`：当前会话是否可申请/持有控制；false 时网页只读，仍允许 estop。
- `device`：firmware、backend=`test_targets`、stage、uptime_ms、last_cmd_ms、stopped_at_ms、stop_sequence、stop_reason、max_safety_gap_ms、publish_drops、free_heap；均为设备诊断信息，不是传感器测量。
- error 追加可选 `request_type` / `request_id`，供网页立即结束对应失败请求。
- `CLOCK_NOT_READY`、`STALE_COMMAND`、`READ_ONLY`、`UNSUPPORTED_MODE`、`CAMERA_OFFLINE`、`FAULT_ACTIVE` 为新增错误码。

主控首次 ping 建立每个会话的 Unix 时间估计，之前不发布传感器遥测；未校时的错误/急停 ACK 使用 ts=0 表示未建立时间基准。浏览器连接立即 ping。所有安全时限和来源新鲜度使用设备单调时钟。

本轮只实现 IDLE/MANUAL/HEALTH_CHECK；自主模式拒绝。MANUAL 在 CAM 超时、所有者断开、网络断开或有效非零命令过期时清零并回 IDLE，需重新申请模式。实际过期阈值 240 ms，周期任务 5 ms，为 250 ms 上限保留余量。未安装输出在所有 stage 中均不可开启。

健康结果中的 null 明确清除对应旧值；省略字段仍保留旧值。健康网页新鲜度为 2500 ms；机器人许可仍为 1000 ms。固件健康有效性另外要求采样仍在推进。传感器块只在新结果或失效变化时发送，不用 10 Hz 重发旧块延长有效期。

固件最多 4 个 WS 会话；整条入站消息不超过 64 KiB，仅接受未分片文本帧，JSON 深度上限 8，顶层不超过 8 个键且不允许重复键。模式/运动/恢复的请求时间相对会话时间基准落后超过 200 ms 或超前超过 100 ms 时拒绝；急停、零释放不受这个窗口限制。请求 id 若提供必须是非负安全整数。ping 的 ts 需为 2000–2100 年范围的 Unix 毫秒，id 为非负安全整数。

## 人脸跟随集成扩展（2026-09-12）

CAM 与主控 UART 115200 8N1，G/P 共用递增 uint32 seq：

```text
@G,seq,cam_ms,hand,label,score_milli,x0,y0,x1,y1,infer_ms*CRC8\r\n
@P,seq,cam_ms,found,score_milli,x0,y0,x1,y1,infer_ms*CRC8\r\n
```

CRC8 多项式 0x07、初值 0，覆盖 `G,...` 或 `P,...`，不含 @、* 和换行。score 为 0..1000，found 为 0/1，框在 320×240 内且必须有正面积；无目标时 score/四坐标全为零。测试黄金向量在 `tests/fixtures/vision_uart.txt`。CRC/字段/范围错误、重复和乱序帧不更新来源时钟；超过来源超时后重同步，先停止跟随。

原有 WebSocket 命令字段不变。新增或扩展的 telemetry 字段：

| 字段 | 语义 |
|---|---|
| `vision.person.seq` / `age_ms` | 人物来源序号、主控单调时钟计算的结果年龄；重复聚合不是新检测 |
| `video.stream_url` | 优先 `http://192.168.4.2/stream`；允许相对 URL 和 HTTP(S)，禁止凭据 URL |
| `video.source_width/height` / `protocol` | 320×240 / mjpeg |
| `imu.valid/calibrated/tilt_fault` / `age_ms` | 有效性、开机校准、倾角故障和来源年龄 |
| `imu.yaw_deg/pitch_deg/roll_deg` | 度；失效为 null。yaw 为相对航向 |
| `device.supported_modes` / `integration` | 当前固件能力和 observe/manual/follow 配置 |
| `robot.calibration_ready` / `motion_output_installed` | 校准已由操作者验证、实际 PWM 输出已安装 |

网页视频优先级：合法 `?stream=` → 合法遥测地址 → 同源 `/stream`。视频发生错误后 1–5 秒退避重试，切换画面源或 WS 断开取消重试。人物框独立过期，重复 seq 不刷新页面来源期限。

跟随只接受控制者的现有 `ping` 格式作保活，推荐每 100 ms；有效会话中只有递增 id 且 ts 新鲜的 ping 才续期。手动速度、控制者保活、控制计算输出分别 240 ms 内部过期；CAM/人物来源 490 ms 过期，预留调度余量。ping 不续手动速度；旁观者 ping 不续跟随。非零 cmd_vel 在跟随模式被拒绝，零值停止并退出跟随。手动/跟随切换、来源失效及故障恢复后均需显式重新进入模式。

跟随目标为人脸框，进入后用三个匹配新框的面积中位数记录近似距离；不提供身份识别、米制距离、实测轮速或自动搜索。来源无效、低置信度或目标匹配失败立即停止。全部运动经主控安全仲裁；网页不是物理 watchdog。

## HC-SR04 前方保护增量

主控新增可选 `front` 遥测块；旧设备缺块时网页显示未接入并禁用演示开关。`distance_cm` 是探头至前方回波物体的距离，不是目标人物距离。无效为 null，220 ms 及以上的采样年龄视为未知；网页还累加本地等待时间，不保留失效读数。

```json
{"front":{"enabled":true,"ready":true,"valid":true,"distance_cm":82,"age_ms":30,"status":"CLEAR","phase":"NONE","demo_ready":true,"demo_enabled":false,"release_required":false,"stop_reason":"mode_changed"}}
```

- status：DISABLED / UNCONFIGURED / UNKNOWN / CLEAR / WARN / SLOW / BLOCKED / STOPPED / BYPASS。
- phase：NONE / HALT / RIGHT / MARGIN / PASS / REACQUIRE。
- ready 是已验证保护配置；demo_ready 还要求已验证绕障参数和支持 follow。仅 UI Mock 使用合成配置。
- release_required 表示手动近障停车后须先发零速度。运动命令格式不变，网页不指定 GPIO、阈值、舵机 PWM 或绕行时间。

新增命令（默认关闭，一次演示完成后自动关闭）：

```json
{"type":"set_demo_bypass","ts":1700000000000,"request_id":42,"enabled":true}
```

仅当前 PERSON_FOLLOW 控制者可执行，保留时间戳有效期、只读阶段、所有者和急停约束；必须是 boolean。使用已有 ACK/error 及 request_id 关联。途中禁用会停止并回 IDLE。错误可能为 NOT_IN_FOLLOW / BYPASS_CALIBRATION_REQUIRED / FRONT_CALIBRATION_REQUIRED / FRONT_UNKNOWN / FRONT_RELEASE_REQUIRED，以及既有 CONTROL_BUSY / ESTOP_ACTIVE / READ_ONLY / STALE_COMMAND。

绕障临时覆盖跟随输出，但不续租目标、IMU、控制者或计算输出。动作失败、断链、急停均退出 IDLE，不自动重试。所有停止使用原校准中值；网页速度仍为控制输出，非实测轮速。详细标定和失败语义见 [超声波开发指南](ultrasonic-development.md)。

## 0915 向后兼容扩展

- 顶层 `tuning_profile`：`SAFE_BASELINE` / `DEMO_BALANCED` / `DIAGNOSTIC_RAW`。
- `vision.gesture.held/age_ms`：显示保持及距直接支持帧的年龄；`stable` 由主控确认，不再以网页置信度二次否决。
- `vision.person.predicted`：该框为明确标记的预测；`seq/age_ms` 仍来自最后实测，重复消息不续期。预测最大 700 ms，不授权运动。
- `health.hr_valid/hr_held/hr_age_ms` 与 `spo2_valid/spo2_held/spo2_age_ms` 独立；`quality` 为 0–1 诊断分数。旧数值字段保持 number/null。掉线、无手指、250 ms 样本超时清空数值。
- `imu.held/warning_tilt/rejected_frames/accepted_frames`：样本保持、倾角提示与计数；拒绝样本不刷新 `age_ms` 的来源时间。

详见 [0915 实现与验收边界](0915-demo-development.md)。

## 通话期间超声暂停（2026-09-29）

仅远程音频链路内部使用：家长 `/audio` WebSocket 连接时，中继经 Windows 网关向主板 `/audio` 发送文本帧 `{"type":"call_state","active":true}`，并每秒刷新；断开时发送 `active:false`。主板仅接受这两种精确格式，3 秒未收到刷新则自动恢复。此信号不由网页控制 WebSocket 的普通控制命令发送。

主板在通话有效期间保持 HC-SR04 的 TRIG 为 LOW，不启动新测距；`front.call_paused=true`，距离失效为 null。通话断开或租约超时后恢复周期测距。此变化不设置急停，也不禁用其他安全约束；但通话期间没有新的超声波前方障碍数据，不能依赖该探头避障。四角红外防跌落逻辑不受影响。
