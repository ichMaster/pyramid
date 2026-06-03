#pragma once

// Pyramid v1.4 — turn-state machine (pure, host-testable).
//
// One source of truth for "what is the device doing right now", so the LCD is
// always correct and every path updates state the same way. The transition
// function is pure (no M5 / Arduino deps) and unit-tested on the host; main.cpp
// fires events and maps each state to an LCD label + color. Distinguishing
// Replying (talking back) from Thinking (waiting) lets the user tell the two
// apart at a glance.

#include <cstdint>

namespace pyramid {

enum class TurnState : uint8_t {
  Idle,       // ready, waiting for the user
  Listening,  // capturing the user's speech
  Thinking,   // processing: ASR -> LLM -> TTS fetch
  Replying,   // speaking the answer back
  Error,      // a turn failed; shown briefly, then back to Idle
  Offline,    // Wi-Fi down (and at boot, before the first connect) — input paused
};

// Events that drive the turn. main.cpp fires these; nextState() maps them.
enum class TurnEvent : uint8_t {
  Listen,    // user started talking (button / VAD)
  Think,     // capture done, processing started
  Reply,     // audio playback started
  Done,      // turn finished (or degraded to text) -> Idle
  Fail,      // a stage errored -> Error
  WifiLost,  // connectivity dropped -> Offline (input paused)
  WifiUp,    // connectivity restored -> Idle
};

// Pure transition. Connectivity events win from any state (a drop must always
// surface as Offline; a restore clears it). While Offline, turn events are
// ignored — input is paused until Wi-Fi returns.
inline TurnState nextState(TurnState s, TurnEvent e) {
  switch (e) {
    case TurnEvent::WifiLost:
      return TurnState::Offline;
    case TurnEvent::WifiUp:
      return (s == TurnState::Offline) ? TurnState::Idle : s;
    default:
      break;
  }
  if (s == TurnState::Offline) return s;  // input paused while offline
  switch (e) {
    case TurnEvent::Listen:
      return TurnState::Listening;
    case TurnEvent::Think:
      return TurnState::Thinking;
    case TurnEvent::Reply:
      return TurnState::Replying;
    case TurnEvent::Done:
      return TurnState::Idle;
    case TurnEvent::Fail:
      return TurnState::Error;
    default:
      return s;
  }
}

// Short LCD label for a state (pure; the color mapping lives in main.cpp where
// the TFT constants are available).
inline const char* label(TurnState s) {
  switch (s) {
    case TurnState::Idle:
      return "idle";
    case TurnState::Listening:
      return "listening";
    case TurnState::Thinking:
      return "thinking";
    case TurnState::Replying:
      return "replying";
    case TurnState::Error:
      return "error";
    case TurnState::Offline:
      return "offline";
  }
  return "?";
}

}  // namespace pyramid
