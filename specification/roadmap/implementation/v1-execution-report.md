# Version v1 — Execution Report

**Date:** 2026-06-03
**Branch:** main
**Label:** v1::version:1
**Scope:** phases **v1.1** (Audio I/O + PlatformIO, PYR-004…007) and **v1.2** (TTS output, PYR-008…010)
**Executed by:** Claude Code

> v1.1 (released **1.1.0**) and v1.2 are both **complete and hardware-verified**
> on the AtomS3R + Atomic Echo Base. v1.2 spoken replies use **buffered**
> playback (smooth); a streaming attempt tore/underran on the single-threaded
> TLS+audio path and is **deferred to v1.4** (needs a background audio task). All
> 7 issues (PYR-004…010) closed. v1.3 (ASR) and v1.4 (states/UX) remain. v1 is
> **incomplete** → no version bump yet (v1.2 would release as `1.2.0` on
> confirmation).

## Summary

| Status | Count |
|--------|-------|
| Completed & hardware-verified (closed) | 7 |
| Failed | 0 |
| Skipped | 0 |
| Remaining (v1.3, v1.4) | not yet scoped |

## Issues

| # | PYR ID | Title | Phase | Status | Commit | Files | Tests |
|---|--------|-------|-------|--------|--------|-------|-------|
| 4 | PYR-004 | PlatformIO migration | v1.1 | completed | 053de73 | 11 | pio run + native build |
| 5 | PYR-005 | Native test environment | v1.1 | completed | fc2de16 | 7 | pio test -e native (5/5) |
| 6 | PYR-006 | I2S audio capture (push-to-talk) | v1.1 | completed | 88ca5d8 | 4 | native 6/6 + hardware |
| 7 | PYR-007 | I2S audio playback (record → playback) | v1.1 | completed | 9c97895 | 2 | hardware (audible) |
| 8 | PYR-008 | Cloud TTS client (ElevenLabs → PCM16) | v1.2 | completed | 87f1be6 | 4 | native 7/7 + hardware |
| 9 | PYR-009 | TTS → playback pipeline | v1.2 | completed | 75450d1 | 1 | hardware (audible) |
| 10 | PYR-010 | TTS robustness (timeout/fallback/max len) | v1.2 | completed | 70dc1ff | 3 | native 7/7 + hardware |

> v1.2 tuning commit **a1022a5**: terse persona + 5 s buffer (`REC_MAX_MS`) + `TTS_MAX_CHARS` so a full reply fits buffered playback. (The streaming `ttsSpeak` experiment was reverted — see Notes.)

## Detailed Results

