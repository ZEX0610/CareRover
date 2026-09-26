#pragma once

#include <stddef.h>
#include <stdint.h>

// Standalone wire format: 20 ms, mono, signed PCM16 little-endian at 16 kHz.
// Header is deliberately tiny and independent of CareRover's JSON/control WS.
namespace carerover_audio {
constexpr uint32_t kSampleRate = 16000;
constexpr size_t kSamples = 320;
constexpr size_t kPayloadBytes = kSamples * sizeof(int16_t);
constexpr size_t kHeaderBytes = 8;
constexpr size_t kPacketBytes = kHeaderBytes + kPayloadBytes;

inline void put_header(uint8_t* out, uint32_t sequence) {
  out[0] = 'C'; out[1] = 'R'; out[2] = 1; out[3] = 1;
  out[4] = static_cast<uint8_t>(sequence);
  out[5] = static_cast<uint8_t>(sequence >> 8);
  out[6] = static_cast<uint8_t>(sequence >> 16);
  out[7] = static_cast<uint8_t>(sequence >> 24);
}

inline bool valid_packet(const uint8_t* data, size_t len) {
  return data != nullptr && len == kPacketBytes && data[0] == 'C' &&
         data[1] == 'R' && data[2] == 1 && data[3] == 1;
}

inline uint32_t sequence_of(const uint8_t* data) {
  return uint32_t(data[4]) | (uint32_t(data[5]) << 8) |
         (uint32_t(data[6]) << 16) | (uint32_t(data[7]) << 24);
}

inline int16_t mic32_to_pcm16(int32_t raw) {
  // INMP441 places 24 valid bits in a 32-bit Philips-I2S slot.
  return static_cast<int16_t>(raw >> 16);
}

inline int32_t pcm16_to_dac32(int16_t sample) {
  return static_cast<int32_t>(sample) * 65536;
}
}  // namespace carerover_audio
