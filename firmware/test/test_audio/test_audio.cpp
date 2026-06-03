// Host unit test for the pure v1.1 audio math (audio.h): sample sizing,
// duration cap, and PCM level/clipping analysis. Run via `pio test -e native`.

#include <unity.h>
#include <cstdint>

#include "audio.h"

using pyramid::analyzePcm;
using pyramid::capSamples;
using pyramid::PcmStats;
using pyramid::samplesForMs;

#define CHECK(cond, msg) TEST_ASSERT_TRUE_MESSAGE((cond), (msg))

void setUp(void) {}
void tearDown(void) {}

void test_all(void) {
  // samplesForMs: 1 s @ 16 kHz = 16000 samples; 250 ms = 4000.
  CHECK(samplesForMs(1000, 16000) == 16000, "samplesForMs: 1s@16k");
  CHECK(samplesForMs(250, 16000) == 4000, "samplesForMs: 250ms@16k");
  CHECK(samplesForMs(0, 16000) == 0, "samplesForMs: zero");

  // capSamples bounds the request to the max.
  CHECK(capSamples(100, 64000) == 100, "capSamples: under");
  CHECK(capSamples(70000, 64000) == 64000, "capSamples: over -> cap");
  CHECK(capSamples(64000, 64000) == 64000, "capSamples: equal");

  // analyzePcm: peak magnitude + clip count (threshold 32700).
  {
    const std::int16_t buf[] = {0, 100, -200, 32767, -32700, 5};
    PcmStats s = analyzePcm(buf, 6, 32700);
    CHECK(s.peak == 32767, "analyze: peak");
    CHECK(s.clipped == 2, "analyze: two clipped (32767, -32700)");
  }

  // Silence: zero peak, no clipping.
  {
    const std::int16_t buf[] = {0, 0, 0, 0};
    PcmStats s = analyzePcm(buf, 4, 32700);
    CHECK(s.peak == 0, "analyze: silence peak 0");
    CHECK(s.clipped == 0, "analyze: silence no clip");
  }

  // Empty span is safe.
  {
    PcmStats s = analyzePcm(nullptr, 0, 32700);
    CHECK(s.peak == 0 && s.clipped == 0, "analyze: empty span");
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all);
  return UNITY_END();
}
