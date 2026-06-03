#pragma once

// Pyramid v1.3 — G.711 µ-law encode (pure, host-testable).
//
// Halves the ASR upload: 16-bit PCM → 8-bit µ-law before POSTing to Deepgram
// (`encoding=mulaw`), which fixes the `SLOW_UPLOAD` timeouts on the ESP32's
// limited TLS write throughput. Standard Sun/G.711 µ-law. The device encodes
// the capture buffer in place (see asrTranscribe in main.cpp).

#include <cstdint>

namespace pyramid {

// Encode one 16-bit signed PCM sample to an 8-bit µ-law byte (G.711).
inline std::uint8_t ulawEncode(std::int16_t sample) {
  constexpr int kBias = 0x84;
  constexpr int kClip = 32635;
  int sign = 0;
  int s = sample;
  if (s < 0) {
    s = -s;
    sign = 0x80;
  }
  if (s > kClip) s = kClip;
  s += kBias;

  int exponent = 7;
  int mask = 0x4000;
  while ((s & mask) == 0 && exponent > 0) {
    --exponent;
    mask >>= 1;
  }
  const int mantissa = (s >> (exponent + 3)) & 0x0F;
  return static_cast<std::uint8_t>(~(sign | (exponent << 4) | mantissa) & 0xFF);
}

}  // namespace pyramid
