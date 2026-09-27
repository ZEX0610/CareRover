# 远程服务器现场进度（2026-09-26）

## 已核验

- 公网主机 `<SERVER_PUBLIC_IP>`，Ubuntu 22.04.4 LTS，x86_64，4 vCPU / 8 GB。系统盘 40 GB，约 34 GB 可用；额外 40 GB `vdb` 未挂载。
- SSH 主机 ED25519 指纹经云平台控制台与本机交叉核对：`SHA256:cMfcdcr3N0Coq19t/Ad/EIxizSnQ63jKq/FsyCzyRXo`。
- v2 SSH 公钥指纹 `SHA256:fVmxnAa5NZIGY97iMlKELhu4qmjKaSUMo8mq/RPTIj8` 已验证可登录；本机实际使用的无口令私钥为 `C:\Users\new20\.ssh\carerover_remote_v2`，不要上传或提交。
- SSH 配置新增 `/etc/ssh/sshd_config.d/01-carerover-keyonly.conf`，`sshd -t` 通过；生效值 `PasswordAuthentication no`、`KbdInteractiveAuthentication no`、`PermitRootLogin prohibit-password`、`PubkeyAuthentication yes`；重载后公钥登录复测通过。云安全组仍应把 TCP 22 限定管理来源。
- 日志出现来自另一公网地址的多次 SSH 密码猜测，所见记录均为失败，未据此证明入侵。该地址不是本机管理出口 IP。密码登录现已关闭；用户仍应检查云平台登录审计及重置曾展示在截图中的密码。
- 服务器原无 Node.js；从 Node.js 官方站点安装 v24.21.0 LTS 到 `/opt/node-v24.21.0-linux-x64`，压缩包与官方 `SHASUMS256.txt` 校验通过，没有覆盖系统包。
- 创建非登录系统账号 `carerover`。中继源码（不含令牌、私钥、`node_modules`）传输前后 SHA-256 一致，已解压到 `/opt/carerover/remote-hub/relay`，由服务账号拥有。
- 服务账号执行 `npm ci --ignore-scripts --no-audit --no-fund` 成功；服务器端 `npm test`：6/6 通过。

## 尚未执行 / 未验收

- 未启动常驻服务，未创建生产令牌，未向公网开放 80/443，未部署受信任 HTTPS 证书，也未接入域名或做备案流程。
- 未连接车端的 `/device/ws`、`/device/frame` 和 `/audio`；INMP441/MAX98357 的实物接线、固件并发和家长手机双向通话均未验收。
- Node.js 解压包和上传归档目前仍在服务器临时目录；后续确认部署后可清理，先保留以便排障。新增的 40 GB 数据盘未动。

## 下一阶段

1. 用户已选定长期路径：拟在阿里云购买域名并办理大陆网站备案。南京云服务器由**并行智算云**租赁；域名注册商与备案接入商不同，备案资格及办理入口需向并行智算云客服/工单核实，特别是这台按量计费实例是否符合备案条件。待域名和备案资格/状态明确后，将子域名 `A` 记录指向公网 IP，配置可信 HTTPS 证书；在此之前不开放公网网页。
2. 确定 HTTPS 入口后，生成独立的家长/设备高熵令牌，配置只对服务账号可读的环境文件与证书，安装 systemd 服务并先做服务器本机及远程手机验收。
3. 在独立音频板实测通过、当前两板固件备份完成后，再分阶段实现车端上行与音频；不能把当前 6/6 软件测试当成整车远程通话验收。

## 2026-09-26 私有演示路径增量

- 用户已确认 Mac 可通过 USB 使用 iPhone 蜂窝网络上网，同时通过 Wi-Fi 接入小车 AP；Mac 浏览器可同时打开 `http://192.168.4.1` 和互联网网站，macOS 版本报告为 27.0。尚待 Mac 的路由表与持续连接实测。
- 服务器从 Tailscale 官方 Ubuntu 22.04 软件源安装 `tailscale 1.102.4`；`tailscaled` 已 active。登录进展见下节；**子网路由未批准，公网网页端口未开放。**
- 独立远程中继加入 `AUDIO_PROXY_PUBLIC_ORIGIN` 模式：HTTP 后端只监听 `127.0.0.1`，预期由服务器上的 Tailscale Serve 提供私有 HTTPS。该模式使用精确的 HTTPS Origin 检查、`Secure` 会话 Cookie 和 WSS CSP；本机 `npm test` 7/7 通过。源码已上传服务器，但**真实 Tailscale Serve 尚未验收**。
- Mac 与服务器已加入同一 tailnet；仅在确认小车网段没有冲突后，由 Mac 广播 `192.168.4.0/24` 并在管理台批准，再测试服务器私下访问主板/CAM。
- 现有两块板尚无云端音频上传。原网页的远程视频、人物框、手势、遥测、运动控制需独立桥接与断线停车验证；麦克风/功放未接线，不能宣称远程通话或控制已经实物可用。

## 2026-09-26 Tailscale 联网复核

