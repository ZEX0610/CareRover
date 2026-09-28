#include "audio_call_lease.h"
#include <cassert>
#include <cstdint>

int main() {
  carerover::AudioCallLease lease;
  assert(!lease.active(0));
  lease.set(true, 1000);
  assert(lease.active(1000));
  assert(lease.active(3999));
  assert(!lease.active(4000));
  lease.set(true, UINT32_MAX - 1000);
  assert(lease.active(0));
  assert(!lease.active(2000));
  lease.set(false, 2001);
  assert(!lease.active(2001));
  lease.set(true, 2002);
  lease.reset();
  assert(!lease.active(2002));
}
