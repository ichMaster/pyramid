#pragma once

// Pyramid — shared firmware state and tuning constants (app namespace).
//
// The pure logic lives in the per-feature headers (states.h, vad.h, audio.h,
// timing.h, ...); the M5/WiFi/HTTP glue lives in the ui/net/audio_io/cloud/turn
// modules. Those modules are coupled through a handful of mutable globals — they
// are declared here (extern) and defined once in app_state.cpp.

#include <Arduino.h>

#include <cstdint>
#include <string>

#include "audio.h"         // samplesForMs (kMaxSamples)
#include "config.h"        // REC_MAX_MS, AUDIO_SAMPLE_RATE, HISTORY_MAX_TURNS, ...
#include "history.h"       // pyramid::History
#include "line_reader.h"   // pyramid::LineReader
#include "states.h"        // pyramid::TurnState
#include "timing.h"        // pyramid::VoiceStamps

namespace app {

// --- Tuning constants -------------------------------------------------------
constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kWifiTimeoutMs = 20000;
constexpr uint32_t kHttpConnectMs = 8000;
constexpr uint32_t kHttpReadMs = 15000;
constexpr uint32_t kTtsReadMs = 15000;  // TTS audio read budget (v1.2)
constexpr uint32_t kAsrReadMs = 15000;  // ASR read budget (v1.3)

// Bounded retry for transient failures (transport / 429 / 5xx).
constexpr int kMaxRetries = 2;
constexpr int kAsrMaxRetries = 2;
constexpr uint32_t kRetryBaseMs = 500;
constexpr uint32_t kRetryCapMs = 4000;

// Wi-Fi reconnect backoff (ARCHITECTURE §Error handling: ~0.5 -> 8 s).
constexpr uint32_t kWifiBackoffBaseMs = 500;
constexpr uint32_t kWifiBackoffCapMs = 8000;

// How long Error lingers on the LCD before auto-returning to Idle (v1.4).
constexpr uint32_t kErrorDwellMs = 2000;

// Audio capture buffer bound (compile-time; sizes g_pcm). PCM16 mono.
constexpr size_t kMaxSamples =
    pyramid::samplesForMs(REC_MAX_MS, AUDIO_SAMPLE_RATE);

// --- Shared globals (defined in app_state.cpp) ------------------------------
// Serial line reader + short rolling chat history.
extern pyramid::LineReader g_reader;
extern pyramid::History g_history;

// Wi-Fi reconnect state (non-blocking supervisor in net.cpp).
extern bool g_offline;
extern int g_wifiAttempt;
extern uint32_t g_nextWifiTryMs;

// Shared PCM16 buffer: capture writes it, TTS fills it, ASR encodes it in place.
extern int16_t g_pcm[kMaxSamples];
extern size_t g_pcmLen;

// Push-to-talk latency tracking (button press -> first spoken audio).
extern pyramid::VoiceStamps g_stamps;
extern bool g_voiceActive;

// Turn-state machine + LCD.
extern pyramid::TurnState g_state;
extern uint32_t g_errorSinceMs;

// Latest exchange, for the optional on-screen transcript (SHOW_TRANSCRIPT).
extern std::string g_userText;
extern std::string g_replyText;

// Answer-time stats (request ready -> reply spoken): last + session average.
extern uint32_t g_answerStartMs;
extern uint32_t g_lastAnswerMs;
extern uint32_t g_answerCount;
extern uint64_t g_answerSumMs;

}  // namespace app