- 用户在 Mac 上确认 Tailscale 地址 `100.75.183.9`（`eddys-macbook-pro`），并使用同一账号批准服务器加入 tailnet。服务器 `carerover-relay` 的 Tailscale 地址为 `100.97.241.31`；两端已在 `tailscale status` 中互见。
- 服务器已安装并校验代理模式的独立中继源码，服务器端 `npm test` 现为 7/7 通过。服务仍未常驻启动，也未配置令牌或对外开放公网网页端口。
- 服务器 `tailscale netcheck` 显示 UDP 可用、IPv4 公网端点可见；对 Mac 的 5 次 `tailscale ping` 均经 `DERP(sfo)`，往返约 484–968 ms，未建立直连。**私有互通成立，但当前质量不足以认定实时音视频或驾驶控制可用。**
- 服务器进程正在 `0.0.0.0:41641/udp` 监听，服务器本机 `ufw` 未启用；并行智算云安全组的入站 UDP 规则尚未核实，因此暂不能归因于 Mac 端蜂窝网络。
- Mac 端 `tailscale netcheck`：UDP 可用，IPv4/IPv6 可用，`MappingVariesByDestIP: true`，最近 DERP 为旧金山（约 181 ms）。这是直连困难的线索，但尚不能排除云平台安全组。Mac 到 `192.168.4.1` 的路由明确走 `en0`；`1.1.1.1` 经 `utun4`，不能仅凭该地址判断整个互联网的默认路由；Mac 浏览器此前已实测互联网和小车网页同时可用。
- 固件核对：主板 AP 固定 `192.168.4.1/24`，CAM 静态 `192.168.4.2/24`。服务器没有与该网段冲突的现有路由；已设 `tailscale set --accept-routes=true`，待 Mac 宣告并由管理员批准小车子网后生效。已请求用户在 Mac 宣告 `192.168.4.0/24`，尚待回报；服务器目前仍不能访问主板/CAM。
- 下一阶段先诊断直连与更近中继的可能性，再由 Mac 广播小车网段、管理员批准路由、服务器接受路由，并实测主板/CAM 的访问延迟与稳定性；在此之前不启用远程运动控制。
- 用户已在 Mac 运行 `Tailscale set --advertise-routes=192.168.4.0/24`，命令未输出错误。随后服务器 `tailscale status --json` 中 Mac 的 `AllowedIPs` 暂仍只有其 Tailscale 单机地址，服务器到 `192.168.4.1` 仍走默认 `eth0`；已请用户在 Tailscale 管理页对 `eddys-macbook-pro` 勾选并保存该子网路由，尚待批准结果。

## 2026-09-26 子网路由批准后的首测

- 用户确认管理页已批准 `192.168.4.0/24`。服务器现将主板 `192.168.4.1` 与 CAM `192.168.4.2` 都路由到 `tailscale0`，Mac peer 的 `AllowedIPs`、`PrimaryRoutes` 均显示 `192.168.4.0/24`；路由宣告/批准/接受这一层已完成。
- 但服务器对两块板的 HTTP 连接均在 5 秒连接阶段超时；对主板的 ICMP 2/2 丢失。服务器仍能通过 Tailscale ping Mac，当前经 `DERP(sfo)` 约 471–481 ms。**不能把“路由已生效”误判为板端服务可达。**
- 正在请求 Mac 本地 `curl`、`net.inet.ip.forwarding` 和批准后到两块板的路由结果，区分本地板端断线、路由环路、转发或 ACL/防火墙问题；未发送运动命令，也未切换电脑网络。
- 已建立可重复的红灯测试：服务器连续两次 `curl --connect-timeout 2 http://192.168.4.1/` 均在 2 秒连接阶段超时（HTTP 000）；Mac 的 Tailscale 地址仍可 ping 通，约 471–481 ms、经 DERP(sfo)。服务器 netmap 中已出现批准的主路由和包含 `192.168.4.0/24` 的过滤规则，但尚不能仅凭此断言完整端到端 ACL 无问题。
- Mac 现场只读检查：`net.inet.ip.forwarding=0`；Mac 到主板 `.1` 和 CAM `.2` 的路由均走 `en0`；本地 HTTP 连接分别约 11 ms 与 10 ms（状态码 302、404，证明 TCP 可达，未要求根路径一定返回 200）。由此排除板端断线和本地路由环路；接下来仅临时设 `sudo sysctl -w net.inet.ip.forwarding=1` 做单变量测试，然后立即重跑服务器端红灯测试；若无改善恢复为 0。

## 2026-09-26 后续链路定位

- 用户确认 Mac 已连接小车 Wi-Fi，Mac 本地 `curl http://192.168.4.1/` 返回 302，`route -n get 192.168.4.1` 走 `en0`。服务器通过 Tailscale 可 ping 到 Mac（仍经 DERP(sfo)，往返约 432–944 ms），对主板和 CAM 的 HTTP 请求仍连接超时。
- 服务器路由指向 `tailscale0`；服务器抓包已看到发往 `192.168.4.1:80` 的 TCP SYN 重传，但没有收到对应的握手响应。Mac 上第一次宽泛抓包混入大量正常网页流量，只有汇总计数，无法确认上述 SYN 是否经过 `en0`。已请求在 Mac 关闭本地小车网页后用仅匹配目标 SYN 的过滤条件重测，并回传实际包头行。
- 临时将 Mac `net.inet.ip.forwarding` 设为 1 未让链路接通。Tailscale 官方文档说明 macOS 宣告子网路由时会自动处理转发；这项手动设置不应当作已修复依据。待定向抓包完成后恢复现场原值 0。
- 音频模块仍未接线或烧录整车；远程视频、控制与通话均未验收。当前只向用户提供了与现有 GPIO 不冲突的 INMP441/MAX98357A 接线表，等待断电接线和实物核对。

## 2026-09-26 Mac 反向隧道首测

