> Windows 接手：见 [资源、命令和可复制提示词](docs/WINDOWS_ULTRASONIC_HANDOFF.md)。

> **0915 更新：** 已合并现场修复并加入可选 Demo 优化。先读 [0915 开发与验收记录](docs/0915-demo-development.md)。根目录为当前开发源，原包保持归档；新代码未做本轮实物验收。以下 0912 说明保留为历史环境与联调流程。

> 新增 HC-SR04 前方保护与固定向右单障碍演示绕行：见 [接线、标定门禁与模拟演示](docs/ultrasonic-development.md)。实物引脚和机械参数尚未标定，默认未接入；真实保护与绕行分别验证后启用。

> 人脸跟随集成：参阅 [构建、校准与 Windows 联调](docs/tracking-development.md)。源码新增 CAM 人脸/手势/MJPEG、主控 IMU/舵机/跟随与安全仲裁。真机验收仍为 NOT RUN。参阅 [本机结果与实物验收表](docs/tracking-acceptance.md)。

> 原无线基线：`integration=legacy` 仍使用无执行器的测试目标。旧阶段记录见 [无线开发](docs/wireless-development.md) 和 [历史验收](docs/wireless-acceptance.md)；人物跟随与真实 PWM 使用上方的新集成说明。

# CareRover Console

双 ESP32-S3 全向移动健康机器人的网页控制台。无需硬件即可演示视频叠框、手势、心率 / 血氧 / PPG、全向摇杆、模式切换及急停。

原生 HTML、CSS、JavaScript ES Modules、Canvas。没有前端依赖、CDN 或构建步骤。默认中文，可切换英文。

## 最快体验：浏览器内模拟

环境：Python 3.10+，现代 Chrome / Edge / Safari。项目需要通过 HTTP 打开，不能直接双击 HTML。

macOS / Linux，在终端执行：

```bash
cd /path/to/spider-webite-dev
python3 -m http.server 8080 --bind 127.0.0.1
```

Windows，在项目目录打开 PowerShell：

```powershell
py -m http.server 8080 --bind 127.0.0.1
```

浏览器打开 <http://127.0.0.1:8080/>。这是 `?transport=mock` 模式，数据和命令在浏览器内模拟，不需要安装 Python 库。

## 完整通信验证：WebSocket + MJPEG Mock 服务

先用 Ctrl+C 停止上面的静态服务器，以免端口冲突。

macOS / Linux：

```bash
cd /path/to/spider-webite-dev
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r mock/requirements.txt
python mock/server.py
```

Windows PowerShell（无需修改脚本执行策略）：

```powershell
cd C:\path\to\spider-webite-dev
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r mock\requirements.txt
.\.venv\Scripts\python.exe mock\server.py
```

打开 <http://127.0.0.1:8080/?transport=ws>。这时浏览器实际通过 `/ws` 收发 JSON，并通过 `/stream` 播放本地生成的 MJPEG。画面和所有传感器结果仍是模拟数据。

要同时验证 WebSocket 与浏览器 Canvas 场景，可打开 <http://127.0.0.1:8080/?transport=ws&video=canvas>。

首次安装依赖需要网络；安装完成后运行完全离线。依赖已在 Python 3.14 / aiohttp 3.14.3 / Pillow 12.3.0 验证。

## 操作

1. 等待右上角“已连接”。
2. 点击底部“手动”，等机器人回报确认；拖动摇杆，观察 vx / vy 和设备速度。
3. 向右上拖：vx、vy 都为正；松手立即归零。速度上限可调 20–100%。
4. 按住左右旋转按钮控制 wz；松开停止。键盘 W/A/S/D 平移、Q/E 旋转、空格停止、Esc 急停。
5. 切换跟随 / 手势 / 健康模式，手动输入随即禁用。跟随和手势行为由模拟机器人决策。
6. 点击红色“紧急停止”。所有运动锁定；点击“解除急停”并确认，机器人回到待机，不自动恢复之前的运动。
7. 展开“调试与联调”，点击“模拟断线”，观察重连和速度归零。也可以停止 Python 服务，再启动，验证真实服务重启。
8. 画面源可切换本地视频，再点击“打开文件”选取视频；仅在本机播放，不上传。
9. 录制、导出 JSON 后可回放；回放期间不发送任何机器人指令，结束后点击“返回实时”。录制数据只保存在本机。

