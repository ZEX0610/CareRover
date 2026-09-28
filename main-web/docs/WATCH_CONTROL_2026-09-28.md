# Watch control integration (2026-09-28)

The current wearable is an ESP32-S3 Zero with an MPU6050 only. Its firmware
(`CareRover_Watch_Prototype_2026-09-24`) sends `watch_v1` UDP posture packets
to the rover AP on port 45670 at about 20 Hz. No MAX30102 is connected to the
wearable; rover health readings still come from the rover-mounted sensor.

## Control contract

- A camera-detected, action-eligible `FIVE` gesture toggles `WATCH_CONTROL` on
  and off. It must pass the existing repeated-frame gesture latch; the hand
  must be lowered/released before a second `FIVE` is recognized. Other motion
  gestures cannot seize an active watch session. `DISLIKE`, webpage IDLE and
  emergency stop still stop it.
- Entry requires a fresh calibrated wearable packet and no active e-stop,
  fault, front-release hold or other controller owner. After entry the watch
  must send a **new neutral packet** before movement is armed. This avoids
  launching from a tilted pose already held while switching modes.
- Non-neutral posture commands are **continuous while held and refreshed**,
  not a fixed movement burst. Return to neutral to stop. Zero's B button
  re-centers the posture; release/neutral, five-finger exit, emergency stop,
  or stale/missing watch packets (350 ms) stop the motors. The chassis drive
  also has a 240 ms output lease. Wrist roll is rate-based, so rotation stops
  shortly after the roll stops; the MPU6050 cannot provide drift-free yaw.
- The front ultrasonic guard, chassis tilt fault and installed cliff-sensor
  filter also apply to watch output. The currently installed cliff sensors are
  front-right (GPIO41) and rear-left (GPIO48), HIGH meaning edge. Front-right
  blocks forward motion; rear-left blocks backward motion; either blocks both
  rotations. The uninstalled corners have **no coverage**. This is a
  demonstrator, not a guarantee against falling.
- The webpage displays `WATCH_CONTROL` in telemetry but cannot request the
  mode directly. The existing remote HTTPS call/video flow remains independent
  of motion control; local HTTP cannot request browser microphone permission.

## Verification boundary

The safety controller, gesture latch, native cliff tests and frontend protocol
tests pass offline. The mainboard app and local web image were flashed and
verified, and the post-flash UART showed IDLE, CAM packets with no bad frames,
and the expected two-sensor installed mask. Camera `FIVE` recognition, actual
watch link, infrared HIGH/LOW transitions, live browser video and two-way call
still require separate device acceptance. Keep the wheels raised and servo 5 V
disconnected for signal-path acceptance; do not infer real fall prevention from
that test.
