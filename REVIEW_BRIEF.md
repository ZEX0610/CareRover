# Independent review brief — 2026-09-26

## Read in this order

1. `README.md` (scope and deployment status).
2. `main-web/AGENTS.md`, `main-web/WINDOWS_START_HERE.md`, `main-web/docs/0915-demo-development.md`, then `main-web/docs/windows-progress.md` (chronological Windows/physical evidence).
3. `main-web/docs/BOARD_EXACT_SOURCE_2026-09-22.md`, `main-web/docs/VIDEO_PERF_2026-09-25.md`, `main-web/docs/WECHAT_APSTA_2026-09-25.md` (actual board provenance and later changes).
4. `main-web/firmware/main_wireless/`, `main-web/firmware/cam_tracking/`, `main-web/js/`, `main-web/tests/`, `main-web/tools/` (current local source and tests).
5. `remote-hub/README.md`, `remote-hub/deploy/REMOTE_SERVER_PROGRESS_2026-09-26.md`; then `audio-lab/README.md` and `watch-prototype/README.md` (independent future work).

## What is physically present vs proposed

- Car: ESP32-S3 mainboard; separate ESP32-S3 CAM; MAX30102, MPU6050, OLED, HC-SR04; four continuous-rotation MG90S servos with omni mini wheels. The onboard AP is `192.168.4.1`; CAM is `192.168.4.2`. The web UI offers video, gesture/person boxes, vitals, IMU, joystick and modes. Server酱 seated/vacant notifications over AP+STA had short live acceptance: one message of each, without observed video slowdown.
- Last recorded mainboard app update: 2026-09-25 Server酱/AP+STA, version `4e480f5e21e63c05-s5-follow`, app SHA-256 `B9E1AF34520135C77DE65BCBA50CD035F1AD56B51FC18DBCA4B5C53DFA369AC7`; the previously verified browser FFat SHA-256 was `CAEEA4A795CCC12B75B4095E84AF2CB8C6C0B478AB20546312A51473F1430E1B`. Last recorded CAM balanced video app SHA-256 `EBEA840C5E7DE742C7A86B5E4585F12C5C0CE6F3ABF8A7481EFA9B7EB9BDDB9B`.
- These hashes are **historical field evidence**, not a fresh 2026-09-26 readback. The complete programmed binaries and NVS are intentionally absent. `main-web/firmware/main_wireless/front_config.local.h` is included because its pin/threshold values are non-secret and part of actual car configuration; real Wi-Fi and notification credentials are not included. Use examples to configure a development board, never assume placeholders are real credentials.
- Remote hub: independent server prototype and Tailscale private demo path. Server and Mac joined one tailnet, but as of this snapshot server-to-robot HTTP still times out despite route approval. A temporary Mac IPv4-forwarding test alone did not establish connectivity. No claim of remote driving, remote browser video, or two-way call acceptance.
- Wrist watch and audio modules remain separate prototypes. Do not describe their functionality as installed or linked into the two car boards.

## Snapshot verification

- The 2026-09-26 curated copy has 228 staged source/document files. The copied main-web source files were hash-compared with the current local checkout; the copied watch and audio files match their prototype directories. Two remote-hub deployment Markdown files differ only because the public server IP was replaced with `<SERVER_PUBLIC_IP>`.
- In this snapshot, `main-web` JavaScript tests passed 24/24 and Python unit tests passed 37/37. The remote-hub source-equivalent relay tests passed 7/7 in its original local checkout. These are local software checks, not a new board flash or physical acceptance.
- Later Mac-side checking confirmed local access to the mainboard (HTTP 302) while using the car Wi-Fi; from the server, requests to both boards still timed out even after the subnet route was approved and Mac IPv4 forwarding was temporarily enabled. Remote relay reachability remains unresolved.

## Review focus

Please inspect architecture, source provenance, control/stop behavior, remote access/auth boundaries, transport latency and backpressure, video/AI scheduling, gesture command idempotence, health signal quality, tests vs actual hardware evidence, build reproducibility, and hardcoded or accidentally committed secrets. Distinguish a demonstrated bug from a hypothetical risk and cite exact file/line for each finding. Do not treat historical test logs or software tests as proof of current physical behavior.

## Source and privacy boundaries

- This is a **new single-snapshot repository**, not the original `NeedleAss/spider-webite-dev` history. The original branch had local uncommitted changes; this package captures their file contents without rewriting that checkout.
- Excluded: original duplicate handoff bundle, full Flash/NVS backups, binary firmware, Wi-Fi passwords, Server酱 send key, SSH keys, session tokens, `.env` files, user recordings and caches. `remote-hub` deployment notes replace the server's public IP with a placeholder.
- A private GitHub repository still exposes every committed file to authorized collaborators and any connected coding agent. Treat access grants as repository-wide unless your GitHub plan/policy says otherwise.
