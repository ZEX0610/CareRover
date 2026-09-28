#include "gesture_actions.h"
#include <cassert>
#include <iostream>
using namespace carerover;

int main() {
  GestureActionLatch call(4, 3, true);
  for (uint64_t ms : {100u, 200u, 300u})
    assert(call.update(true, "call", ms) == GestureAction::None);
  assert(call.update(true, "call", 400) == GestureAction::ToggleCall);
  assert(call.update(true, "call", 500) == GestureAction::None);
  // Confidence may briefly fall while the same CALL hand shape is still
  // visible. This must not count as disappearance and must not hang up.
  for (uint64_t ms : {550u, 575u, 590u, 595u})
    assert(call.update(false, "call", ms) == GestureAction::None);
  for (uint64_t ms : {596u, 597u, 598u, 599u})
    assert(call.update(true, "call", ms) == GestureAction::None);
  for (uint64_t ms : {600u, 700u, 800u})
    assert(call.update(false, "no_hand", ms) == GestureAction::None);
  for (uint64_t ms : {900u, 1000u, 1100u})
    assert(call.update(true, "CALL", ms) == GestureAction::None);
  assert(call.update(true, "CALL", 1200) == GestureAction::ToggleCall);

  GestureActionLatch fixed(4, 3, false);
  for (uint64_t ms : {100u, 200u, 300u})
    assert(fixed.update(true, "CALL", ms) == GestureAction::None);
  assert(fixed.update(true, "CALL", 400) == GestureAction::ToggleCall);
  for (uint64_t ms : {500u, 600u, 700u, 800u})
    assert(fixed.update(false, "CALL", ms) == GestureAction::None);
  for (uint64_t ms : {900u, 1000u, 1100u, 1200u})
    assert(fixed.update(true, "CALL", ms) == GestureAction::None);
  std::cout << "CALL gesture needs confirmation and release before re-trigger PASS\n";
}
