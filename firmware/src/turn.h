#pragma once

// Pyramid v2.1 — turn handling (app namespace).
//
// The device is a thin WSS client now: it streams mic audio / typed text up and
// renders results. The ASR→LLM→TTS turn runs server-side. These are the WS event
// handlers (called from ws_client.cpp) plus the two input paths (push-to-talk
// streaming and the serial text bridge). Early/streaming playback: each inbound
// tts_audio frame is played as it arrives.

#include <cstddef>
#include <cstdint>
#include <string>

namespace app {

// Centralized mid-turn failure: log, restore the audio bus, recover to Offline
// (server/Wi-Fi dropped) or Error (auto-returns to Idle).
void failTurn(const char* what);

// WS events (from ws_client.cpp).
void onWsConnected();                              // send hello, go Idle
void onWsDisconnected();                           // restore bus, go Offline
void onWsText(const std::string& json);            // parse + drive state/transcript
void onTtsAudio(const uint8_t* data, size_t len);  // play a chunk immediately

// Input paths.
void streamVoice();                                // push-to-talk: stream a listen window
void handleTextIn(const std::string& text);        // serial line → text_in

}  // namespace app
