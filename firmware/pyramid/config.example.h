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

// --- LLM (v0.2) -------------------------------------------------------------
// OpenAI-compatible chat-completions endpoint. Works with OpenAI, DeepSeek,
// Qwen, etc. — e.g. https://api.deepseek.com/v1/chat/completions
#define LLM_ENDPOINT "https://api.openai.com/v1/chat/completions"
#define LLM_MODEL    "gpt-4o-mini"
#define LLM_API_KEY  "sk-replace-me"

// The persona system prompt — the device's character. Behavior is defined by
// this config, not hardcoded in logic (config is the source of truth). In
// v2+ this moves server-side into the Role.
#define LLM_PERSONA \
  "Ти — дружній голосовий помічник на ім'я Піраміда. " \
  "Відповідай українською мовою, коротко, тепло і по суті."