- 定向抓包在 Mac 小车 Wi-Fi 网卡 `en0` 未捕获服务器请求对应的 TCP SYN；服务器侧确认请求已进入 `tailscale0`。因 Mac 子网转发仍未打通，演示改试由 Mac 主动连服务器的 SSH 反向隧道，不改变 Mac 的双网络连接。
- 服务器创建了独立无登录 shell 的 `carerover-tunnel` 账号，公钥仅准许在服务器 `127.0.0.1:18081`、`:18082` 建立远程监听。Mac 私钥不在服务器或仓库；当前隧道需 Mac 终端保持运行，未做开机自启。
- 服务器两个监听端口已建立。CAM `127.0.0.1:18082/` 返回 HTTP 404（CAM 根路径正常现象），`/stream` 返回 HTTP 200，在 4 秒内收到约 201 KB MJPEG，证明 CAM 视频链路经隧道可传输。
- 主板 `127.0.0.1:18081/` 的 TCP 可建立，但 HTTP 返回空响应；保留原 `Host: 192.168.4.1` 后仍然如此，连续三次复测相同。已请求 Mac 本地重新 `curl http://192.168.4.1/`、查看隧道终端是否有 `channel ... open failed`，并短暂关闭其他小车网页排除主板 HTTP 套接字占满。主板链路和远程网页控制均**尚未验收**。
- Mac 本地随后也曾出现 `curl: (56) Recv failure: Connection reset by peer`，网页已关闭且隧道终端无 `channel ... open failed`。在舵机与功放 5 V 均断开时，用户按主板 RST 一次；重启后服务器经隧道两次获得主板 HTTP 302（约 2–3 秒），证明主板路径曾恢复。CAM 在 AP 重启后短时不稳定，但之后又返回过 HTTP 404；无需把它描述为已由远程软件重启。
- 后续三轮主板/CAM 低频探测均超时。用户随即报告 Mac 已断网，并要求稍后再试；本轮立即停止探测。故 SSH 隧道与两板视频/控制只算**短时局部通路已证明**，稳定性与断网恢复均未验收。Mac 网络恢复后先核对 Mac 本地 `.1/.2` HTTP、双网路由和 SSH 隧道状态，再做服务器端短时连通及视频测试；不应直接启动远程运动或音频。

## 2026-09-26 Mac 重联网后复测（进行中）

- 用户报告 Mac 已恢复有线互联网及小车 Wi-Fi、两块板经 USB 连至 Windows。Mac 重新运行反向 SSH 隧道后，服务器 `127.0.0.1:18081`、`:18082` 两个监听恢复。主板首页曾经返回 HTTP 302（约 1.4 秒），CAM 根路径曾返回 404（约 1.9 秒），但后续短时探测又超时；**仅监听存在不代表板端应用连接稳定**。
- Mac 独立测试 CAM `http://192.168.4.2/stream`，5 秒超时且收到 0 字节（`cam_status=000`）。这说明此次视频失败不只发生在服务器/Tailscale 段；Mac 对两块板首页的单独直连测试待回报，用以区分本地 Wi-Fi/HTTP 与 CAM 视频专属问题。
- Windows 即时设备枚举显示 USB-SERIAL CH340 `COM3` 与 `COM6` 均为 OK。短时只读监听 `COM3`：CAM 定期输出 `gesture_fps≈6`、`face_fps≈2.8–3.4`、`wifi=true`、`video_capture_failures=0`，当时 `jpeg_fps=0`、`stream_fps=0`（没有成功的视频观看会话）。短时只读监听 `COM6`：主板报告 `ap=true`、`mode=IDLE`、`estop=false`、`fault=false`，其他传感器任务仍在运行。串口任务活跃不能单独证明 HTTP 可达。
- 服务器与 Mac 的 Tailscale 连接仍经 `DERP(nue)`（约 316–380 ms），未建立点对点直连；此延迟可能降低最终视频观感，但不能解释 Mac 本地直连 CAM 也 0 字节的全部现象。
- 本轮未改固件、未烧录、未启用舵机/功放、未启动远程运动或音频服务。下一步依 Mac 本地 `.1` 与 `.2` 首页结果，再决定检查 Wi-Fi 链路、HTTP 套接字或 CAM 视频任务。
- Mac 本地首页复测：主板 HTTP 302 耗时 4.01 秒、CAM 根路径 HTTP 404 耗时 2.45 秒，均可达但远慢于此前约 10 毫秒。Mac 连续 5 次 ping 主板丢 1 包（20%，回复约 8–81 ms）；5 次 ping CAM 全部超时。根路径 404 是 CAM 预期响应，不是应用错误。
- 再次只读监听 CAM 串口，`wifi=true`、手势约 6.1 FPS、人脸约 2.8 FPS，`jpeg_total` 不变且 `stream_fps=0`。CAM 仍报告拿到 IP 的连接状态，但 Mac 当时无法稳定访问该 IP。正在向用户核对 Mac 的 Wi-Fi 地址/SSID 和是否有其他客户端；尚不能确定是无线丢包、地址冲突、ARP、AP 客户端隔离还是 CAM 网络任务阻塞。
- Mac `en0` 为 `192.168.4.3/24`，与 CAM 静态 `.2` 无地址冲突；`ifconfig` 显示 `status: active`，到主板的路由走 `en0`，用户确认菜单栏连接 CareRover。`networksetup -getairportnetwork en0` 却返回“not associated”，与同一时段的实际 HTTP/ping/网卡状态冲突，暂不据此判定断线。iPad 此刻未连接小车 Wi-Fi。已请求对 CAM `/stream` 做连接/首字节分解计时，并尝试同步串口指标。
- Mac 单次直连 CAM `/stream` 8 秒测试：TCP 连接耗时约 4.804 秒、未收到 HTTP 首字节，0 字节后超时。服务器经现有 SSH 隧道的单次 `/stream` 测试：本机反向监听连接立即完成，约 4.18 秒收到首字节，8 秒内仅收到 36,931 字节，HTTP 200。两次测试可能时间相邻且 CAM 仅允许一个观看者，不能用其绝对吞吐直接比较，但均证明当前视频链路严重退化。
- CAM 串口监听期间识别任务继续运行；视频 JPEG 总数由 527 变为 529，随后一段监听内不再增长，说明仅偶发视频帧通过。下一步检查 Mac 的 CAM ARP 条目、无线信号及距离；不先修改识别/编码参数。

