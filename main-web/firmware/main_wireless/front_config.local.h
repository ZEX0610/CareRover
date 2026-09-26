#pragma once
// Local HC-SR04 installation (git-ignored, device-only).
// Wiring: VCC 5V, GND common, Trig GPIO14, Echo GPIO15 (Echo via 1k/2k divider to 3.3V).
inline carerover::FrontInstallation installedFront() {
  carerover::FrontInstallation i;
  i.trig = 14;
  i.echo = 15;
  i.control.enabled = true;
  i.control.verified = true;
  i.control.bypassVerified = true;
  i.control.stopCm = 5;      // <=5cm: blocked -> fixed-right bypass
  i.control.slowCm = 10;     // <10cm: slow
  i.control.warnCm = 15;     // <15cm: warn
  i.control.releaseCm = 15;  // >=15cm: recovered
  i.control.lateralSpeed = 0.15;
  i.control.forwardSpeed = 0.15;
  i.control.settleMs = 210;
  i.control.marginMs = 400;
  i.control.passMs = 800;
  i.control.lateralTimeoutMs = 4000;
  return i;
}
