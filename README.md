# CareRover — private code-review snapshot

This is a curated, source-only snapshot prepared on 2026-09-26 for a new reviewer. It is **not** a claim that every component below is deployed. Start with [REVIEW_BRIEF.md](REVIEW_BRIEF.md), then read each component's README and progress record. The repository is intended to remain private.

| Directory | What it contains | Physical deployment status |
|---|---|---|
| `main-web/` | ESP32-S3 mainboard firmware, CAM firmware, onboard web UI, control/vision/health tests and Windows development records | Mainboard + CAM last physically updated and observed on 2026-09-25; see `main-web/docs/windows-progress.md` and `main-web/docs/VIDEO_PERF_2026-09-25.md` |
| `remote-hub/` | Independent Node.js relay/web/audio prototype and server deployment notes | Software/server tests only; remote video/control/two-way audio are **not** accepted end to end |
| `watch-prototype/` | ESP32-S3 Zero wrist module design, firmware, tests, wiring plan | Local prototype only; not integrated with car firmware |
| `audio-lab/` | INMP441/MAX98357A audio bench prototype, tests and wiring plan | Local software prototype; mic/amplifier/speaker not connected to the car |

The `main-web/` tree reflects the **current local working sources**, including as-yet-uncommitted changes from the original checkout. It deliberately omits the large historical 0915 duplicate bundle, complete Flash/NVS dumps, build products, credentials, and cache. The exact last-known programmed image hashes and evidence boundaries are described in [REVIEW_BRIEF.md](REVIEW_BRIEF.md). Do not infer byte-identical device images from the absence of private `wifi_secrets.h`, generated `build_version.h`, and binaries.

This snapshot does not grant an AI independent GitHub rights. A reviewer on another computer must authenticate with a GitHub account that can access this private repository and authorize its own Codex/GitHub connection for this repository.