## 2026-09-26 本地无线故障进一步定位

- Mac 的 ARP 表中主板 `.1` 与 CAM `.2` 都有 MAC 地址；Mac 与小车距离很近、无遮挡。Mac `en0` 为 `.3`，因此未见直接 IP 冲突或单纯距离问题。
- 经反向隧道的同一条短测：主板首页 HTTP 302 首字节 5.39 秒；CAM 根路径 HTTP 404 首字节 4.62 秒；CAM `/stream` 6 秒 0 字节。结合之前 Mac 直连主板首页约 4 秒、CAM 根路径约 2.45 秒，慢点已经出现在 Mac↔小车本地链路/板端处理，不是公网/Tailscale 独有，也不是仅视频编码路径。
- 用户报告电脑直连主板热点时网页和视频仍卡；尚待确认是 Mac 还是另一台电脑。当前 Windows `netsh wlan show interfaces` 显示它连接 `Tsinghua-Secure` 5 GHz，并非小车 AP，本轮未主动切换 Windows 网络，也不能把 Windows 的当前连接当成本地对照。
- 源码主板在通知功能已配置时执行 `WiFi.mode(WIFI_AP_STA)`，并以 `WiFi.softAP(..., channel=1, ..., max_connection=4)` 启动 AP；同时 `serverchanBegin()` 会让 STA 连接独立热点。Espressif ESP32-S3 文档确认 AP+STA 的 home channel 相同，外部热点的 STA channel 优先，AP 会随之迁移信道（[官方文档](https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s3/api-guides/wifi.html)）。这是待测假设，不应仅凭源码断言它造成当前卡顿。
- 下一步先用 iPad 直接连小车作跨客户端对照，并读取 Mac 当前关联 CareRover 的 RSSI/信道；若两客户端都慢，再做 AP+STA/热点状态与重启前后单变量 A/B。先不刷固件、不修改 Wi-Fi 配置、不启用远程运动/音频。
- 用户补充 Mac 与 iPad 直连小车网页/视频都很卡。Mac `wdutil`：RSSI `-52 dBm`、噪声 `-96 dBm`、关联速率 `72 Mbps`、当前 AP 信道 `2g1/20`；信号与距离不足以直接解释数秒级 HTTP 慢响应。历史实测同一 AP+STA 通知配置下 iPad 视频约 6 FPS 且推送时无明显变慢（见 `docs/windows-progress.md` 2026-09-26 条目），故 AP+STA 是待验因素，不是已证实根因。
- 用户确认网页 IDLE、舵机和功放 5 V 断开、四轮架空，主板与 CAM 均仅由电脑 USB 供电。只按主板 RST 一次后，iPad 网页可显示且频繁断连改善，但视频仍空白。CAM 未重启、未刷写。
- 空白视频期间 CAM 串口 `jpeg_fps≈5.7–5.8`、`stream_fps=0–0.44`、`video_capture_failures=0`、`wifi=true`，表明摄像头采集/JPEG 编码在工作，帧发送严重滞后。主板重启导致 Mac 反向 SSH 隧道至少 CAM 端口监听消失，服务器 `18082` 拒绝连接；该服务器测量不能用于评价重启后的本地视频。
- 用户关闭 iPad/Mac 全部小车网页并等待后，CAM `jpeg_fps` 立即回到 0、`jpeg_total` 保持 797，不支持“旧观看者永久占用”的假设。下一步只由 Mac 单客户端直接请求 CAM `/stream` 做重启后的传输复测，避免双观看者干扰。
- Mac 暂不在用户手边，改由 iPhone 直连视频。初次手机回复后串口 50 秒 `jpeg_fps=0`，经核对手机直开 CAM URL 实际显示“无法打开”，不能把先前屏幕静止图像当作持续实时流。该 iPhone 与原先给主板提供外部联网热点的 iPhone16 是同一部，故当前连接小车观看时网络拓扑已与此前工作状态不同。
- 用户只断开 CAM USB 5 秒并重新插回，主板未重启；COM3 恢复，CAM `jpeg_total=0`、识别任务和 `wifi=true` 正常、启动后 `min_heap≈32 KB`（重启前约 24.7 KB）。CAM 单独重启后，手机直连 `/stream` 时 `jpeg_fps≈5–6`、`stream_fps≈0–1`，数秒后编码停止并再次重试；断电重启未恢复持续视频，弱化“CAM 旧会话/暂态死锁”假设。
- 已请用户恢复原网络角色：iPhone16 只提供外部热点、iPad 接入小车 AP 观看，不动 Windows 网络，再测网页观感与 CAM 编码/发送指标。当前不应因为手机同时兼作热点来源和小车观看端的混合实验下定论；若恢复原拓扑仍差，继续查 CAM↔主板 AP 的无线质量/射频布局与 HTTP 发送背压。
- 原拓扑恢复后 iPad 获得 `192.168.4.5/24`，iPhone 显示主板已连接上网热点，但 iPad 访问 `http://192.168.4.1/` 连接超时；CAM 串口仍 `wifi=true` 且识别活跃。Arduino ESP32 core 3.3.10 的 `WiFiAP.cpp` 确认 `softAPConfig` 第四参数确为 `dhcp_lease_start`，项目配置 `.3`，不支持将 CAM `.2` 与 DHCP 租约冲突作为当前解释。
- 随后只读打开主板 COM6（显式关闭 DTR/RTS）仍触发了板上自动复位，串口打印 `rst:0x1 (POWERON)`。这是一次**由串口打开引起的主板重启**，不是纯观察；今后诊断避免再次打开 COM6 破坏 A/B 状态。启动日志 `ap_ready`、`mode=IDLE`、`estop=false`、`fault=false`、`min_heap≈194–200 KB`，`vision_link.valid` 连续递增且 `bad=0`；未见 HTTP/内存/视觉串口错误。
- 此次主板复位、保持 iPhone 外部热点 + iPad 小车 AP 的原拓扑后，用户报告 iPad 首页可打开、视频明显更流畅，2 分钟观察结束后再次确认画面比较流畅。CAM COM3 同步 `jpeg_fps≈5.4–6.0`、`stream_fps≈5.4–6.3`。随后在 iPad 视频前台持续 2 分钟的 57 个样本：成功发送平均 `5.46 FPS`、范围 `4.79–6.23`，编码平均 `5.56 FPS`，没有发送低于 3 FPS 的采样；`wifi=true`、最小内部堆保持 `25499` 字节。与故障时“编码 5–6、发送 0–1 FPS”形成同一路径的红/绿对照。
- 现可说本地视频链路**短时恢复**，不能说已经定位到唯一根因或远程链路/通话验收。CAM 单独断电重启未恢复持续视频，而主板 AP 重启加原网络角色恢复后明显改善；两项变化在时间上相邻，无法严格分离是主板 AP/STA 状态、热点连接/信道/扫描，还是无线转发连接积累。Mac 反向 SSH 隧道 CAM 监听在主板重启后消失，Mac 当前不在用户手边，远程网页、音频、运动仍未验收。后续若复发，先保留原拓扑并同步记录 AP/STA 事件、信道、关联设备与 CAM RSSI/发送指标，再做单变量复现，不直接重刷两板。

