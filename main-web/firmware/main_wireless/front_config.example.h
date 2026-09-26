#pragma once
// Copy to front_config.local.h AFTER verifying actual pin exposure and wiring.
// Leave verified flags false until physical measurements have been recorded.
inline carerover::FrontInstallation installedFront() {
  carerover::FrontInstallation i;
  i.trig=-1; i.echo=-1;
  i.control.enabled=true;
  i.control.verified=false;
  i.control.bypassVerified=false;
  // Fill measured stopCm < slowCm <= warnCm, slowCm < releaseCm <= 400.
  // Then fill lateralSpeed/forwardSpeed (0..0.25 normalized), settleMs,
  // marginMs, passMs, lateralTimeoutMs. Units are ms, not measured distance.
  return i;
}
