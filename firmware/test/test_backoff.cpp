// Host unit test for the pure v0.3 backoff helper (backoff.h):
// capped exponential schedule.
//
//   c++ -std=c++17 -I../pyramid test_backoff.cpp -o /tmp/test_backoff \
//     && /tmp/test_backoff

#include <cstdio>

#include "backoff.h"

using pyramid::backoffDelayMs;

static int g_failures = 0;

#define CHECK(cond, msg)                  \
  do {                                    \
    if (!(cond)) {                        \
      std::printf("FAIL: %s\n", (msg));   \
      ++g_failures;                       \
    }                                     \
  } while (0)

int main() {
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

  if (g_failures == 0) {
    std::printf("ok - all tests passed\n");
    return 0;
  }
  std::printf("FAILED - %d check(s) failed\n", g_failures);
  return 1;
}
