// Host unit test for the pure v1.4 end-of-utterance detector (vad.h).
// Run via `pio test -e native`.

#include <unity.h>

#include "vad.h"

using pyramid::Endpointer;

void setUp(void) {}
void tearDown(void) {}

static Endpointer makeEp() {
  return Endpointer{/*silencePeak=*/500, /*hangoverMs=*/800, /*patienceMs=*/5000};
}

// Speech then a sustained pause ends the utterance after ~hangoverMs.
void test_pause_ends(void) {
  Endpointer ep = makeEp();
  // 300 ms of speech (3 x 100 ms chunks, peak above the silence floor).
  TEST_ASSERT_FALSE(ep.feed(2000, 100));
  TEST_ASSERT_FALSE(ep.feed(2000, 100));
  TEST_ASSERT_FALSE(ep.feed(2000, 100));
  // Silence accumulates; ends once it reaches the 800 ms hangover (8th chunk).
  for (int i = 0; i < 7; ++i) TEST_ASSERT_FALSE(ep.feed(100, 100));
  TEST_ASSERT_TRUE(ep.feed(100, 100));  // silenceMs == 800 -> end
}

// A short gap mid-utterance (< hangover) must NOT cut: speech resets silence.
void test_no_cut_on_short_gap(void) {
  Endpointer ep = makeEp();
  TEST_ASSERT_FALSE(ep.feed(2000, 100));        // speech
  TEST_ASSERT_FALSE(ep.feed(100, 100));         // gap 100
  TEST_ASSERT_FALSE(ep.feed(100, 100));         // gap 200
  TEST_ASSERT_FALSE(ep.feed(100, 100));         // gap 300 (< 800)
  TEST_ASSERT_FALSE(ep.feed(2000, 100));        // speech again -> silence resets
  // Now a full hangover of trailing silence is needed again.
  for (int i = 0; i < 7; ++i) TEST_ASSERT_FALSE(ep.feed(100, 100));
  TEST_ASSERT_TRUE(ep.feed(100, 100));
}

// Continuous speech (never pauses) ends at the recog_patience cap.
void test_patience_cap(void) {
  Endpointer ep = makeEp();
  // 49 x 100 ms = 4900 ms, still speaking, no end yet.
  for (int i = 0; i < 49; ++i) TEST_ASSERT_FALSE(ep.feed(3000, 100));
  TEST_ASSERT_TRUE(ep.feed(3000, 100));  // totalMs == 5000 -> patience cap
}

// Quiet-only input never trips the pause path (speechSeen stays false); it only
// ends at the patience cap, where the v1.3 noise gate then rejects it.
void test_quiet_no_false_trigger(void) {
  Endpointer ep = makeEp();
  for (int i = 0; i < 40; ++i) TEST_ASSERT_FALSE(ep.feed(50, 100));  // 4000 ms quiet
  TEST_ASSERT_FALSE(ep.speechSeen);
  // ...and it does eventually end at the patience cap (5000 ms).
  for (int i = 0; i < 9; ++i) TEST_ASSERT_FALSE(ep.feed(50, 100));   // 4900 ms
  TEST_ASSERT_TRUE(ep.feed(50, 100));                                // 5000 ms
}

// Peak exactly at the threshold counts as speech (>=), not silence.
void test_threshold_inclusive(void) {
  Endpointer ep = makeEp();
  TEST_ASSERT_FALSE(ep.feed(500, 100));  // == silencePeak -> speech
  TEST_ASSERT_TRUE(ep.speechSeen);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pause_ends);
  RUN_TEST(test_no_cut_on_short_gap);
  RUN_TEST(test_patience_cap);
  RUN_TEST(test_quiet_no_false_trigger);
  RUN_TEST(test_threshold_inclusive);
  return UNITY_END();
}
