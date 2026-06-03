// Host unit test for the pure v0.3 conversation history (history.h):
// windowing by turn count and the "window starts with a user turn" invariant.
//
//   c++ -std=c++17 -I../pyramid test_history.cpp -o /tmp/test_history \
//     && /tmp/test_history

#include <cstdio>
#include <string>

#include "history.h"

using pyramid::History;
using pyramid::Turn;

static int g_failures = 0;

#define CHECK(cond, msg)                  \
  do {                                    \
    if (!(cond)) {                        \
      std::printf("FAIL: %s\n", (msg));   \
      ++g_failures;                       \
    }                                     \
  } while (0)

int main() {
  // 1. Empty history has no turns.
  {
    History h;
    CHECK(h.size() == 0, "empty: size 0");
    CHECK(h.turns().empty(), "empty: no turns");
  }

  // 2. Turns are appended in order with the right roles.
  {
    History h(8);
    h.addUser("hi");
    h.addAssistant("привіт");
    CHECK(h.size() == 2, "order: two turns");
    CHECK(h.turns()[0].role == "user" && h.turns()[0].content == "hi",
          "order: user first");
    CHECK(h.turns()[1].role == "assistant" && h.turns()[1].content == "привіт",
          "order: assistant second");
  }

  // 3. Windowing caps the number of turns and keeps the most recent.
  {
    History h(4);
    h.addUser("u1");
    h.addAssistant("a1");
    h.addUser("u2");
    h.addAssistant("a2");
    h.addUser("u3");
    h.addAssistant("a3");
    CHECK(h.size() == 4, "window: capped at 4");
    // Oldest (u1/a1) dropped; window starts with u2.
    CHECK(h.turns().front().content == "u2", "window: keeps most recent");
    CHECK(h.turns().back().content == "a3", "window: newest at the back");
  }

  // 4. Window always starts with a user turn (Messages API requirement).
  //    Cap 3 over 3 pairs would leave [a2,u3,a3]; the leading assistant is
  //    dropped, yielding [u3,a3].
  {
    History h(3);
    h.addUser("u1");
    h.addAssistant("a1");
    h.addUser("u2");
    h.addAssistant("a2");
    h.addUser("u3");
    h.addAssistant("a3");
    CHECK(!h.turns().empty(), "starts-user: non-empty");
    CHECK(h.turns().front().role == "user", "starts-user: front is user");
    CHECK(h.turns().front().content == "u3", "starts-user: dropped lead asst");
    CHECK(h.size() == 2, "starts-user: [u3,a3]");
  }

  // 5. clear() empties the history.
  {
    History h;
    h.addUser("x");
    h.addAssistant("y");
    h.clear();
    CHECK(h.size() == 0, "clear: empty after clear");
  }

  if (g_failures == 0) {
    std::printf("ok - all tests passed\n");
    return 0;
  }
  std::printf("FAILED - %d check(s) failed\n", g_failures);
  return 1;
}
