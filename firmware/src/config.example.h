#pragma once

// Pyramid firmware config — TEMPLATE.
//
// Copy this file to `config.h` (same directory) and fill in your values.
// `config.h` is gitignored and MUST NOT be committed: from v0.2 it also holds
// the LLM API key, which is extractable from a flashed device. Per
// ARCHITECTURE §Security, on-device secrets are acceptable only under the
// private allowlist model — never publish such firmware.

// Wi-Fi credentials (STA mode).
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"

// Gate every status log through logf(). Set to false to quieten the device's
// serial output; the reply line is always written regardless.
#define DEBUG_SERIAL true

// --- LLM (v0.2): Anthropic Messages API -------------------------------------
// https://docs.anthropic.com/en/api/messages
#define LLM_ENDPOINT "https://api.anthropic.com/v1/messages"

// Model. Options (cheapest/fastest -> most capable):
//   claude-haiku-4-5-20251001  · claude-sonnet-4-6  · claude-opus-4-8
// Haiku suits short, low-latency voice replies; bump to Sonnet/Opus for depth.
#define LLM_MODEL "claude-haiku-4-5-20251001"

// API key (header: x-api-key). Get one at https://console.anthropic.com
#define LLM_API_KEY "sk-ant-replace-me"

// Anthropic API version header (anthropic-version). Stable Messages version.
#define LLM_ANTHROPIC_VERSION "2023-06-01"

// Upper bound on reply length (Messages API requires max_tokens). Short for a
// voice assistant; raise if you want longer answers.
#define LLM_MAX_TOKENS 1024

// The persona system prompt — the device's character. Behavior is defined by
// this config, not hardcoded in logic (config is the source of truth). In
// v2+ this moves server-side into the Role.
#define LLM_PERSONA \
  "Ти — дружній голосовий помічник на ім'я Піраміда. " \
  "Відповідай українською мовою, коротко, тепло і по суті."

// --- Conversation history (v0.3) --------------------------------------------
// How many past turns (user/assistant messages) to keep in RAM and resend with
// each request, for short-term context. Larger = more context, more tokens.
// History is per-session only; nothing is persisted on the device.
#define HISTORY_MAX_TURNS 8

// --- Audio (v1.1) -----------------------------------------------------------
// PCM capture/playback format (Echo Base mic/speaker). 16 kHz mono PCM16 is the
// format the cloud ASR/TTS expect from v1.2+.
#define AUDIO_SAMPLE_RATE 16000

// Fixed maximum push-to-talk record duration (ms). Caps the RAM buffer:
// AUDIO_SAMPLE_RATE * REC_MAX_MS/1000 samples * 2 bytes.
#define REC_MAX_MS 4000

// Speaker volume for playback (0–255).
#define SPK_VOLUME 200

// --- TTS (v1.2): ElevenLabs ------------------------------------------------
// Spoken replies. Provider: ElevenLabs, output_format=pcm_16000 (raw 16 kHz
// mono PCM16 — must match AUDIO_SAMPLE_RATE, no on-device decode). Ukrainian
// comes from the multilingual model, not the voice. Get a key at
// https://elevenlabs.io (Settings -> API Keys); pick any voice's ID.
// NOTE: PCM output may require a paid ElevenLabs tier (free tier can be
// MP3-only) — confirm pcm_16000 is available on your plan.
#define TTS_ENDPOINT_BASE "https://api.elevenlabs.io/v1/text-to-speech/"
#define TTS_API_KEY       "xi-replace-me"          // sent as the xi-api-key header
#define TTS_VOICE_ID      "replace-with-voice-id"  // an ElevenLabs voice id
#define TTS_MODEL         "eleven_multilingual_v2"
#define TTS_SAMPLE_RATE   AUDIO_SAMPLE_RATE         // 16 kHz; matches playback

// Max reply length (UTF-8 bytes, boundary-safe) sent to TTS, so a long reply
// fits the playback buffer and stays cheap. Over-long replies are truncated
// (v0.3 keeps the full text on serial). ~4 s of speech fits the v1.1 buffer.
#define TTS_MAX_CHARS     280
