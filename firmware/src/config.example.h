#pragma once

// Pyramid firmware config — TEMPLATE (v2.1).
//
// Copy this file to `config.h` (same directory) and fill in your values.
// `config.h` is gitignored and MUST NOT be committed.
//
// v2.1: the device is a thin **WSS client** of our own server. All intelligence
// (ASR → LLM → TTS, persona/Role, history) lives server-side, so the firmware
// no longer holds LLM/ASR/TTS keys — only Wi-Fi, the server address, and a
// device token. (Intelligence off-device — MISSION principle.)

// Wi-Fi credentials (STA mode).
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"

// Gate every status log through logf(). Set false to quieten serial output.
#define DEBUG_SERIAL true

// On-screen transcript mode (v1.4). When true, the LCD shows the conversation —
// your text and the assistant's reply (streamed) — in a small Cyrillic-capable
// font instead of the big turn-state label. When false, the colored state screen.
#define SHOW_TRANSCRIPT false

// --- Server (v2.1) ----------------------------------------------------------
// Our backend's WSS endpoint (ARCHITECTURE §WS device↔server). Dev runs plain
// ws:// to a LAN server; set SERVER_USE_TLS true for wss:// (v2.6 cuts over to
// real public TLS). Point SERVER_HOST at the machine running `uvicorn`.
#define SERVER_HOST    "192.168.1.100"  // server IP / hostname
#define SERVER_PORT    8000
#define SERVER_PATH    "/ws"
#define SERVER_USE_TLS false            // false: ws://  ·  true: wss:// (self-signed in dev)

// Device identity. In v2.5 the server issues this via activation; for now set a
// placeholder the server accepts structurally (allowlist enforced from v2.6).
#define DEVICE_TOKEN   "dev-token"

// Protocol version negotiated in `hello` (major.minor). Server speaks major 1.
#define PROTO_VER      "1.0"

// Audio wire format advertised in `hello` (the only v2.1 format).
#define AUDIO_FMT      "pcm16"

// --- Audio (v1.1) -----------------------------------------------------------
// Capture/playback format on the Echo Base ES8311: 16 kHz mono PCM16, streamed
// to/from the server as binary frames.
#define AUDIO_SAMPLE_RATE 16000

// Max single listen window (ms). Caps the capture RAM buffer:
// AUDIO_SAMPLE_RATE * REC_MAX_MS/1000 samples * 2 bytes (5 s ≈ 160 KB).
#define REC_MAX_MS 5000

// Speaker volume for playback (0–255).
#define SPK_VOLUME 200

// --- End-of-utterance / VAD (v1.4) -----------------------------------------
// Pause-based end-of-utterance so you need not time the button: hold and speak,
// and streaming stops on a natural trailing pause (button release also ends it).
#define VAD_SILENCE_PEAK   500    // chunk peak below this counts as silence (|sample|)
#define VAD_HANGOVER_MS    800    // trailing silence after speech that ends capture
// recog_patience: hard cap on a single listen window (bounded by REC_MAX_MS RAM).
// In v2.2 this moves into the server-side Role (Role.recog_patience).
#define RECOG_PATIENCE_MS  5000

// Local pre-gate: don't open a listen window for an accidental, too-short tap.
#define REC_MIN_MS         300    // ignore captures shorter than this (ms)
#define REC_MIN_PEAK       500    // ignore captures quieter than this (|sample|)
