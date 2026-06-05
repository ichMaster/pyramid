# Pyramid firmware

AtomS3R + Atomic Echo Base firmware for the Pyramid voice AI assistant. The
device is **thin** — I/O and a status screen only; no persona/LLM/memory logic
lives here (ARCHITECTURE §Components). Built with **PlatformIO** (from v1.1; v0
used the Arduino IDE — the sketch `pyramid/pyramid.ino` moved to `src/`).

**Deep dive:** [docs/firmware/ARCHITECTURE.md](../docs/firmware/ARCHITECTURE.md)
(structure, data flow, turn lifecycle) and
[docs/firmware/INTERNALS.md](../docs/firmware/INTERNALS.md) (line-by-line
walkthrough + per-file inventory).

## Layout

The firmware glue is split into small modules (in `namespace app`) that share a
little mutable state via `app_state`; the pure, Arduino-free logic lives in
header-only units shared with the host tests.

```
firmware/
  platformio.ini        # PlatformIO project: atoms3r (device) + native (host tests)
  src/
    main.cpp            # setup() / loop()
    app_state.h/.cpp    # shared globals + tuning constants
    log.h               # status logging (app::logf, gated by DEBUG_SERIAL)
    ui.h/.cpp           # turn-state machine + LCD (state screen / transcript)
    net.h/.cpp          # Wi-Fi bring-up + non-blocking reconnect supervisor
    audio_io.h/.cpp     # Echo Base mic capture + speaker playback (ES8311 / I2S)
    cloud.h/.cpp        # LLM (Anthropic) / ASR (Deepgram) / TTS (ElevenLabs) HTTPS clients
    turn.h/.cpp         # ASR->LLM->TTS orchestration + mid-turn failure recovery
    config.example.h    # config template — copy to config.h (gitignored)
    # --- pure, host-testable headers ---
    line_reader.h       # non-blocking line reader
    serial_protocol.h   # text_in parse
    chat_api.h          # LLM request build / reply parse / retry class
    history.h           # short rolling conversation history
    backoff.h           # capped exponential backoff
    sse.h               # chunked + SSE streaming decode
    audio.h             # PCM sizing / level stats / capture gate
    vad.h               # pause-based end-of-utterance (endpointer)
    timing.h            # press->speak latency breakdown
    states.h            # turn-state enum + transitions
    tts_api.h           # TTS request build / UTF-8 clamp
    asr_api.h           # ASR (Deepgram) response parse + host extraction
    ulaw.h              # G.711 µ-law encode (ASR upload)
  test/                 # one Unity suite per pure header: test_<name>/
```

The AtomS3R has no built-in PlatformIO board definition, so the env targets the
ESP32-S3 (`esp32-s3-devkitc-1`) with the AtomS3R's USB flags
(`ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`); M5Unified detects the actual
board at runtime.

## What it does so far

v1 (Voice) is complete — the device runs the full Ukrainian voice loop and a
text path. By phase:

- **v0.1–v0.3:** board + Wi-Fi + USB-CDC line channel; each typed `text_in` goes
  to **Claude** (Anthropic Messages API, direct HTTPS, persona as `system`) and
  the reply **streams** back token by token (SSE). A short **rolling history**
  is resent for context, the call does a **bounded retry** with backoff, **Wi-Fi
  loss auto-recovers**, and only successful turns are committed to history.
