# Four-corner tabletop edge guard (front-right staged firmware on mainboard)

## Confirmed wiring and polarity

The intended installation uses mainboard 3V3 and common GND. YL-62 OUT is LOW
when the grey-white tabletop is detected and HIGH when it is not. The user
confirmed the following planned OUT mapping on 2026-09-28, but then clarified
that **only front-right GPIO41 is physically connected now**:

| Position | Mainboard GPIO | Current wiring | HIGH blocks in MANUAL |
| --- | ---: | --- | --- |
| Front-left | 40 | Not installed yet | left (`vy < 0`), both rotations (`wz != 0`) |
| Front-right | 41 | Connected | forward (`vx > 0`), both rotations |
| Rear-right | 47 | Not installed yet | right (`vy > 0`), both rotations |
| Rear-left | 48 | Not installed yet | backward (`vx < 0`), both rotations |

Other components of a diagonal command remain permitted. The filter is applied
to the final safety snapshot on every 5 ms safety loop, so an already-received
manual command is blocked as soon as the corresponding input becomes HIGH.
HIGH blocks immediately. LOW must remain stable for 30 ms before that corner
is released. Before the first complete sample, every **installed** corner is treated
as exposed. Input pull-ups mean an unpowered/disconnected sensor normally
reads unsafe rather than falsely clear. Neither the emergency stop nor the
existing front ultrasonic and tilt protections were removed.

`cliff_config.h` currently sets `CliffInstalledMask = CliffFrontRight` (`0x02`).
Uninstalled pins are neither configured nor sampled; they cannot falsely block
motion. The four-corner algorithm is implemented and host-tested, but the
remaining directions are **not protected** until their modules are wired,
verified, and the installed mask is changed to `0x0f` in a new build.

The output and browser telemetry now include `cliff.edge_mask` (bit 0 FL,
bit 1 FR, bit 2 RR, bit 3 RL), `cliff.installed_mask`, `cliff.sampled`, and four named booleans. Serial
`wireless_status` includes `cliff_mask` and `cliff_installed_mask`. This is diagnostics, not an indicator
that optical edge detection has been physically validated.

## Scope and acceptance

This implementation intentionally gates **MANUAL only**, as requested. Person
follow and gesture-control motion are not made table-safe by this change. Do
not run those modes on an elevated table. The YL-62 is reflective, so the
grey-white surface, mounting height, ambient light, and shadows require
real-world calibration; four digital outputs alone cannot guarantee prevention
of a fall. GPIO48 may also be connected to a board RGB LED data input; check
the actual mainboard schematic and make sure no code drives GPIO48 as an output.

Host tests passed on 2026-09-28: new `cliff_test.cpp`, eight existing C++
firmware suites, and 24 Node web tests. ESP32-S3 Arduino board compile passed
with the current AP-only/audio-gateway flags. After a full 16 MB backup and
port/MAC verification, only the mainboard app0 was flashed and read-back
verification passed; CAM, FFat, NVS and partitions were not written. The new
firmware booted IDLE with `cliff_installed_mask=2`. COM6 observed the front-right
bit alternate between `0` and `2`, then settle at `0` for eight seconds. Whether
those alternations were caused by the user moving the test surface or by a
threshold set at the edge remains to be confirmed. No wheel-motion test has
been performed. Keep the wheels raised and servo 5 V disconnected until the
front-right sensor's deliberate LOW→HIGH→LOW transition is verified. When the
other three sensors are installed, verify their individual bits before changing
the mask to `0x0f` and bench-test both forbidden and allowed manual axes.

## Later update: front-right plus rear-left

The user subsequently connected rear-left OUT to GPIO48. The current flashed
app uses installed mask `0x0a` (front-right GPIO41 and rear-left GPIO48); the
front-left/right-rear corner bits remain disabled. Both sensors are filtered
in `MANUAL` and `WATCH_CONTROL`, not autonomous FOLLOW or GESTURE motion.
The mainboard reports both bits. The initial moving observation was ambiguous,
so each position was later held steady: only rear-left suspended gave mask 8
for 10 seconds, only front-right suspended gave mask 2 for about 9 seconds,
and both aimed at the grey-white desktop gave mask 0 for about 8 seconds.
This establishes the two installed input polarities in the current mounting
and light conditions, but not safety for the two uninstalled corners or a
moving rover; recheck after final mechanical mounting.
The mainboard backup, build, flash and serial evidence are in
`windows-progress.md` under the watch-control update.
