# 超声波软件验证记录

日期：2026-09-12。当前交付包含原先未提交的人脸跟随/IMU/运动集成，以及新增 HC-SR04 前方保护。历史原始参考包在本地保留；当前构建不依赖该目录。

| 项目 | 结果 | 验证范围 |
|---|---|---|
| Node | PASS，18 项 | 原有协议/控制回归，加测距净化与过期显示、释放锁、模拟完整绕行与停止 |
| Python | PASS，24 项 | 原有协议/构建工具回归，加超声波模拟、真实 WS 命令、控制者隔离与旁观者不能续期 |
| C++ 主机测试 | PASS，5 套 | safety / tracking / drive / front / ultrasonic；front 包含五个绕障阶段各八种独立故障场景 |
| ASan / UBSan | PASS | front_test.cpp，包含完整绕障、40 个故障/阶段组合及保护边界 |
| 浏览器 | PASS | 浏览器 Mock 与实际 Python WS Mock 均走完所有绕障阶段并恢复跟随；第二次绕障急停取消；开关初始禁用/默认关闭；中英文显示 |
| 页面布局 | PASS | 1280×1000、390×844，无横向溢出；截图已目视检查；浏览器 pageerror 为 0 |
| ESP32-S3 主控 | PASS | Arduino-ESP32 3.3.10，stage 5 follow，compile_only；代码 1,084,478 字节，静态 RAM 56,288 字节 |
| 实物 / Windows | NOT RUN | 未烧录；引脚、电平、安装、停止距离、横移余量、越过时间、整机并行和稳定性均需现场验收 |

本机验证日志位于 `output/evidence/ultrasonic/`，浏览器截图位于 `output/playwright/front-desktop.png` 和 `front-mobile.png`。这些运行产物不随 Git 提交；测试源码与重跑命令随源码提交。固件包位于 `build/stage5-follow-56e70af209caf9a1-check`，内容版本 `56e70af209caf9a1`，不允许烧录到真实设备。后续修改重新构建后以新 manifest 为准。

主机和 Mock 均不是实物避障证明。HC-SR04 只覆盖前向有限声束；右侧/出口由演示场地保证，清空确认必须有有效远距回波。机械停止和时间开环位移必须独立实测。接手操作见 [开发与标定指南](ultrasonic-development.md)。
