#pragma once

// Pyramid — cloud API clients: LLM (Anthropic), TTS (ElevenLabs), ASR
// (Deepgram), over direct HTTPS (app namespace). Each fills/reads the shared
// g_pcm buffer or returns text. Wire-format parsing is in the pure *_api.h
// headers; this is the network glue.

#include <cstdint>
#include <string>
#include <vector>

#include "chat_api.h"  // pyramid::Turn (via history.h), pyramid::Usage

namespace app {

// Outcome of an LLM turn: ok (reply set), or failed (err set) with a retry hint.
// Carries time-to-first-token / total time and the token usage from the API.
struct Attempt {
  bool ok = false;
  bool retryable = false;
  std::string reply;
  std::string err;
  uint32_t firstMs = 0;  // request sent -> first token
  uint32_t totalMs = 0;  // request sent -> stream complete
  pyramid::Usage usage;
};

// LLM chat turn with bounded retry + backoff; streams tokens to serial as they
// arrive. Returns the final Attempt.
Attempt llmTurn(const std::vector<pyramid::Turn>& turns);

// Fetch spoken audio for `text` into g_pcm (raw 16 kHz PCM16); sets g_pcmLen.
bool ttsFetch(const std::string& text, std::string& err);

// Transcribe `pcm`/`samples` via Deepgram (µ-law in place). Sets transcript +
// confidence on a non-empty result. `pcm` is mutated (encoded in place).
bool asrTranscribe(int16_t* pcm, size_t samples, std::string& transcript,
                   float& confidence, std::string& err);

}  // namespace app
