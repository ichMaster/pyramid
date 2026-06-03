#pragma once

// Pyramid v1.1 — audio buffer math (pure, host-testable).
//
// The actual I2S capture/playback (Echo Base mic/speaker via M5Unified) lives
// in main.cpp and needs the board; the framing, sizing, and level/clipping
// math here is Arduino-free so it is covered by `pio test -e native`
// (see test/test_audio/).

#include <cstddef>
#include <cstdint>

namespace pyramid {

// Number of PCM samples for `ms` milliseconds at `rate` Hz (mono).
constexpr std::size_t samplesForMs(std::uint32_t ms, std::uint32_t rate) {
  return (static_cast<std::uint64_t>(ms) * rate) / 1000u;
}

// Cap a requested sample count to a maximum (the fixed record-duration bound).
constexpr std::size_t capSamples(std::size_t want, std::size_t maxSamples) {
  return want < maxSamples ? want : maxSamples;
}

// Level/clipping summary of a PCM16 mono span.
struct PcmStats {
  std::int16_t peak = 0;   // max |sample|
  std::size_t clipped = 0; // samples at/above the clip threshold (|s| >= thr)
};

// Analyze `n` PCM16 samples: peak magnitude and clipped-sample count. A sample
// counts as clipped when |sample| >= `clipThreshold` (e.g. ~32700 of 32767).
inline PcmStats analyzePcm(const std::int16_t* data, std::size_t n,
                           std::int16_t clipThreshold) {
  PcmStats s;
  for (std::size_t i = 0; i < n; ++i) {
    int v = data[i];
    if (v < 0) v = -v;            // |sample| (note: -32768 -> 32768, still fine)
    if (v > s.peak) s.peak = static_cast<std::int16_t>(v > 32767 ? 32767 : v);
    if (v >= clipThreshold) ++s.clipped;
  }
  return s;
}

}  // namespace pyramid
