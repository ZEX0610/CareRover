# Physical wheel calibration

Run `python tools/tracking.py calibration-build --profile config/board.local.json` from the project root. The tool stages this sketch with the current main-controller PWM/kinematics files; use the resulting complete sketch in `build/calibration/motion_calibration` with the verified board profile.

The original standalone bounded movement commands remain. New commands:

- `wheelspan fl 300` (also fr/rl/rr): per-wheel PWM span, DISARMED only.
- `confirm_calibration`: saves parameters and sets the NVS verified marker, DISARMED only. Execute only after physical neutral, polarity, independent stop and chassis-direction tests.

`save` clears the verified marker. Main motion is inhibited until valid parameters and verified=true are present. No default values are claimed as measured calibration. This sketch has no wireless control parser; the final main firmware does not expose these standalone movement commands over WebSocket.

Before upload, retain the complete current flash and NVS backup. First tests require raised wheels, an adequate separate 5V servo supply, and common ground. See the project tracking-development guide for the full staged procedure.
