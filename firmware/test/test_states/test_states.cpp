// Host unit test for the pure v1.4 turn-state machine (states.h).
// Run via `pio test -e native`.

#include <unity.h>
#include <cstring>

#include "states.h"

using pyramid::label;
using pyramid::nextState;
using pyramid::TurnEvent;
using pyramid::TurnState;

void setUp(void) {}
void tearDown(void) {}

// The normal turn cycle: Idle -> Listening -> Thinking -> Replying -> Idle.
void test_happy_cycle(void) {
  TurnState s = TurnState::Idle;
  s = nextState(s, TurnEvent::Listen);
  TEST_ASSERT_EQUAL(TurnState::Listening, s);
  s = nextState(s, TurnEvent::Think);
  TEST_ASSERT_EQUAL(TurnState::Thinking, s);
  s = nextState(s, TurnEvent::Reply);
  TEST_ASSERT_EQUAL(TurnState::Replying, s);
  s = nextState(s, TurnEvent::Done);
  TEST_ASSERT_EQUAL(TurnState::Idle, s);
}

// A failure routes to Error, and Done recovers to Idle.
void test_error_recovers(void) {
  TurnState s = nextState(TurnState::Thinking, TurnEvent::Fail);
  TEST_ASSERT_EQUAL(TurnState::Error, s);
  s = nextState(s, TurnEvent::Done);
  TEST_ASSERT_EQUAL(TurnState::Idle, s);
}

// Wi-Fi loss wins from any state; restore clears it to Idle.
void test_wifi_overrides(void) {
  const TurnState from[] = {TurnState::Idle, TurnState::Listening,
                            TurnState::Thinking, TurnState::Replying,
                            TurnState::Error};
  for (TurnState s : from) {
    TEST_ASSERT_EQUAL(TurnState::Offline, nextState(s, TurnEvent::WifiLost));
  }
  TEST_ASSERT_EQUAL(TurnState::Idle, nextState(TurnState::Offline, TurnEvent::WifiUp));
}

// While Offline, turn events are ignored (input paused) until Wi-Fi returns.
void test_offline_pauses_input(void) {
  TEST_ASSERT_EQUAL(TurnState::Offline,
                    nextState(TurnState::Offline, TurnEvent::Listen));
  TEST_ASSERT_EQUAL(TurnState::Offline,
                    nextState(TurnState::Offline, TurnEvent::Think));
  TEST_ASSERT_EQUAL(TurnState::Offline,
                    nextState(TurnState::Offline, TurnEvent::Reply));
}

// WifiUp from a non-Offline state is a no-op (shouldn't normally fire).
void test_wifiup_noop_when_online(void) {
  TEST_ASSERT_EQUAL(TurnState::Thinking,
                    nextState(TurnState::Thinking, TurnEvent::WifiUp));
}

// Every state has a distinct, non-empty label; Thinking != Replying so the
// user can tell "still working" from "talking back".
void test_labels(void) {
  TEST_ASSERT_EQUAL_STRING("idle", label(TurnState::Idle));
  TEST_ASSERT_EQUAL_STRING("listening", label(TurnState::Listening));
  TEST_ASSERT_EQUAL_STRING("thinking", label(TurnState::Thinking));
  TEST_ASSERT_EQUAL_STRING("replying", label(TurnState::Replying));
  TEST_ASSERT_EQUAL_STRING("error", label(TurnState::Error));
  TEST_ASSERT_EQUAL_STRING("offline", label(TurnState::Offline));
  TEST_ASSERT_TRUE(strcmp(label(TurnState::Thinking), label(TurnState::Replying)) != 0);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_happy_cycle);
  RUN_TEST(test_error_recovers);
  RUN_TEST(test_wifi_overrides);
  RUN_TEST(test_offline_pauses_input);
  RUN_TEST(test_wifiup_noop_when_online);
  RUN_TEST(test_labels);
  return UNITY_END();
}
