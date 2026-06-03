# Version v1 — Execution Report

**Date:** 2026-06-03
**Branch:** main
**Label:** v1::version:1
**Scope:** phases **v1.1** (Audio I/O + PlatformIO, PYR-004…007), **v1.2** (TTS output, PYR-008…010), **v1.3** (ASR / full voice loop, PYR-011…013)
**Executed by:** Claude Code

> v1.1 (released **1.1.0**) and v1.2 (released **1.2.0**) are complete. **v1.3 is
> complete and hardware-verified**: you hold the button, speak Ukrainian, and
> hear a reply — the full voice loop. All **10** issues (PYR-004…013) closed.
> Only **v1.4 (states/UX)** remains in v1. v1 is **incomplete** → no version bump
> yet (v1.3 would release as `1.3.0` on confirmation). The device now speaks in
> the **Лілі** character.

## Summary

| Status | Count |
|--------|-------|
| Completed & hardware-verified (closed) | 10 |
| Failed | 0 |
| Skipped | 0 |
| Remaining (v1.4) | not yet scoped |

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
| 11 | PYR-011 | Cloud ASR client (Deepgram → transcript) | v1.3 | completed | 1fb87b5 | 4 | native 9/9 + hardware |
| 12 | PYR-012 | Full voice loop (button → ASR → LLM → TTS → speaker) | v1.3 | completed | 2417c93 | 1 | hardware (~10 turns) |
| 13 | PYR-013 | ASR robustness (gate / re-prompt / µ-law+retry) | v1.3 | completed | 6744e66 | 4 | native 9/9 + hardware |

> v1.2 tuning commit **a1022a5**: terse persona + 5 s buffer (`REC_MAX_MS`) + `TTS_MAX_CHARS` so a full reply fits buffered playback. (The streaming `ttsSpeak` experiment was reverted — see Notes.)
> v1.3 optimization commit **f88b035**: µ-law ASR upload + retry (fix 408 SLOW_UPLOAD), TTS → `eleven_turbo_v2_5` (latency); **979912f**: Лілі persona.

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

### PYR-011: Cloud ASR client (Deepgram → transcript)
**Commit:** 1fb87b5 (+ f88b035) · [#11](https://github.com/ichMaster/pyramid/issues/11) (closed)
- `src/asr_api.h` (pure: `parseAsrTranscript`) + `test/test_asr/`. `asrTranscribe()` POSTs the capture to Deepgram `/v1/listen` and parses `results.channels[0].alternatives[0].transcript`. Final version encodes to **8-bit µ-law in place** + `encoding=mulaw` (halves the upload) with a bounded retry.
- **Validation:** ✅ `pio test -e native` (asr + ulaw); ✅ **hardware**: Ukrainian transcripts at 74–99% confidence. Path de-risked via curl (ElevenLabs clip → Deepgram → 0.9995; µ-law@16k → 0.9995).

### PYR-012: Full voice loop (button → ASR → LLM → TTS → speaker)
**Commit:** 2417c93 · [#12](https://github.com/ichMaster/pyramid/issues/12) (closed)
- Button: `recordWhileHeld()` → `voiceTurn()` → `asrTranscribe(g_pcm)` → shared `handleTurn()` (LLM + history + TTS + playback). `asrTranscribe` reads `g_pcm` before `ttsFetch` overwrites it. Serial path still works.
- **Validation:** ✅ **hardware**: ~10-turn spoken Ukrainian exchange, end to end.

### PYR-013: ASR robustness (gate / re-prompt / µ-law + retry)
**Commit:** 6744e66 (+ f88b035) · [#13](https://github.com/ichMaster/pyramid/issues/13) (closed)
- Pure `shouldTranscribe()` (host-tested) gates too-short/too-quiet captures before any API call; empty/failed recognition → spoken re-prompt; bounded ASR timeout. The **µ-law upload + retry** fix the intermittent Deepgram **408 SLOW_UPLOAD** seen on longer clips.
- **Validation:** ✅ `pio test -e native` 9/9 (`test_audio` covers `shouldTranscribe`, `test_ulaw` the encoder); ✅ **hardware**: silent captures gated; loop never hangs.

## Notes

- **Toolchain:** firmware is now **PlatformIO** (`pio run`, `pio run -t upload`, `pio test -e native`). espressif32 6.10.0 (arduino-esp32 2.x). Board profile `esp32-s3-devkitc-1` with AtomS3R USB flags; M5Unified detects the board at runtime.
- **Hardware bring-up done with the user on a connected AtomS3R + Atomic Echo Base.** The headless environment can't press the button, hear the speaker, or reliably read the S3's USB-CDC serial — so the audio acceptance items were confirmed interactively (serial logs + listening). Pure logic (parsers/framing/SSE/audio math) is fully covered by `pio test -e native`.
- **Benign log noise:** `[E] Wire.cpp:137 setPins(): bus already initialized` on each mic↔speaker switch (M5Unified re-touches the ES8311 I2C). No functional impact; the upstream example emits it too.
- **Streaming deferred to v1.4:** a `ttsSpeak()` that streamed PCM chunks to the speaker (to remove the buffer cap) tore/underran on the single-threaded TLS-read + real-time-play path, and hit a keep-alive read timeout. Reverted to buffered playback (smooth); gapless long-reply playback needs a background audio task / ring buffer — a v1.4 item.
- **TTS length is buffer-bounded:** spoken audio fits the ~5 s `REC_MAX_MS` buffer; the terse persona keeps replies short enough (observed 2.2–4.5 s), so no truncation in practice. The full reply text is always on serial regardless.
- **Secrets:** `src/config.h` holds the user's real Wi-Fi / Anthropic / ElevenLabs / **Deepgram** keys and is gitignored; only `config.example.h` is committed. ElevenLabs keys must include the **`text_to_speech`** permission; Deepgram keys must allow `/v1/listen`.
- **Stale-codec gotcha (v1.3):** after using the board for another project, the mic captured `peak=0` (ES8311 ADC left powered down). A **warm reflash didn't fix it; a cold USB power-cycle did**. Worth remembering for any audio bring-up.
- **µ-law upload (v1.3):** the recorded PCM is encoded to 8-bit µ-law **in place** before the ASR POST — halves the upload, fixing Deepgram `408 SLOW_UPLOAD` on longer clips. Validated end-to-end via curl before firmware.
- **Latency levers applied:** µ-law upload + TTS `eleven_turbo_v2_5`. Remaining (v1.4): streaming TTS playback + LLM→TTS clause pipelining; TLS handshakes to 3 hosts are an inherent floor.
- **Character:** the default persona is now **Лілі** (authored canon; commit 979912f), with a short voice-format line so spoken replies fit the buffer. In v2 this canon moves server-side into the Role.

## Next Steps

- **Release v1.3 as `1.3.0`** (per `A.B.C`) — on explicit confirmation only.
- **v1.4 (states/UX):** the last v1 phase — pause-based end-of-utterance (VAD bounded by `recog_patience`), richer LCD states, mid-turn Wi-Fi/timeout recovery, and the deferred **streaming TTS playback** (background audio task) for lower latency + uncapped reply length.
- Then **v2** (server + Name/Canon + emoji face) per the updated ROADMAP.