## 2026-09-26 Windows 双网与私网中继续测

- Windows 当前通过 iPhone USB 网卡 `172.20.10.10/28` 上网、Wi-Fi `192.168.4.3/24` 连接小车，默认路由优先走有线热点；未切换网络。经解除本机网络沙箱限制的只读测试，主板首页 HTTP 302 约 9 ms、CAM 根路径预期 HTTP 404 约 99 ms。COM3/CAM 与 COM6/主板的 CH340 设备均枚举为 OK；本阶段未打开串口，避免触发主板重启。
- 现有服务器公钥 SSH 正常；生产中继源码与本机测试版 SHA-256 一致。远程中继本地 `npm test` 7/7 PASS；用 `bootstrap_tailnet.sh` 在云机生成两枚独立随机令牌并保存在仅 root/carerover 可读的 `/etc/carerover/remote-hub.env`，无令牌打印或入库。安装既有 systemd 单元后服务 active，`127.0.0.1:8088/health` 显示 `audioPaired=false`、`controlDevice=false`、`videoFresh=false`（设备未接入时的预期状态）。
- Windows 通过已有公钥建立 SSH 反向隧道，服务器仅在 `127.0.0.1:18081/18082` 监听主板/CAM；服务器端 GET 分别返回主板 HTTP 302、CAM HTTP 404。`8088/18081/18082` 均仅绑定服务器回环地址，没有开放公网通话网页、视频或控制端口。隧道依赖本次 Windows SSH 进程存活，尚未做自动重连及长时间质量验收。
- Tailscale Serve 初次启用提示需管理者允许；用户点首次授权链接后显示 node 404。已改为请用户到 tailnet 管理后台 DNS 页面核对同一 tailnet 并启用 MagicDNS/HTTPS Certificates，**不要启用 Funnel**。因此 HTTPS/WSS 家长端尚不可访问，通话实测 NOT RUN。
- 接线经用户再次澄清：INMP441 SCK→GPIO1、WS→GPIO2 **原本已经接好**，功放 MAX98357 BCLK→GPIO39、LRC→GPIO42；不是把麦克风迁至 GPIO40/41，也不能让两模块共用时钟 GPIO。其余为麦克风 SD→GPIO16、功放 DIN→GPIO21、SD→GPIO38。独立草图改为 I²S0 RX（1/2/16）和 I²S1 TX（39/42/21），正在复编译；**未烧录、未做声音实测**。GPIO39/42 若启用外部 JTAG 需另作分配。
- 本阶段不声称远程通话完成。主板现行固件仍没有 I²S 音频任务或设备到云的音频连接；Tailscale 私网地址不能由未加入 tailnet 的 ESP32 直接访问，下一步需确定车旁 Windows 是否长期充当音频/视频网关，或另建可独立联网的设备路径。

## 2026-09-26 迁移到用户当前 tailnet 与 HTTPS 首验

- 用户选择把云服务器从原 Mac 所在的 `eddyzheng97@gmail.com` tailnet 迁到当前管理后台使用的 `new20070610@gmail.com` tailnet。服务器执行 Tailscale logout/relogin 后，`tailscale status --json` 核对到账号 `new20070610@gmail.com` 和后缀 `tail86bfa5.ts.net`，随后设备名设为 `carerover-relay`。旧 Mac 的子网路由不再属于同一个 tailnet；现阶段 Windows 双网反向 SSH 隧道仍可独立使用。
- 仅替换服务环境文件中的 `AUDIO_PROXY_PUBLIC_ORIGIN` 为 `https://carerover-relay.tail86bfa5.ts.net`，原随机家长/设备令牌不变；`carerover-remote-hub.service` 重启后 active。Tailscale Serve 只在 tailnet 内将 HTTPS 443 反代到服务器 `127.0.0.1:8088`，未启用 Funnel，也未开放公网音视频端口。
- 初次 HTTPS 握手卡在证书申请。tailscaled 日志显示 ACME 域名经服务器原 `100.96.0.2/3` DNS 查询超时；已在**云服务器**设置 `tailscale set --accept-dns=false`，并临时将 `eth0` systemd-resolved 的 DNS 指向测试可达的 `223.5.5.5`、`1.1.1.1`。证书随后成功签发。该 `resolvectl dns eth0` 是运行时设置，重启后需要持久化或重验；Tailscale 关闭 accept-dns 后，服务器自身不会解析 MagicDNS 名称，所以本机健康测试用 `curl --resolve` 保留正确 SNI/证书校验。
- 使用证书校验的私有 HTTPS 健康端点返回预期 JSON：`audioPaired=false`、`controlDevice=false`、`videoFresh=false`。这只证明服务器网页入口与本机服务，不代表家长端设备可达、车端连接或远程通话/视频/控制验收。下一步由登录同一 tailnet 的家长设备打开健康页；随后实现/测试 Windows 网关与车端音频。
- 独立音频草图按现场**已经接好**的麦克风 GPIO1/2/16、功放 GPIO39/42/21 改为 I²S0 RX + I²S1 TX，并完成 ESP32-S3 / 16 MB / OPI PSRAM 本地编译：程序 1,020,447 字节（32%）、全局变量 46,608 字节（14%）。尚未烧录或实测；刷写现有主板前须保存可恢复的当前 Flash 备份，实测结果不得以编译代替。

