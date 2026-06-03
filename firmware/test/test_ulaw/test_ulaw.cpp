// Host unit test for the pure v1.3 G.711 µ-law encoder (ulaw.h).
// Reference bytes generated from the same encoder validated against Deepgram.
// Run via `pio test -e native`.

#include <unity.h>
#include <cstdint>

#include "ulaw.h"

using pyramid::ulawEncode;

#define CHECK(cond, msg) TEST_ASSERT_TRUE_MESSAGE((cond), (msg))

void setUp(void) {}
void tearDown(void) {}

void test_all(void) {
  CHECK(ulawEncode(0) == 0xFF, "ulaw 0");
  CHECK(ulawEncode(100) == 0xF2, "ulaw 100");
  CHECK(ulawEncode(-100) == 0x72, "ulaw -100");
  CHECK(ulawEncode(1000) == 0xCE, "ulaw 1000");
  CHECK(ulawEncode(-1000) == 0x4E, "ulaw -1000");
  CHECK(ulawEncode(8000) == 0xA0, "ulaw 8000");
  CHECK(ulawEncode(-8000) == 0x20, "ulaw -8000");
  CHECK(ulawEncode(32767) == 0x80, "ulaw 32767 (clip)");
  CHECK(ulawEncode(-32768) == 0x00, "ulaw -32768 (clip)");

  // Sign symmetry: +x and -x differ only in the 0x80 sign bit.
  CHECK((ulawEncode(5000) ^ ulawEncode(-5000)) == 0x80, "ulaw sign bit");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all);
  return UNITY_END();
}
