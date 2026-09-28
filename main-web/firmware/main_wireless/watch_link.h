#pragma once

#include "watch_link_state.h"

// Starts a low-priority read-only UDP listener on the CareRover AP. The task
// never controls motors and does not use the CAM UART or HTTP video socket.
bool watchLinkBegin();
carerover::WatchLinkSnapshot watchLinkSnapshot();