Mock 健康数据每隔一段时间模拟手指离开，读数显示“—”；这不是故障。原型健康监测不可用于医疗诊断。

手机测试：在可信局域网执行 `python mock/server.py --host 0.0.0.0`，手机访问电脑的局域网 IP 和 8080 端口。只使用同一服务器提供的页面；Mock 服务阻止其他来源网页控制。不要将开发服务器直接暴露到互联网。

## 结构

| 路径 | 职责 |
| --- | --- |
| `index.html`, `css/app.css` | 界面、响应式布局、设计系统 |
| `js/app.js` | 应用生命周期、状态确认、发送频率与安全协调 |
| `js/state.js`, `js/protocol.js` | 状态、固定容量 PPG 缓冲、协议校验 |
| `js/transport.js`, `js/mock-transport.js` | WebSocket / 浏览器 Mock 统一接口 |
| `js/sim.js` | 浏览器内机器人行为模拟 |
| `js/joystick.js` | Pointer Events、触摸、键盘和输入释放 |
| `js/video.js`, `js/video-overlay.js` | 合成画面 / 视频源、contain 坐标映射 |
| `js/telemetry.js`, `js/debug.js`, `js/i18n.js` | 波形、录制回放、中英文本 |
| `mock/server.py` | Python WebSocket / MJPEG 仿真服务 |
| `docs/` | 协议、架构、设计和验收记录 |
| `tests/` | Node 核心 / 输入测试、Python 网络集成测试 |

## 验证

开发测试可选 Node 22+；运行产品不需要 Node 或 npm。

```bash
node --test tests/*.test.js
python -m unittest discover -s tests -p 'test_*.py' -v
```

第二条命令在已安装 Mock 依赖的虚拟环境执行。GitHub Actions 会运行这两套检查。

## 最终接入：主 ESP32-S3 无线网关（不使用 USB）

把 `index.html`、`css/`、`js/`、`assets/` 放入主 ESP32-S3 的 FFat 静态文件系统。主控建立 CareRover Wi-Fi 热点，浏览器或手机连入后访问 `http://192.168.4.1/?transport=ws`。

- 主控提供 `GET /` 与同源 `WS /ws`，聚合 CAM 手势、MAX30102、未来 IMU、运动和电池数据。
- WS 帧遵循 [通信协议](docs/protocol.md)，UI 不读取 UART，也不计算轮子运动学。
- CAM 继续通过当前 115200 baud UART 与主控通信。后续需要实景视频时，CAM 可接入同一热点并单独提供 MJPEG `/stream`。
- 可用 `?transport=ws&ws=ws%3A%2F%2F192.168.4.1%2Fws` 显式配置地址；正常部署优先直接使用同源地址。
- 真机模式不加载浏览器模拟器。模拟器和 Python 后端均不需要烧录运行。
- 主控必须独立执行超时停车、故障优先级、急停锁定和多客户端控制仲裁。前端不能替代固件安全逻辑。

`hardware/serial_bridge.py` 是已验证的 USB 开发桥，便于在无线固件完成前用 COM 口核对真实传感器数据；它不是比赛最终运行方式。

## Git 与发布

仓库：<https://github.com/NeedleAss/spider-webite-dev>。原有代码已经单独保存为基线提交。改动按功能提交，推送前运行测试、检查差异，不使用 force push。

`.gitignore` 排除了 `.venv`、`.env`、日志、缓存和 `.DS_Store`。GitHub 上只应包含源码和项目文档；本地录制文件不应随手加入版本控制。GitHub Pages 可以托管静态页面和浏览器内 Mock，但不能直接读取开发板数据，也不是最终实时链路。比赛运行时网页由主控本地托管，GitHub 仅用于源码同步、版本管理和协作；本项目没有自动启用 Pages 或其他云服务。
