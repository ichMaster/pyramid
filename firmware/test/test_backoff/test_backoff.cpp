// Host unit test for the pure v0.3 backoff helper (backoff.h):
// capped exponential schedule.
//
//   c++ -std=c++17 -I../pyramid test_backoff.cpp -o /tmp/test_backoff \
//     && /tmp/test_backoff

#include <unity.h>
#include <cstdio>

#include "backoff.h"

using pyramid::backoffDelayMs;


#define CHECK(cond, msg) TEST_ASSERT_TRUE_MESSAGE((cond), (msg))

void setUp(void) {}
void tearDown(void) {}

void test_all(void) {
  // Doubling schedule from a 500 ms base, capped at 8000 ms.
  CHECK(backoffDelayMs(0, 500, 8000) == 500, "attempt 0 -> base");
  CHECK(backoffDelayMs(1, 500, 8000) == 1000, "attempt 1 -> 2x");
  CHECK(backoffDelayMs(2, 500, 8000) == 2000, "attempt 2 -> 4x");
  CHECK(backoffDelayMs(3, 500, 8000) == 4000, "attempt 3 -> 8x");
  CHECK(backoffDelayMs(4, 500, 8000) == 8000, "attempt 4 -> cap");
  CHECK(backoffDelayMs(10, 500, 8000) == 8000, "large attempt -> cap");

  // Negative attempts clamp to the base.
  CHECK(backoffDelayMs(-3, 500, 8000) == 500, "negative -> base");

  // Base already at/over cap returns the cap.
  CHECK(backoffDelayMs(2, 9000, 8000) == 8000, "base over cap -> cap");

}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all);
  return UNITY_END();
}
