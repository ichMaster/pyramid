// Host unit test for the pure push-to-talk latency breakdown (timing.h).
// Run via `pio test -e native`.

#include <unity.h>
#include <cstdint>

#include "timing.h"

using pyramid::computeLatency;
using pyramid::elapsed;
using pyramid::LatencyBreakdown;
using pyramid::VoiceStamps;

void setUp(void) {}
void tearDown(void) {}

// A normal turn: 1450 ms of speech, then asr+llm+tts = 2370 ms to first audio.
void test_breakdown(void) {
  VoiceStamps s;
  s.pressMs = 1000;
  s.recEndMs = 2450;  // spoke for 1450 ms
  s.asrMs = 480;
  s.llmMs = 1310;
  s.ttsMs = 580;
  s.speakMs = 4820;  // press -> speak = 3820 ms

  const LatencyBreakdown b = computeLatency(s);
  TEST_ASSERT_EQUAL_UINT32(3820, b.total);
  TEST_ASSERT_EQUAL_UINT32(1450, b.speech);
  TEST_ASSERT_EQUAL_UINT32(480, b.asr);
  TEST_ASSERT_EQUAL_UINT32(1310, b.llm);
  TEST_ASSERT_EQUAL_UINT32(580, b.tts);
  // other = 3820 - 1450 - 480 - 1310 - 580 = 0
  TEST_ASSERT_EQUAL_UINT32(0, b.other);
  // reply (release -> speak) = total - speech
  TEST_ASSERT_EQUAL_UINT32(2370, b.total - b.speech);
}

// Realistic case where the stages don't sum exactly to the total: the slack
// (gate, status updates, mic<->speaker switch) lands in `other`, never negative.
void test_other_is_slack(void) {
  VoiceStamps s;
  s.pressMs = 0;
  s.recEndMs = 1000;
  s.asrMs = 300;
  s.llmMs = 700;
  s.ttsMs = 400;
  s.speakMs = 2500;  // total 2500; accounted 1000+300+700+400 = 2400
  const LatencyBreakdown b = computeLatency(s);
  TEST_ASSERT_EQUAL_UINT32(2500, b.total);
  TEST_ASSERT_EQUAL_UINT32(100, b.other);
}

// If measured stages overshoot the total (clock skew / overlap), other clamps
// to 0 rather than underflowing to a huge unsigned value.
void test_other_clamps(void) {
  VoiceStamps s;
  s.pressMs = 0;
  s.recEndMs = 100;
  s.asrMs = 50;
  s.llmMs = 50;
  s.ttsMs = 50;
  s.speakMs = 120;  // total 120 < accounted 250
  const LatencyBreakdown b = computeLatency(s);
  TEST_ASSERT_EQUAL_UINT32(120, b.total);
  TEST_ASSERT_EQUAL_UINT32(0, b.other);
}

// millis() wraps at ~2^32; unsigned subtraction keeps the deltas correct.
void test_wrap_safe(void) {
  const uint32_t base = 0xFFFFFF00u;  // close to rollover
  VoiceStamps s;
  s.pressMs = base;
  s.recEndMs = base + 200u;  // wraps past 0
  s.speakMs = base + 500u;   // wraps past 0
  const LatencyBreakdown b = computeLatency(s);
  TEST_ASSERT_EQUAL_UINT32(500, b.total);
  TEST_ASSERT_EQUAL_UINT32(200, b.speech);
  TEST_ASSERT_EQUAL_UINT32(300, elapsed(s.recEndMs, s.speakMs));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_breakdown);
  RUN_TEST(test_other_is_slack);
  RUN_TEST(test_other_clamps);
  RUN_TEST(test_wrap_safe);
  return UNITY_END();
}
