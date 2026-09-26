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
