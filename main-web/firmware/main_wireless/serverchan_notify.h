#pragma once
#include <cstdint>
#include "seat_notification_gate.h"

// The main CareRover AP, camera, WebSocket and motion tasks do not depend on
// this optional, best-effort outbound notification link.
bool serverchanConfigured();
void serverchanBegin();
void serverchanSeatEvent(carerover::SeatNotice notice, uint64_t nowMs);
