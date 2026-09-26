#pragma once
#include "front_guard.h"
namespace carerover {
struct FrontInstallation {
  int trig=-1,echo=-1;
  FrontConfig control;
};
// Excludes UART17/18, buses4..9, motors10..13, USB19/20, strapping,
// onboard LED48 and flash/OPI PSRAM pins. Board exposure must still be verified.
inline bool frontPinAllowed(int p) { return p==1||p==2||p==14||p==15||p==16||p==21||(p>=38&&p<=42); }
inline bool frontPinsReady(const FrontInstallation& i) { return i.trig!=i.echo&&frontPinAllowed(i.trig)&&frontPinAllowed(i.echo); }
}
#if __has_include("front_config.local.h")
#include "front_config.local.h"
#else
inline carerover::FrontInstallation installedFront() { return {}; }
#endif
