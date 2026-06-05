#include "ui.h"

#include <M5Unified.h>

#include "app_state.h"

namespace app {
namespace {

#if !SHOW_TRANSCRIPT
// State -> LCD background color for the colored state screen.
uint16_t stateColor(pyramid::TurnState s) {
  switch (s) {
    case pyramid::TurnState::Idle:
      return TFT_BLACK;
    case pyramid::TurnState::Listening:
      return TFT_NAVY;     // capturing speech
    case pyramid::TurnState::Thinking:
      return TFT_OLIVE;    // processing (ASR/LLM/TTS)
    case pyramid::TurnState::Replying:
      return TFT_DARKGREEN;  // talking back
    case pyramid::TurnState::Error:
      return TFT_MAROON;   // a turn failed
    case pyramid::TurnState::Offline:
      return TFT_DARKGREY;  // Wi-Fi down / input paused
  }
  return TFT_BLACK;
}
#endif

#if SHOW_TRANSCRIPT
// Bright per-state color for the status word in transcript mode (the dark
// stateColor backgrounds would be unreadable as text on black).
uint16_t statusTextColor(pyramid::TurnState s) {
  switch (s) {
    case pyramid::TurnState::Idle:      return TFT_WHITE;
    case pyramid::TurnState::Listening: return TFT_CYAN;
    case pyramid::TurnState::Thinking:  return TFT_YELLOW;
    case pyramid::TurnState::Replying:  return TFT_GREEN;
    case pyramid::TurnState::Error:     return TFT_RED;
    case pyramid::TurnState::Offline:   return TFT_ORANGE;
  }
  return TFT_WHITE;
}

// Transcript mode: show the conversation text on the LCD in a small Unicode
// font. efontCN_12 is a U8g2 Unicode font whose base covers Cyrillic (incl.
// Ukrainian і/ї/є/ґ); the default GFX font is ASCII-only and would render
// Cyrillic as boxes.
void renderTranscript() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setFont(&fonts::efontCN_12);  // ~12 px, Cyrillic-capable
  M5.Display.setTextSize(1);
  M5.Display.setTextWrap(true);
  M5.Display.setCursor(0, 0);

  M5.Display.setTextColor(statusTextColor(g_state));
  M5.Display.printf("[%s]\n", pyramid::label(g_state));
  if (g_answerCount > 0) {  // last + average answer time (request-ready -> spoken)
    const uint32_t avg = static_cast<uint32_t>(g_answerSumMs / g_answerCount);
    M5.Display.setTextColor(TFT_YELLOW);
    M5.Display.printf("last %u.%us avg %u.%us\n", g_lastAnswerMs / 1000,
                      (g_lastAnswerMs % 1000) / 100, avg / 1000, (avg % 1000) / 100);
  }
  if (!g_userText.empty()) {
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.printf("> %s\n", g_userText.c_str());
  }
  if (!g_replyText.empty()) {
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.print(g_replyText.c_str());
  }
}
#endif

}  // namespace

void renderState() {
#if SHOW_TRANSCRIPT
  renderTranscript();
#else
  M5.Display.fillScreen(stateColor(g_state));
  M5.Display.setCursor(0, 0);
  M5.Display.print(pyramid::label(g_state));
#endif
}

void setState(pyramid::TurnState s) {
  g_state = s;
  if (s == pyramid::TurnState::Error) g_errorSinceMs = millis();
  renderState();
}

void applyEvent(pyramid::TurnEvent e) { setState(pyramid::nextState(g_state, e)); }

}  // namespace app