- **v1.1:** PlatformIO build + native test env, and **audio I/O** on the Echo
  Base (ES8311, `cfg.external_speaker.atomic_echo`). Push-to-talk record →
  playback; mic and speaker share the bus, so each side is ended before the
  other begins (per M5Unified's Microphone example).
- **v1.2:** cloud **TTS** (ElevenLabs, `pcm_16000`). The LLM reply is spoken
  through the Echo Base; buffered for smooth, complete playback.
- **v1.3:** cloud **ASR** (Deepgram) + the full voice loop — speak → transcribe
  → the same LLM→TTS chain. The capture is encoded to **8-bit µ-law in place**
  (halves the upload, fixing 408 SLOW_UPLOAD) with a bounded retry; a noise/length
  gate skips silence and a low-confidence transcript re-prompts.
- **v1.4:** **states & UX** — a turn-state machine drives the LCD
  (`idle/listening/thinking/replying/error/offline`), **pause-based
  end-of-utterance** (VAD, bounded by `recog_patience`) so the button needn't be
  timed, **mid-turn recovery** (Wi-Fi loss / per-stage timeouts → clean Idle),
  and an optional **on-screen transcript** (`SHOW_TRANSCRIPT`) showing the
  conversation + answer-time in a small Cyrillic-capable font.

Per-turn the device prints `[stats]` (LLM time-to-first-token / total / tokens),
`[latency]` (press→speak split into speech/asr/llm/tts), and `[answer]`
(last + session-average answer time). Behavior is **config-driven** (persona,
models, endpoints, voice, thresholds), never hardcoded.

**Security:** the keys are extractable from a flashed device; acceptable only
under the private allowlist model (ARCHITECTURE §Security) — never publish such
firmware. TLS uses `setInsecure()` in v0–v1 (no cert pinning).

## Build & flash (PlatformIO)

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/) (`pio`),
then from `firmware/`:

1. `cp src/config.example.h src/config.h` and fill it in (`config.h` is
   gitignored — it holds your keys, never commit it):
   - **Wi-Fi** — `WIFI_SSID`, `WIFI_PASS`
   - **LLM (Anthropic)** — `LLM_ENDPOINT`, `LLM_MODEL`, `LLM_API_KEY` (`sk-ant-…`),
     `LLM_ANTHROPIC_VERSION`, `LLM_MAX_TOKENS`, `LLM_PERSONA`, `HISTORY_MAX_TURNS`
   - **TTS (ElevenLabs)** — `TTS_API_KEY` (needs `text_to_speech` permission),
     `TTS_VOICE_ID`, `TTS_MODEL`, `TTS_MAX_CHARS`
   - **ASR (Deepgram)** — `ASR_ENDPOINT`, `ASR_API_KEY`, `ASR_MODEL`, `ASR_LANG`
   - **Audio / gates / VAD** — `AUDIO_SAMPLE_RATE`, `REC_MAX_MS`, `SPK_VOLUME`,
     `REC_MIN_MS`, `REC_MIN_PEAK`, `ASR_MIN_CONFIDENCE`, `VAD_SILENCE_PEAK`,
     `VAD_HANGOVER_MS`, `RECOG_PATIENCE_MS`
   - **UI / debug** — `DEBUG_SERIAL`, `SHOW_TRANSCRIPT`
2. `pio run` — compile (first build fetches M5Unified + ArduinoJson).
3. `pio run -t upload` — flash the AtomS3R (auto-detects the port, or
   `--upload-port /dev/cu.usbmodemXXXX`; close the serial monitor first).
4. `pio device monitor` — serial @115200. Type a line for a spoken reply, or hold
   **BtnA** and speak. Wi-Fi state is logged on boot.

> v0 was built in the Arduino IDE (board **M5AtomS3R**) / `arduino-cli compile
> --fqbn m5stack:esp32:m5stack_atoms3r`. PlatformIO is the supported toolchain
> from v1.

## Host tests (pure logic)

The pure, Arduino-free logic runs on the host via PlatformIO's **native** test
environment (Unity) — one suite per header. From `firmware/`:

```sh
pio test -e native
```

Suites: `line_reader`, `serial_protocol` (via chat_api), `chat_api`, `history`,
`backoff`, `sse`, `audio`, `vad`, `timing`, `states`, `tts`, `asr`, `ulaw`. Each
lives in `test/test_<name>/` and builds against the headers in `src/`;
ArduinoJson is pulled from `lib_deps` for the native build. This is the host-test
entry point CI runs (ARCHITECTURE §Testing and CI).

On-device behavior — Wi-Fi join, LCD, button, the mic/speaker bus, a live
ASR→LLM→TTS round-trip and audible Ukrainian reply — needs the board, real API
keys, and the network, so it's covered by compile + the manual DoD checks in
ROADMAP (the decode/parse/FSM logic above is fully host-tested).
