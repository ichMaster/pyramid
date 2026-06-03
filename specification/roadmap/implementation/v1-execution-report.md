# Version v1 — Execution Report

**Date:** 2026-06-03
**Branch:** main
**Label:** v1::version:1
**Scope:** phase **v1.1 — Audio I/O and PlatformIO migration** (PYR-004…007)
**Executed by:** Claude Code

> This run covers **v1.1 only**. v1.2 (TTS), v1.3 (ASR), and v1.4 (states/UX)
> are not yet broken into issues. v1 is **incomplete** → no version bump.

## Summary

| Status | Count |
|--------|-------|
| Completed | 4 |
| Failed | 0 |
| Skipped | 0 |
| Remaining (v1.1) | 0 |

## Issues

| # | PYR ID | Title | Phase | Status | Commit | Files | Tests |
|---|--------|-------|-------|--------|--------|-------|-------|
| 4 | PYR-004 | PlatformIO migration | v1.1 | completed | 053de73 | 11 | pio run + native build |
| 5 | PYR-005 | Native test environment | v1.1 | completed | fc2de16 | 7 | pio test -e native (5/5) |
| 6 | PYR-006 | I2S audio capture (push-to-talk) | v1.1 | completed | 88ca5d8 | 4 | native 6/6 + hardware |
| 7 | PYR-007 | I2S audio playback (record → playback) | v1.1 | completed | 9c97895 | 2 | hardware (audible) |

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

## Notes

- **Toolchain:** firmware is now **PlatformIO** (`pio run`, `pio run -t upload`, `pio test -e native`). espressif32 6.10.0 (arduino-esp32 2.x). Board profile `esp32-s3-devkitc-1` with AtomS3R USB flags; M5Unified detects the board at runtime.
- **Hardware bring-up done with the user on a connected AtomS3R + Atomic Echo Base.** The headless environment can't press the button, hear the speaker, or reliably read the S3's USB-CDC serial — so the audio acceptance items were confirmed interactively (serial logs + listening). Pure logic (parsers/framing/SSE/audio math) is fully covered by `pio test -e native`.
- **Benign log noise:** `[E] Wire.cpp:137 setPins(): bus already initialized` on each mic↔speaker switch (M5Unified re-touches the ES8311 I2C). No functional impact; the upstream example emits it too.
- **Secrets:** `src/config.h` holds the user's real Wi-Fi creds + Anthropic key and is gitignored; only `config.example.h` is committed.

## Next Steps

- **v1.2 — TTS output:** break the phase into issues (`v1.2-issues.md`), `/upload-issues`, then `/execute-issues`. The v1.1 audio playback path is the foundation TTS renders into.
- v1.3 (ASR) and v1.4 (states/UX) follow.
- **Release:** v1 is incomplete; when ready to cut a milestone for v1.1, use a fresh patch/minor (e.g. `0.4.0`) on the per-phase cadence — the `0.2.0` (nominal v1-complete) slot is already used.
