# Version v1 — Execution Report

**Date:** 2026-06-03
**Branch:** main
**Label:** v1::version:1
**Scope:** phases **v1.1** (Audio I/O + PlatformIO, PYR-004…007) and **v1.2** (TTS output, PYR-008…010)
**Executed by:** Claude Code

> v1.1 is complete and hardware-verified (released as **1.1.0**). v1.2 is
> **implemented + host-tested + compiled + flashed**, but its on-device audio
> checks are **deferred until an ElevenLabs API key** (pcm_16000-capable tier)
> is set in `src/config.h` — so PYR-008/009/010 are committed (`refs`, not
> `Closes`) and remain **open** pending that verification. v1.3 (ASR) and v1.4
> (states/UX) are not yet broken into issues. v1 is **incomplete** → no version
> bump.

## Summary

| Status | Count |
|--------|-------|
| Completed (v1.1, closed) | 4 |
| Implemented, pending hardware (v1.2, open) | 3 |
| Failed | 0 |
| Skipped | 0 |

## Issues

| # | PYR ID | Title | Phase | Status | Commit | Files | Tests |
|---|--------|-------|-------|--------|--------|-------|-------|
| 4 | PYR-004 | PlatformIO migration | v1.1 | completed | 053de73 | 11 | pio run + native build |
| 5 | PYR-005 | Native test environment | v1.1 | completed | fc2de16 | 7 | pio test -e native (5/5) |
| 6 | PYR-006 | I2S audio capture (push-to-talk) | v1.1 | completed | 88ca5d8 | 4 | native 6/6 + hardware |
| 7 | PYR-007 | I2S audio playback (record → playback) | v1.1 | completed | 9c97895 | 2 | hardware (audible) |
| 8 | PYR-008 | Cloud TTS client (ElevenLabs → PCM16) | v1.2 | implemented (open) | 87f1be6 | 4 | native 7/7 + compile |
| 9 | PYR-009 | TTS → playback pipeline | v1.2 | implemented (open) | 75450d1 | 1 | compile |
| 10 | PYR-010 | TTS robustness (timeout/fallback/max len) | v1.2 | implemented (open) | 70dc1ff | 3 | native 7/7 + compile |

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
**Commit:** 87f1be6 · [#8](https://github.com/ichMaster/pyramid/issues/8) (**open** — pending key)
- `src/tts_api.h` (pure: `buildTtsRequest`) + `test/test_tts/`. `ttsFetch()` POSTs to ElevenLabs `…/{voice}?output_format=pcm_16000` (`xi-api-key`, `accept: audio/pcm`), reads raw 16 kHz PCM16 into `g_pcm` (Content-Length or chunked via `Dechunker`), bounded read + timeout. TTS config block added.
- **Validation:** ✅ `pio test -e native` 7/7 (incl. `test_tts`); ✅ `pio run`. ⏳ On-device "fetches audio" pending an ElevenLabs key (pcm_16000-capable).

### PYR-009: TTS → playback pipeline
**Commit:** 75450d1 · [#9](https://github.com/ichMaster/pyramid/issues/9) (**open** — pending key)
- Serial path: after a successful LLM reply, `ttsFetch(reply)` → `playbackCaptured()` (reuses the v1.1 mic↔speaker switch). Text/reply stay on serial; LCD shows `speaking`. TTS failure degrades to logged text.
- **Validation:** ✅ `pio run` compiles. ⏳ On-device "you hear the spoken reply" pending the key.

### PYR-010: TTS robustness (timeout / fallback / max length)
**Commit:** 70dc1ff · [#10](https://github.com/ichMaster/pyramid/issues/10) (**open** — pending key)
- `clampUtf8()` (UTF-8 boundary-safe, host-tested) caps the reply at `TTS_MAX_CHARS` with a truncation log; bounded TTS timeout (`kTtsReadMs`); spoken-or-logged fallback; empty audio rejected.
- **Validation:** ✅ `pio test -e native` 7/7 (`test_tts` covers `clampUtf8`); ✅ `pio run`. ⏳ Reliability pass pending the key.

## Notes

- **Toolchain:** firmware is now **PlatformIO** (`pio run`, `pio run -t upload`, `pio test -e native`). espressif32 6.10.0 (arduino-esp32 2.x). Board profile `esp32-s3-devkitc-1` with AtomS3R USB flags; M5Unified detects the board at runtime.
- **Hardware bring-up done with the user on a connected AtomS3R + Atomic Echo Base.** The headless environment can't press the button, hear the speaker, or reliably read the S3's USB-CDC serial — so the audio acceptance items were confirmed interactively (serial logs + listening). Pure logic (parsers/framing/SSE/audio math) is fully covered by `pio test -e native`.
- **Benign log noise:** `[E] Wire.cpp:137 setPins(): bus already initialized` on each mic↔speaker switch (M5Unified re-touches the ES8311 I2C). No functional impact; the upstream example emits it too.
- **Secrets:** `src/config.h` holds the user's real Wi-Fi creds + Anthropic key and is gitignored; only `config.example.h` is committed.

## Next Steps

- **Finish v1.2 (close #8/#9/#10):** add an ElevenLabs `TTS_API_KEY` + `TTS_VOICE_ID` to `src/config.h` (confirm the tier allows `pcm_16000`), flash, then on-device: type a serial line → hear the spoken Ukrainian reply; check `tts:` logs + truncation; run a short reliability pass. Then close the three issues as hardware-verified.
- **Release:** v1.1 shipped as **1.1.0**. When v1.2 is hardware-verified, cut **`1.2.0`** (per the `A.B.C` scheme) — only on explicit confirmation.
- **v1.3 (ASR)** then **v1.4 (states/UX)**: break into issue files (`v1.3-issues.md`, …) and execute. v1.3 reuses the PTT capture (v1.1) → cloud ASR → the existing LLM→TTS chain.
