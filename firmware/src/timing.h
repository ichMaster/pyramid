#pragma once

// Pyramid — push-to-talk latency tracking (pure, host-testable).
//
// Answers "how long from the button press until the device starts speaking?"
// The voice loop crosses several functions (record -> ASR -> LLM -> TTS ->
// playback), so main.cpp stamps a few millis() readings into VoiceStamps along
// the way and this header turns them into a breakdown. Kept pure so the math is
// unit-tested on the host (see test/test_timing); only the millis() reads and
// the serial print live in main.cpp.

#include <cstdint>

namespace pyramid {

// Timestamps/durations captured along one push-to-talk turn. pressMs, recEndMs
// and speakMs are absolute millis() readings (monotonic); asrMs/llmMs/ttsMs are
// per-stage durations measured locally where each stage runs (the small gaps
// between stages — gate, status updates, mic/speaker switch — land in `other`).
struct VoiceStamps {
  uint32_t pressMs = 0;   // button pressed (record start) — t0
  uint32_t recEndMs = 0;  // button released (record/speech end)
  uint32_t asrMs = 0;     // ASR (transcription) call duration
  uint32_t llmMs = 0;     // LLM request -> reply-complete duration
  uint32_t ttsMs = 0;     // TTS fetch duration
  uint32_t speakMs = 0;   // first spoken audio queued — the answer the user asked for
};

// Derived latency breakdown (all ms). total is the headline press->speak time.
struct LatencyBreakdown {
  uint32_t total;   // press -> speak (the full felt latency)
  uint32_t speech;  // press -> release (user holding the button / speaking)
  uint32_t asr;
  uint32_t llm;
  uint32_t tts;
  uint32_t other;   // total - speech - asr - llm - tts (gate/status/bus switch)
};

// Monotonic-clock difference, wrap-safe via unsigned arithmetic (millis()
// rolls over at ~49 days; this stays correct across the wrap).
inline uint32_t elapsed(uint32_t from, uint32_t to) { return to - from; }

inline LatencyBreakdown computeLatency(const VoiceStamps& s) {
  LatencyBreakdown b;
  b.total = elapsed(s.pressMs, s.speakMs);
  b.speech = elapsed(s.pressMs, s.recEndMs);
  b.asr = s.asrMs;
  b.llm = s.llmMs;
  b.tts = s.ttsMs;
  const uint32_t accounted = b.speech + b.asr + b.llm + b.tts;
  b.other = (b.total > accounted) ? (b.total - accounted) : 0;
  return b;
}

}  // namespace pyramid
