#pragma once

// Pyramid — turn orchestration (app namespace). Ties the cloud clients, audio
// I/O, and state machine into the shared ASR->LLM->TTS chain, with mid-turn
// failure recovery.

#include <string>

namespace app {

// Centralized mid-turn failure: log, restore the audio bus, recover to Offline
// (Wi-Fi dropped) or Error (auto-returns to Idle).
void failTurn(const char* what);

// One chat turn from a user utterance (typed or transcribed): LLM (+history) ->
// TTS -> speaker. Shared by the serial text path and the voice path.
void handleTurn(const std::string& userText);

// Speak a short nudge when the input couldn't be used (silence / empty / low
// confidence).
void rePrompt(const char* reason);

// Voice turn: gate the recording, transcribe via ASR, then run handleTurn().
void voiceTurn();

}  // namespace app
