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
// serial output; the reply/echo line is always written regardless.
#define DEBUG_SERIAL true
