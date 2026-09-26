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
