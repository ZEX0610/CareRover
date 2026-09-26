# CareRover 腕带原型（2026-09-24）

本目录是**独立手表侧原型**。小车主板、CAM 板固件均未改动；现在不能声称手表已经控制小车，也不能把电脑模拟测试当作实物验收。

## 已完成与未完成

- 已完成：腕部动作解析、PPG 演示估计、20 Hz `watch_v1` 遥测协议、Arduino 草图、电脑端确定性测试。
- 本机测试：`tests/watch_native_test.cpp` 用 MinGW GCC 6.3 / C++14 编译并运行通过，模拟波形输出 75 BPM、91% 演示 SpO2。Arduino CLI 使用 Waveshare ESP32-S3 Zero / esp32 3.3.10 / SparkFun MAX3010x 1.1.2 编译通过：程序 448148 B（34%），全局 RAM 33312 B（10%）。
- 未完成：真实 MPU/MAX 采样、电源板充电电流及纹波测量、佩戴标定、无线丢包测试、小车接收端、OLED/网页接入、实际运动。模块未到货，这些不得标为通过。

## 目录

- `firmware/CareRoverWatch/CareRoverWatch.ino`：手表独立草图，串口 115200 输出完整 JSON；填入本地 Wi-Fi 配置后还向小车 AP 地址发送 UDP。当前小车固件**没有**该协议接收器，因此 UDP 发送不等于控制生效。
- `watch_motion.h`：姿态和动作解析；`watch_health.h`：PPG 演示估计；`watch_protocol.h`：单包协议。
- `tests/watch_native_test.cpp`：不依赖 ESP32 的算法回归。
- `docs/WIRING_AND_BENCH_PLAN.md`：尺寸、摆放、接线、逐级上电、实测清单。
- `docs/INTEGRATION_CONTRACT.md`：未来与现有小车的接口和验收顺序。

无线台架测试前，将 `watch_secrets.example.h` 复制为同目录的 `watch_secrets.h`，再填小车 AP 的真实 SSID/密码。留空时仍可通过 USB 串口测试；真实密码文件已加入 `.gitignore`，不要上传。

## 本机重跑

在此目录 PowerShell 执行：

```powershell
& 'C:\Users\new20\MinGW\bin\g++.exe' -std=c++14 -Wall -Wextra -Werror -O2 'tests\watch_native_test.cpp' -o 'tests\watch_native_test.exe'
& 'tests\watch_native_test.exe'
```

输出 `watch_native_test PASS` 才算算法单元测试通过。该 `.exe` 是测试产物，不是固件。Arduino IDE 还须装 ESP32 板包、SparkFun MAX3010x Sensor Library，再选择 Waveshare ESP32-S3-Zero（或与板实物匹配的 ESP32-S3 Zero 配置）、USB CDC On Boot=Enabled、115200 串口。真实板 Flash/PSRAM 版本必须核对丝印，不能仅用卖家默认值。

本机 ESP32 链接器不能将含中文的路径作为 `--build-path`；源目录可保持中文，但构建目录需选 ASCII 路径。成功的 CLI 命令：

```powershell
& 'C:\Users\new20\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:waveshare_esp32_s3_zero --libraries 'C:\CareRover\build\deps' --build-path 'C:\Users\new20\AppData\Local\Temp\CareRoverWatch_build_0924' 'C:\Users\new20\Desktop\硬设资料\ESP32-S3开发\CareRover_Watch_Prototype_2026-09-24\firmware\CareRoverWatch'
```

## 用户提供的确定条件

电池照片标注 602025，约 26×20×6 mm、3.7 V、250 mAh，带保护板。MAX30102 放在右手手腕**手背侧**，不延伸到指尖。电池标称容量不是最大充电/放电电流证明；手腕反射式 SpO2 数值须与可靠指夹设备对照，现算法数值仅供大赛演示，不能当医疗读数。

## 参考资料

- [Waveshare ESP32-S3-Zero 产品页](https://docs.waveshare.net/ESP32-S3-Zero/) 与 [Arduino 设置](https://docs.waveshare.net/ESP32-S3-Zero/Arduino/)。
- [Analog Devices MAX30102 页面及数据手册](https://www.analog.com/en/products/max30102.html)；[腕部光学装配建议](https://www.analog.com/en/resources/technical-articles/guidelines-for-the-optomechanical-integration-of-heartrate-monitors-in-wearable-wrist-devices.html)。
- 本地商家模块资料见 `ESP32-S3开发/MAX30102心率血氧传感器`、`ESP32-S3开发/MPU-6050六轴传感器`、`ESP32-S3开发/手表电源模块` 与 `ESP32-S3开发/ESP32-S3 Zero.txt`。
