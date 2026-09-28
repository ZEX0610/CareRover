# CareRover 远程中继迁移记录（2026-09-29）

## 当前拓扑

- 新云主机：`42.81.93.16`，Ubuntu 22.04，4 vCPU / 8 GB。旧云主机 `221.194.149.100` 未删除，但不再作为远程中继使用。
- Tailscale 私有地址：新中继 `100.126.153.36`；MagicDNS 名称和家长端 HTTPS origin 仍为 `carerover-relay.tail86bfa5.ts.net`。旧 Tailscale 设备已从当前 tailnet 移除。未启用 Funnel。
- 中继进程：`carerover-remote-hub.service`，开机启动，仅监听新云主机 `127.0.0.1:8088`。Tailscale Serve 的私有 HTTPS `443` 代理到此回环端口。家长端无需连接小车 Wi-Fi，不得直接访问 `192.168.4.1/.2`。
- 车旁 Windows：iPhone USB 提供互联网、Wi-Fi 连接 CareRover；`gateway/start-windows.ps1 -EnableAudio` 通过 SSH 在本机建立 `127.0.0.1:18088` 隧道，再转发主板控制/遥测、CAM 视频和双向音频。网关占用 CAM 的单观看者连接；同时打开本地 `/stream` 可能得不到视频。
- 家长端：加入 `new20070610` tailnet，访问 `https://carerover-relay.tail86bfa5.ts.net/`。浏览器若使用系统代理，需对该私有域名直连或使用无代理浏览器。

## 迁移验证与边界

- 新主机已安装 Node 24.21.0 与 Tailscale 1.102.4；中继使用独立非登录 `carerover` 服务账号运行。部署包 SHA-256 在传输前后核对一致。
- `relay` 的 19 项测试和 `gateway` 的 2 项测试通过。新主机服务为 `active/enabled`；Tailscale Serve 显示 `tailnet only`。
- 车旁 Windows 的双网络、主板 HTTP 与 CAM HTTP 连通；网关日志显示 `boardUp`、`relayUp`、`cameraUp`、`audioBoardUp`、`audioRelayUp` 均为 `true`，视频与麦克风帧持续上行。
- 从 Windows 验证私有 HTTPS 证书与 `/health`、家长认证、控制台及 MJPEG `/stream` 响应。iPhone182 通过原网址已看到远程视频，并在现场确认手机能听到小车麦克风、按住说话时小车扬声器能听到手机；服务器同时观察到双向音频帧。远程手动控制、实际运动以及长时间稳定性仍待专项验收，不能把帧数或 HTTP 200 当作这些验收的替代。
- 实测期间 CAM 一度从局域网消失，主板 HTTP 与服务器保持正常，网关自动重连后视频恢复；此故障说明 CAM 供电/Wi-Fi/HTTP 稳定性仍需单独观察，不能将瞬时画面恢复视作长稳通过。
- 迁移不改小车主板、CAM 和手表固件。远程视频需要车旁 Windows 持续开机、两路网络稳定且网关运行。旧云主机不再用于故障回退，除非重新配置其 Tailscale、令牌和网关。

## 凭据与客户端

新主机在 `/etc/carerover/remote-hub.env` 中生成新的随机 `AUDIO_PARENT_TOKEN` 和 `AUDIO_DEVICE_TOKEN`；二者未写入 Git 或本记录。旧家长会话、旧令牌不会自动迁移。家长需在网页重新登录；原生 iPhone App 若缓存旧令牌，也必须在 App 的安全存储中替换。后端接口与原 HTTPS origin 不变，App 开发参考 `../docs/IOS_CLIENT_API.md`。不要通过聊天、截图、代码、日志或 GitHub 传播令牌。已在聊天中暴露过的新云主机 root 密码应另行轮换；SSH 公钥访问已验证。

## 运维检查

在新主机上检查：`systemctl status carerover-remote-hub.service`、`tailscale serve status`、`tailscale status`。在车旁 Windows 上检查 `http://127.0.0.1:18088/health` 和网关日志；若回环端口不监听，先确认 iPhone USB 上网与 CareRover Wi-Fi 同时可用，再运行 `gateway/start-windows.ps1 -EnableAudio`。不要同时启动第二份网关抢占 CAM 视频连接。若更换主机，先完成新端验证，再切换 Tailscale 设备名和 Windows 脚本中的 SSH 地址。

安全验收时四轮架空、舵机 5 V 断开；先只检查 iPhone 视频、人物框/遥测与双向语音。功放独立 5 V 接通前确认无发热/啸叫风险。远程运动须另行在安全条件下验证，不能因网页可登录就推断已经可控。