## 2026-09-26 音频实物首测及整车恢复

- 经用户确认四轮架空、舵机 5 V 断开，先读出主板 16 MB Flash 并计算 SHA-256（见 `audio-lab/docs/LOCAL_TEST_RECORD.md`），之后仅暂写应用分区进行独立音频实验。麦克风稳定采样约 50 帧/秒；功放与 8 Ω 扬声器能清楚播放 440 Hz 单音。实时本地自听因扬声器声反馈到麦克风而啸叫，不能作为远程全双工已通过的证据。
- 改为功放静音录音 3 秒到 PSRAM，再单独播放 3 秒：150 帧录满、约 50 帧/秒播放、I²S 写入错误 0；用户确认能听到基本清楚人声并恢复安静。首版远程通话应采用按住说话的收发互斥半双工策略；在线音频链路及与整车功能并发均未验收。
- 现场原 16 MB Flash 已全量写回主板并由 esptool 验证数据哈希，主板重启后串口显示原固件 `4e480f5e21e63c05-s5-follow`、IDLE、`ap=true`、CAM 包持续增长、MPU6050 有效。iPad 可见 AP 且能打开网页，但视频又出现历史卡顿/断连；Windows 当前扫描不到小车 SSID。正在用 iPad 单客户端直接请求 CAM `/stream` 分离问题路径。**不能把固件恢复等同于网页视频性能验收。**

## 2026-09-27 直连视频与 AP 可见性复测

- iPad 关闭控制台、直连 `http://192.168.4.2/stream` 后仍报告卡顿。CAM 串口此前在直连请求期间出现 JPEG 编码约 5.4–5.9 FPS、实际发送约 0.9–1.5 FPS；手势和人脸任务仍运行，丢帧/采集故障计数为 0。此时瓶颈发生在 CAM 编码之后的 HTTP/Wi-Fi 发送路径，不能归因于远程中继或网页人物框绘制。
- Windows 经 iPhone USB 网卡继续上网，Wi-Fi 网卡状态 `Disconnected`；只读 `netsh wlan show networks mode=bssid` 未发现 `CareRover-EE68`，而 iPad 能加入。Windows USB 仍枚举 CAM COM3、主板 COM6 为 OK。AP 频道被热点 STA 拉动、扫描兼容性或 AP 状态问题均待验证，不能仅凭扫描结果定论。
- 再次请 iPad 保持直连画面时，CAM 串口 15 秒连续统计 `gesture_fps≈6`、`face_fps≈3`，却有 `jpeg_fps=0`、`stream_fps=0`、`jpeg_total=107` 不变、`wifi=true`。已询问屏幕是连续变化、停在一帧还是空白；在用户确认前不能把浏览器显示的旧图算作实时流。
- 暂未更改主板/CAM 固件或 Wi-Fi 配置，也未切换 Windows Internet。下一步先确定 iPad 的实时画面状态，再保持原有 iPhone 热点 + iPad 小车 AP 拓扑，仅重启主板做 A/B：若 CAM 发送 FPS 恢复，重点检查主板 AP/STA、信道、客户端与 HTTP 连接状态。远程通话、视频与运动仍未实物验收。
- 用户随后确认直连页一直停顿。保持原拓扑只按主板 RST 后，iPad 反馈“稍好一些，但还是有些卡”，没有恢复到此前稳定约 5.5 FPS 的绿色基线；Windows 仍未扫描到 CareRover SSID。不能将单次 RST 视作修复。
- 为准备 AP-only 隔离测试，仅在私有审查快照的 `wireless_runtime.cpp` 增加 `CAREROVER_DIAG_AP_ONLY` 条件编译，未触碰 `C:\CareRover`，未烧录。首次尝试以 Arduino-ESP32 3.3.10、但误用 SparkFun 1.1.1 和旧 `build_version.h` 标识直接编译，所得应用 SHA-256 `FEB886C160B93B52B99C0AD5E485FE6CF6139690C943743C5EFC5A71D0FB24A1`，**禁止烧录该首次产物**。已烧录并恢复的主板版本为 `4e480f5e21e63c05-s5-follow`，归档应用约 1.22 MB；其后的正确依赖重建见下节。

## 2026-09-27 用户请求暂停：明确恢复点

