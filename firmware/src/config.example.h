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
  "Ти — Піраміда, дружній голосовий помічник. " \
  "Відповідай українською одним коротким реченням (до ~14 слів), " \
  "без емодзі, списків і розмітки."

// --- Conversation history (v0.3) --------------------------------------------
// How many past turns (user/assistant messages) to keep in RAM and resend with
// each request, for short-term context. Larger = more context, more tokens.
// History is per-session only; nothing is persisted on the device.
#define HISTORY_MAX_TURNS 8

// --- Audio (v1.1) -----------------------------------------------------------
// PCM capture/playback format (Echo Base mic/speaker). 16 kHz mono PCM16 is the
// format the cloud ASR/TTS expect from v1.2+.
#define AUDIO_SAMPLE_RATE 16000

// Fixed maximum audio duration (ms) for both push-to-talk capture and buffered
// TTS playback. Caps the RAM buffer: AUDIO_SAMPLE_RATE * REC_MAX_MS/1000
// samples * 2 bytes (5 s ≈ 160 KB). Keep small — SRAM is shared with Wi-Fi/TLS.
#define REC_MAX_MS 5000

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

// Max reply length (UTF-8 bytes, boundary-safe) sent to TTS. Playback is
// buffered into the REC_MAX_MS buffer (~5 s), so keep this within ~that much
// speech (~30 bytes/s of Ukrainian) or the audio is cut at the buffer; the full
// reply always stays on serial. The terse persona keeps replies well under this.
#define TTS_MAX_CHARS     200

// --- ASR (v1.3): Deepgram --------------------------------------------------
// Speech-to-text. Deepgram prerecorded accepts raw 16 kHz mono PCM16 (no
// multipart/WAV): POST the captured g_pcm bytes to
//   {ASR_ENDPOINT}?model={ASR_MODEL}&language={ASR_LANG}&encoding=linear16&sample_rate={rate}
// with header `Authorization: Token <key>`. Get a key at console.deepgram.com.
#define ASR_ENDPOINT     "https://api.deepgram.com/v1/listen"
#define ASR_API_KEY      "dg-replace-me"   // Authorization: Token <key>
#define ASR_MODEL        "nova-2"
#define ASR_LANG         "uk"
#define ASR_SAMPLE_RATE  AUDIO_SAMPLE_RATE  // 16 kHz; matches capture

// Robustness gates (v1.3): skip sending silence / accidental taps to ASR, and
// re-prompt instead of answering noise.
#define REC_MIN_MS         300    // ignore recordings shorter than this
#define REC_MIN_PEAK       500    // ignore recordings quieter than this (|sample|)
#define ASR_MIN_CONFIDENCE 0.30f  // below this, re-prompt instead of calling the LLM
