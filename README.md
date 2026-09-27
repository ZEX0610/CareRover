# CareRover — private code-review snapshot

This is a private, source-only review snapshot. Start with [REVIEW_BRIEF.md](REVIEW_BRIEF.md), then read the component progress records. The table below reflects the 2026-09-27 physical state; test builds and older handoff text are not a substitute for it.

| Directory | What it contains | Physical deployment status |
|---|---|---|
| `main-web/` | ESP32-S3 mainboard firmware, CAM firmware, onboard web UI, control/vision/health tests and Windows development records | Mainboard currently has temporary AP-only `audio2` test application; CAM keeps its 2026-09-25 balanced video build. See `main-web/docs/windows-progress.md` before building or flashing. |
| `remote-hub/` | Private HTTPS/Tailscale relay, Windows video/control/audio gateway and deployment notes | Same Windows computer has physically tested bidirectional short-sentence audio through the relay plus video, person box and telemetry. An independent remote parent device, remote motion and long-duration reliability are not yet accepted. |
| `watch-prototype/` | ESP32-S3 Zero wrist module design, firmware, tests, wiring plan | Local prototype only; not integrated with car firmware |
| `audio-lab/` | INMP441/MAX98357A isolated audio bench prototype, tests and wiring plan | Real mic capture, amplifier tone, and record-then-replay passed before whole-car audio integration. |

The tree omits complete Flash/NVS dumps, build products, credentials and caches. The current `audio2` application SHA-256, backup and program/verification steps are in `main-web/docs/windows-progress.md`; recreating a binary still requires private `wifi_secrets.h` and the documented Arduino-ESP32 toolchain. The currently flashed AP-only diagnostic build pauses the previous Server酱 WeChat notifications. Do not present it as the final full-featured deployment.

This snapshot does not grant an AI independent GitHub rights. A reviewer on another computer must authenticate with a GitHub account that can access this private repository and authorize its own Codex/GitHub connection for this repository.