- **暂停时未烧录本轮 AP-only 固件。** 主板仍是恢复过的原应用 `4e480f5e21e63c05-s5-follow`；CAM 本轮未写入。不要把私有快照中的临时修改说成现板代码。Windows 仍通过 iPhone USB 网卡上网；本轮没有切换它的互联网连接。
- 重新核对了主板原始 16 MB 备份：`main-web/build/backups/main_pre_audio_2026-09-26_full16mb.bin`，长度 16,777,216 B，SHA-256 `D739B6EC3139761E2E435185F4BA942F93564BCA982C13A0BC3717764AC678DA`。从备份偏移 `0x10000` 读取 1,224,032 B 的 SHA-256 为 `B9E1AF34520135C77DE65BCBA50CD035F1AD56B51FC18DBCA4B5C53DFA369AC7`，与 `C:\CareRover\build\stage5-follow-4e480f5e21e63c05-device\binaries\main_wireless.ino.bin` 完全一致；该归档应用可用于只恢复应用分区。
- 进一步核对当前 `C:\CareRover` 源码经项目 `content_id` 生成的 DEMO_BALANCED 版本确为 `4e480f5e21e63c05`。私有快照与其主板源码只有 `wireless_runtime.cpp`（本轮诊断条件分支）和 `build_version.h`（本轮诊断标识）两文件不同。首次诊断包意外使用 SparkFun 1.1.1 且旧版本标识，**不得烧录**。
- 最终候选诊断包用 Arduino-ESP32 3.3.10、正确的 SparkFun 1.1.2、相同 16 MB/OPI PSRAM FQBN 和 `compiler.cpp.extra_flags=-DCAREROVER_DIAG_AP_ONLY=1` 构建成功：`main-web/build/ap_only_diag_20260927_v2/main_wireless.ino.bin`，1,099,296 B，SHA-256 `EBCD2A4DBCE5821A6CD6A3BC32A1BF5FCFC69B97E6FB4FB6778111369E44E461`，内含 `wifi_diagnostic` 与 `4e480f5e21e63c05-s5-follow-apdiag` 标记。体积减少主要因 AP-only 构建让未引用的 Server酱 HTTPS 代码被剔除；这只是编译成功，**未做设备验收**。
- 用户在候选包编译完成后要求“请先暂停并记录”。当时已发问确认舵机与功放两路 5 V 是否都断开，尚未收到明确“两路均已断开”的答复。恢复时先核对供电、四轮架空和网页 IDLE，再决定是否只写主板应用分区做 AP-only A/B。该测试会暂时暂停微信通知；仅测 iPad 直连 CAM 与控制台视频/串口 `jpeg_fps`、`stream_fps`、Windows 能否发现 CareRover AP，不执行运动。测试完用上述归档原应用恢复，确认固件版本、网页与推送拓扑，再继续远程网关及半双工通话。
- 远程服务端私有 HTTPS 健康入口已建立，但车端控制/视频/音频尚未接入；家长设备尚未安装/登录当前 `new20070610@gmail.com` tailnet。远程通话尚不可使用，不能以本机测试或音频离线回放替代实物验收。

## 2026-09-27 恢复调试：AP-only 短时 A/B

- 现场再次确认主板/CAM USB 稳定、四轮架空、舵机及功放独立 5 V 断开；Windows 通过 iPhone USB 网卡上网，Wi-Fi 连 CareRover。服务器 `/health` 在测试前仍为 `audioPaired=false`、`controlDevice=false`、`videoFresh=false`，尚未打通车端。
- 原 AP+STA 应用下，用户关闭全部控制台标签后主板首页 HTTP 302 可恢复至约 0.15–0.6 s；单开一个 iPad 页面时 Windows 对主板 HTTP 多次 3 s 超时，CAM 根路径大多 0.14–2.06 s、偶发超时。期间 WLAN 仍关联，RSSI 约 -32 dBm，主板 ping 0/3 丢包但延迟 2–166 ms，CAM ping 2/3 丢包且成功包约 740 ms。用户页面显示重连中、无视频；这不能仅用弱信号或浏览器绘制解释。
- 核对原 16 MB 备份 SHA-256 仍为 `D739B6EC3139761E2E435185F4BA942F93564BCA982C13A0BC3717764AC678DA`；`COM6`/`COM3` 枚举 OK。使用 esptool 5.1.0 **只在主板 app0 地址 0x10000** 写入正确依赖构建的 AP-only 诊断应用（1,099,296 B，SHA-256 `EBCD2A4DBCE5821A6CD6A3BC32A1BF5FCFC69B97E6FB4FB6778111369E44E461`），`Hash of data verified`。CAM/NVS/FFat 均未写入；诊断期间微信推送暂停。烧录自动重启后 Windows 重新连 CareRover，iPhone USB 默认路由仍为 metric 25，Wi-Fi metric 50。
- AP-only 空闲状态下主板 5/5 次首页 HTTP 302 均约 0.06–0.07 s，CAM 5/5 次根路径预期 HTTP 404 为约 0.01–0.11 s。用户只开一个 iPad 页面后报告页面明显打开更快、视频稍流畅，人物/手势及心率血氧数据可见；此为 AP-only 实物正向结果，但视频帧率尚未量化。
- 同一单页打开期间，Windows 对主板首页 6/6 次 2 s 超时，而 CAM 根路径 6/6 次约 0.01–0.10 s。主板 HTTP 配置 `max_open_sockets=7`、`lru_purge_enable=false`，且静态资源无 `Connection: close`，因此第二客户端可能耗尽连接位；须进一步做受控连接测试，不能只据超时断言唯一根因。AP+STA 无线并发劣化与主板多客户端 HTTP 容量是两项独立待解决问题。
- **当前现场主板运行临时 AP-only 诊断应用**，不是原 `4e480f5e21e63c05-s5-follow`，CAM 未改。下一阶段若继续远程网关，应明确选择短时保留 AP-only（微信暂停）或恢复原应用；原应用可从 `C:\CareRover\build\stage5-follow-4e480f5e21e63c05-device\binaries\main_wireless.ino.bin` 只写 app0 恢复。远程通话尚未实物验收。

## 2026-09-27 Windows 网关视频/控制首验

