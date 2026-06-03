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
