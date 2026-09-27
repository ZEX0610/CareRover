# Windows 网关：当前阶段

本目录在车旁 Windows 电脑运行，电脑同时使用 iPhone USB 上网和 CareRover Wi-Fi。网关通过 SSH 回环隧道连接私有云中继；主板 WebSocket 遥测/控制、CAM MJPEG 上传和可选 `/audio` PCM 桥均不要求 ESP32 直接访问互联网。当前 AP-only `audio2` 主板与网关已完成同一台 Windows 上的双向短句实物测试；异地家长设备和长时间质量尚待验收。

使用前运行 `npm ci`、`npm test`。确认 `ssh.exe` 已信任原服务器主机密钥、本机私钥存在、云服务器中继服务在运行。PowerShell 执行 `./start-windows.ps1`；它从服务器通过 SSH 只读取设备令牌到当前进程内存，不写磁盘、不打印；以隐藏窗口启动仅绑定 `127.0.0.1:18088` 的 SSH 转发，退出时停止隧道。它不改变路由或 Wi-Fi 连接。`gateway.mjs` 每 10 秒打印只含连接/帧计数的状态。

CAM 固件同一时刻只允许一个视频观看者。网关上传远程视频时，请关闭本地 iPad 上的 CAM 视频页面；家长到私有 HTTPS 站点看 `/stream`。远程控制要先做四轮架空、舵机断电的遥测与零速度验证，再考虑上电运动。云中继的 `/health` 中 `controlDevice` 和 `videoFresh` 可验证网关是否在线，但不能替代真人音视频及小车动作验收。

启动时默认只转发控制/视频；主板已烧录支持 `/audio` 的固件后，执行 `./start-windows.ps1 -EnableAudio` 才会额外开启音频桥。音频为按住说话的半双工 Demo：家长下行时网关暂时抑制车端上行，避免车端扬声器声音绕回家长端。若当前主板仍是 AP-only 诊断应用，Server酱微信通知会暂停；这不是长期固件配置。退出网关进程或 Windows 电脑关机后远程视频、控制与通话都会失联。