- 新增 `remote-hub/gateway`，通过 Windows 已存在的 CareRover Wi-Fi 访问主板 `/ws` 与 CAM `/stream`，通过 iPhone USB Internet 上的**本机回环 SSH 转发**访问服务器 `/device/ws`、`/device/frame`；并实现音频 `/audio` 的可选双向转发（当前主板尚无整车音频端点，默认关闭）。设备令牌由 SSH 临时读入进程内存，不打印或入库。MJPEG 解析、控制/视频/音频模拟端到端测试 2/2 PASS；这是软件测试。
- 用户关闭 iPad 页面使网关独占 CAM 视频源；真实网关启动后报 `boardUp=true`、`relayUp=true`、`cameraUp=true`，约 20 秒 `boardMessages=296`、`framesUploaded=65`、`framesDropped=0`。云服务器实际 `/health` 从全 false 转为 `controlDevice=true`、`videoFresh=true`，几分钟后复查仍为 true；`audioPaired=false`，因此尚不能通话。未发任何运动指令，舵机/功放 5 V 仍断开。电脑默认互联网路由始终是 iPhone USB。
- `david` Windows 已加入与服务器相同的 Tailscale tailnet；本机 `curl` 对 `https://carerover-relay.tail86bfa5.ts.net/health` 证书校验通过，HTTP 200 且返回上述真实状态。用户浏览器报告 `unexpectedly closed the connection`，正在区分浏览器代理/设备与私有 HTTPS 服务问题，尚未完成家长网页现场验收。
- 主板 `Connection: close` 静态资源修正仅在本地诊断源码，首次中文路径编译到链接阶段失败（输出路径 Unicode 被工具链损坏），已改用授权目录的临时 ASCII `R:` 映射重新编译；**此修正尚未烧录**。当前板上仍是 AP-only 初版诊断应用。

## 2026-09-27 家长网页与双向音频继续开发

- Windows 系统代理 `127.0.0.1:7897` 与 tailnet 内 `100.x` 私有 HTTPS 地址不兼容：经代理的 `curl` TLS 握手失败，`--noproxy '*'` 直连返回 HTTP 200。用独立 `--no-proxy-server` Chrome 配置打开私有站点后，用户已完成登录，并现场确认远程页面显示视频、人物框、手势、心率血氧及 IMU 实时更新。未改变 Windows 的 iPhone USB 上网和 CareRover Wi-Fi 路由，也未启用公网 Funnel。
- `main-web` 新增编译开关 `CAREROVER_AUDIO_GATEWAY`，在主板 HTTP 服务器挂载 `/audio` WebSocket；I²S0 接现有 INMP441 GPIO1/2/16，I²S1 接 MAX98357 GPIO39/42/21，GPIO38 仅在有下行音频帧时使能功放。帧沿用已测试的 16 kHz/20 ms PCM16 协议，三帧有界队列，播放时抑制麦克风上行以避免独立自听测试曾出现的啸叫。此代码仍在编译验证，**尚未烧录，更未完成真实远程通话验收**。
- Windows 网关启动脚本增加显式 `-EnableAudio` 开关，缺省仍只转发已工作的控制/视频。网关软件测试 2/2 PASS；需等主板音频版编译、实物烧录与 `/audio` 上行验收后，才能重启网关启用音频。

## 2026-09-27 音频实测、回声修复与剩余验收

- 主板 audio1 应用只写 app0 且哈希校验通过；其 `/audio` 在功放断电时 6 秒收到 301 帧，约 50 帧/秒。Windows 网关开启音频后同时保持主板控制、CAM 视频、音频两路连接；服务器 `audioPaired=true` 时家长与车端 PCM 计数均增长。用户首轮听感确认双向声音可达，但家长说话时车端回声多且刺耳，不予通过。
- 更新三层回声处理：主板下行衰减 `/8`、限制峰值并避免逐包切换功放，家长下行后 700 ms 抑制车端麦克风；Windows 网关同样抑制这段车端上行；家长网页按住说话及松开后的短暂时间不播放车端音频，收听通道加增益/限幅以解决原先电脑端音量低。网关 2/2、云中继 7/7 测试 PASS。云端旧 `client.js` 已保留独立备份，新版哈希 `FAB3D1BBB8FF240DB8C676D0186BC0CBC9433B8867BF4646054BF5FDFC42439E`，未重启服务。
- 在用户确认两路外设 5 V 断电、车轮架空后，读回 audio1 实机应用并核对 SHA-256 `F58ED43BC91FD773B3FBC9082FBA5AD3C9923DCA7D5E676B9C67EA7DD3B6CF77`；audio2 应用 SHA-256 `1A88EDE9CB47C9EFF111902768EF35CC61001646579E9C68316D40D736BDA7A0`，只写主板 app0，Hash verified。Windows 重连小车 AP，网关重启加载新版代码后，服务器视频/遥测/音频三链路重新在线。
- 用户刷新通话页后确认小车→电脑可听；功放上电空闲安静无发热；随后交替短句测试用户答复“两边都可以”。服务器 `audioPaired=true/controlDevice=true/videoFresh=true`，家长/车端帧计数增长，网关视频无明显上传丢帧。仍待用户明确回声改善后的具体听感，以及用**另一台独立联网家长设备**登录同一 tailnet 做跨设备验收。当前临时 AP-only 固件暂停 Server酱微信通知，舵机未上电、远程运动未验收。
- 用户指出扬声器移远离电脑后回声减轻，支持同处一室时电脑麦克风再次拾取车端扬声器的声学耦合。后续用耳机或在不同房间演示；不能仅凭软件门控保证共处空间完全无回声。用户目前没有可独立上网的第二台家长设备，故异地验收暂不可执行；不要把本机经云中继的双向短句测试称为异地完成。云端通话 HTML 旧版也已备份并更新提示，浏览器刷新生效。
