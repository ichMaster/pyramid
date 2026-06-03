#pragma once

// Pyramid v1.4 — pause-based end-of-utterance (pure, host-testable).
//
// So the user need not time the button: while capturing push-to-talk audio we
// feed each chunk's peak level here, and the Endpointer reports when the
// utterance has ended — letting recordWhileHeld stop on a natural pause instead
// of only on button release. Button release still ends capture immediately
// (handled in main.cpp); this adds the hands-free "speak, then pause" path.
//
// Pure (no Arduino/M5 deps) so the decision logic is unit-tested on the host.

#include <cstdint>

namespace pyramid {

// Streaming end-of-utterance detector. Feed one captured chunk at a time (its
// peak amplitude + duration in ms). Ends when EITHER:
//   - speech was seen and trailing silence has lasted >= hangoverMs (a natural
//     end-of-sentence pause), OR
//   - total listening reached patienceMs (the recog_patience hard cap).
// Quiet-only input never trips the pause path (speechSeen stays false) — it can
// only end at the patience cap, where the v1.3 noise gate then rejects it. A
// short gap mid-utterance (< hangoverMs) does not cut: any speech chunk resets
// the trailing-silence counter.
struct Endpointer {
  int silencePeak;       // chunk peak < this counts as silence
  uint32_t hangoverMs;   // trailing silence after speech that ends the utterance
  uint32_t patienceMs;   // hard cap on total listening (recog_patience)

  bool speechSeen = false;
  uint32_t silenceMs = 0;  // accumulated trailing silence since the last speech
  uint32_t totalMs = 0;    // total listened so far

  // Explicit ctor so {silencePeak, hangover, patience} init works under the
  // firmware's gnu++11 too (a struct with default member initializers is not an
  // aggregate before C++14).
  Endpointer(int silencePeakArg, uint32_t hangoverMsArg, uint32_t patienceMsArg)
      : silencePeak(silencePeakArg),
        hangoverMs(hangoverMsArg),
        patienceMs(patienceMsArg) {}

  // Feed one chunk; returns true once the utterance is considered ended.
  bool feed(int peak, uint32_t chunkMs) {
    totalMs += chunkMs;
    if (peak >= silencePeak) {
      speechSeen = true;
      silenceMs = 0;
    } else if (speechSeen) {
      silenceMs += chunkMs;
    }
    if (speechSeen && silenceMs >= hangoverMs) return true;  // natural pause
    if (totalMs >= patienceMs) return true;                  // patience cap
    return false;
  }
};

}  // namespace pyramid
