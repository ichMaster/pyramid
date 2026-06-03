#pragma once

// Pyramid v0.3 — capped exponential backoff.
//
// Pure helper for spacing retries (LLM call) and Wi-Fi reconnect attempts:
// delay = base * 2^attempt, clamped to cap. `attempt` is 0-based, so the first
// retry waits `base`. Deterministic and Arduino-free so it is host-testable
// (see ../test/test_backoff.cpp); any jitter is added by the caller on-device.

#include <cstdint>

namespace pyramid {

inline std::uint32_t backoffDelayMs(int attempt, std::uint32_t baseMs,
                                    std::uint32_t capMs) {
  if (attempt < 0) attempt = 0;
  std::uint32_t d = baseMs;
  for (int i = 0; i < attempt && d < capMs; ++i) {
    d <<= 1;
  }
  return d > capMs ? capMs : d;
}

}  // namespace pyramid