### PYR-004: PlatformIO migration
**Commit:** 053de73 · [#4](https://github.com/ichMaster/pyramid/issues/4) (closed)
- Added `firmware/platformio.ini` (`[env:atoms3r]` ESP32-S3 + arduino, M5Unified + ArduinoJson, USB flags `ARDUINO_USB_MODE=1`/`CDC_ON_BOOT=1`; `[env:native]`). Sources moved to `src/` (`pyramid.ino` → `main.cpp`). `config.h`/`.pio/` gitignored.
- **Validation:** ✅ `pio run` compiles (Flash 31%); ✅ `pio run -t upload` flashes + boots; ✅ USB flags match the M5Stack `m5stack_atoms3r` board. ⚠️ Headless serial runtime confirmation was blocked (S3 native USB CDC + no TTY); on-device boot/Wi-Fi/chat is a manual `pio device monitor` check.

### PYR-005: Native test environment
**Commit:** fc2de16 · [#5](https://github.com/ichMaster/pyramid/issues/5) (closed)
- Moved each suite to `test/test_<name>/` and converted to **Unity** (`TEST_ASSERT_TRUE_MESSAGE`, `setUp`/`tearDown`, `UNITY_BEGIN/RUN_TEST/UNITY_END`).
- **Validation:** ✅ `pio test -e native` → **5/5 suites pass**; ArduinoJson resolves for native; pure headers compile host-side.

### PYR-006: I2S audio capture (push-to-talk)
**Commit:** 88ca5d8 · [#6](https://github.com/ichMaster/pyramid/issues/6) (closed)
- `src/audio.h` (pure: `samplesForMs`/`capSamples`/`analyzePcm`) + `test/test_audio/`. `recordWhileHeld()` records 16 kHz mono PCM16 while BtnA held, bounded by `REC_MAX_MS`, logs `rec: <N> samples (<ms>) peak=<P> clipped=<C>`.
- Enabled the Atomic Echo Base **ES8311** via `cfg.external_speaker.atomic_echo = true` (the fix for the initial `M5.Mic.begin()` failure).
- **Validation:** ✅ native 6/6 (incl. `test_audio`); ✅ `pio run` (RAM 53.9% with the 128 KB buffer); ✅ **hardware**: peak ~17k–24k on speech, `clipped=0`, duration capped.

### PYR-007: I2S audio playback (record → playback)
**Commit:** 9c97895 · [#7](https://github.com/ichMaster/pyramid/issues/7) (closed)
- `playbackCaptured()` plays the captured buffer on release. Shared ES8311/I2S switch follows M5Unified's `Microphone.ino`: drain → `Mic.end` → `Speaker.begin` → play → `Speaker.end` → `Mic.begin`; setup starts in mic mode.
- **Validation:** ✅ `pio run`; ✅ **hardware**: 8+ record→playback cycles, **audible playback**, no restarts. (Fixed a crash — `i2s_stop` in `mic_task` from tearing down the shared I2S under a live task — by ending the active side before claiming the bus.)

### PYR-008: Cloud TTS client (ElevenLabs → PCM16)
**Commit:** 87f1be6 · [#8](https://github.com/ichMaster/pyramid/issues/8) (closed)
- `src/tts_api.h` (pure: `buildTtsRequest`) + `test/test_tts/`. `ttsFetch()` POSTs to ElevenLabs `…/{voice}?output_format=pcm_16000` (`xi-api-key`, `accept: audio/pcm`), reads raw 16 kHz PCM16 into `g_pcm` (Content-Length or chunked via `Dechunker`).
- **Validation:** ✅ `pio test -e native` 7/7; ✅ **hardware**: `audio/pcm` 200, `tts: <N> samples` per turn. (Note: the user's first key lacked the `text_to_speech` permission — a scoped-key issue, not code.)

### PYR-009: TTS → playback pipeline
**Commit:** 75450d1 · [#9](https://github.com/ichMaster/pyramid/issues/9) (closed)
- Serial path: after a successful LLM reply, `ttsFetch(reply)` → `playbackCaptured()` (reuses the v1.1 mic↔speaker switch). Text/reply stay on serial; LCD shows `speaking`. TTS failure degrades to logged text.
- **Validation:** ✅ **hardware**: typed line → spoken Ukrainian reply, 8-turn session, smooth + complete (2.2–4.5 s), no crashes.

### PYR-010: TTS robustness (timeout / fallback / max length)
**Commit:** 70dc1ff · [#10](https://github.com/ichMaster/pyramid/issues/10) (closed)
- `clampUtf8()` (UTF-8 boundary-safe, host-tested) caps the reply at `TTS_MAX_CHARS`; bounded TTS timeout; spoken-or-logged fallback (exercised live when the key lacked permission — reply stayed on serial); empty audio rejected.
- **Validation:** ✅ `pio test -e native` 7/7 (`test_tts` covers `clampUtf8`); ✅ **hardware** reliability pass.

## Notes

- **Toolchain:** firmware is now **PlatformIO** (`pio run`, `pio run -t upload`, `pio test -e native`). espressif32 6.10.0 (arduino-esp32 2.x). Board profile `esp32-s3-devkitc-1` with AtomS3R USB flags; M5Unified detects the board at runtime.
- **Hardware bring-up done with the user on a connected AtomS3R + Atomic Echo Base.** The headless environment can't press the button, hear the speaker, or reliably read the S3's USB-CDC serial — so the audio acceptance items were confirmed interactively (serial logs + listening). Pure logic (parsers/framing/SSE/audio math) is fully covered by `pio test -e native`.
- **Benign log noise:** `[E] Wire.cpp:137 setPins(): bus already initialized` on each mic↔speaker switch (M5Unified re-touches the ES8311 I2C). No functional impact; the upstream example emits it too.
- **Streaming deferred to v1.4:** a `ttsSpeak()` that streamed PCM chunks to the speaker (to remove the buffer cap) tore/underran on the single-threaded TLS-read + real-time-play path, and hit a keep-alive read timeout. Reverted to buffered playback (smooth); gapless long-reply playback needs a background audio task / ring buffer — a v1.4 item.
- **TTS length is buffer-bounded:** spoken audio fits the ~5 s `REC_MAX_MS` buffer; the terse persona keeps replies short enough (observed 2.2–4.5 s), so no truncation in practice. The full reply text is always on serial regardless.
- **Secrets:** `src/config.h` holds the user's real Wi-Fi / Anthropic / ElevenLabs keys and is gitignored; only `config.example.h` is committed. ElevenLabs keys must include the **`text_to_speech`** permission.

## Next Steps

- **Release v1.2 as `1.2.0`** (per `A.B.C`) — on explicit confirmation only.
- **v1.3 (ASR)** then **v1.4 (states/UX)**: break into issue files (`v1.3-issues.md`, …) and execute. v1.3 reuses the PTT capture (v1.1) → cloud ASR → the existing LLM→TTS chain. v1.4 can pick up **streaming TTS playback** (background audio task) and pause-based end-of-utterance.
