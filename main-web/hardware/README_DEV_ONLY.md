# USB 串口网页桥（仅开发期）

此目录把主控 115200 baud 串口 JSON 转换为本机网页 `/ws`，用于无线固件完成前的真机联调。**比赛最终运行不使用它，也不需要 USB。**

## Windows 使用

```powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r hardware\requirements.txt
.\hardware\start_windows.cmd
```

输入主控 COM 号后打开脚本提示的本机地址。Arduino IDE 串口监视器和友善串口调试助手须先关闭，避免抢占 COM。

桥接器允许查看手势、健康、SQI 与 25 Hz PPG；当前不转发实际运动。非零 `cmd_vel`、模式与解除急停会返回 `MOTION_NOT_INSTALLED`，零速度安全释放命令静默接受。

运行测试：

```powershell
python -m unittest tests\test_serial_protocol_adapter.py -v
npm test
```
