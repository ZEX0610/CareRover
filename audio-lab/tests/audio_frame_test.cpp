#include "../firmware/audio_lab/audio_frame.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
  using namespace carerover_audio;
  static_assert(kPacketBytes == 648, "frame size changed");
  uint8_t frame[kPacketBytes]{};
  put_header(frame, 0x12345678);
  assert(valid_packet(frame, sizeof(frame)));
  assert(sequence_of(frame) == 0x12345678);
  assert(!valid_packet(frame, sizeof(frame) - 1));
  frame[2] = 2;
  assert(!valid_packet(frame, sizeof(frame)));
  assert(mic32_to_pcm16(0x12340000) == 0x1234);
  assert(mic32_to_pcm16(-65536) == -1);
  assert(pcm16_to_dac32(-32768) == INT32_MIN);
  std::puts("audio_frame_test PASS");
}
