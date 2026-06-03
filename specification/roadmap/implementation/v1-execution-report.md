# Version v1 — Execution Report

**Date:** 2026-06-03
**Branch:** main
**Label:** v1::version:1
**Scope:** phases **v1.1** (Audio I/O + PlatformIO, PYR-004…007), **v1.2** (TTS output, PYR-008…010), **v1.3** (ASR / full voice loop, PYR-011…013), **v1.4** (states / UX, PYR-014…017)
**Executed by:** Claude Code

> v1.1 (**1.1.0**), v1.2 (**1.2.0**), and v1.3 (**1.3.0**) are released and
> hardware-verified — the full voice loop works: hold the button, speak
> Ukrainian, hear a reply. **v1.4 (states/UX) is now code-complete**: a turn-state
> machine drives the LCD, pause-based end-of-utterance, mid-turn resilience, and
> the ASR pre-warm / timing-attribution latency wins. All **14** issues
> (PYR-004…017) closed. With v1.4 done, **v1 is complete** (would release as
> `1.4.0` on confirmation). The default persona was reverted to **Піраміда** for
> v1 (the Лілі canon moves server-side in v2). v1.4's on-device behavior (LCD
> legibility, VAD tuning, induced-failure recovery, the pre-warm latency delta)
> awaits a manual upload + check.

## Summary

| Status | Count |
|--------|-------|
| Completed & hardware-verified (closed) | 10 |
| Completed, host-validated / on-device check pending (closed) | 4 |
| Failed | 0 |
| Skipped | 0 |
| Remaining | 0 |

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
| 14 | PYR-014 | Turn-state machine + LCD states | v1.4 | completed | 7bd3dd2 | 3 | native 6/6 (states) + build |
| 15 | PYR-015 | Pause-based end-of-utterance (VAD) | v1.4 | completed | 978e87b | 4 | native 5/5 (vad) + build |
| 16 | PYR-016 | Mid-turn resilience (Wi-Fi loss + timeouts) | v1.4 | completed | 2bb499f | 1 | native (states) + build |
| 17 | PYR-017 | Latency hardening (pre-warm ASR TLS + timing) | v1.4 | completed | a330078 | 3 | native (asr/parseHost) + build |

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

### PYR-014: Turn-state machine + LCD states
**Commit:** 7bd3dd2 · [#14](https://github.com/ichMaster/pyramid/issues/14) (closed)
- New pure `src/states.h`: `TurnState` (Idle/Listening/Thinking/Replying/Error/Offline) + `TurnEvent` + `nextState()` + `label()`. `main.cpp` replaced all scattered `showStatus("…")` strings with `g_state` + `renderState()` (label + per-state LCD color) + `applyEvent()`; Replying is distinct from Thinking; Wi-Fi loss/restore override from any state.
- **Validation:** ✅ `pio test -e native -f test_states` 6/6; ✅ `pio run` SUCCESS; grep confirms no `showStatus` remains. ⚠️ On-device LCD legibility through a turn: manual.

### PYR-015: Pause-based end-of-utterance (VAD)
**Commit:** 978e87b · [#15](https://github.com/ichMaster/pyramid/issues/15) (closed)
- New pure `src/vad.h` `Endpointer`: fed each captured chunk's peak, ends on a trailing pause (hangover after speech) or the `recog_patience` cap; quiet-only never trips the pause path; a short gap doesn't cut. `recordWhileHeld()` analyzes each ~32 ms chunk and stops on release **or** pause (`end=pause|release`); `loop()` drains a held button after a pause-ended turn. New knobs `VAD_SILENCE_PEAK`/`VAD_HANGOVER_MS`/`RECOG_PATIENCE_MS`.
- **Validation:** ✅ `pio test -e native -f test_vad` 5/5; ✅ `pio run` SUCCESS. ⚠️ On-device threshold/hangover tuning: manual. (Build first failed on aggregate-init under gnu++11 → added an explicit `Endpointer` ctor.)

### PYR-016: Mid-turn resilience (Wi-Fi loss + per-stage timeouts)
**Commit:** 2bb499f · [#16](https://github.com/ichMaster/pyramid/issues/16) (closed)
- `failTurn()` centralizes mid-turn aborts: log → `ensureMicMode()` (restore the shared bus) → Offline if Wi-Fi dropped (input paused; `serviceWiFi` recovers) else Error. Error auto-returns to Idle after `kErrorDwellMs`, so the loop never sticks. LLM/speaker/network-ASR failures route through it; per-stage read/connect timeouts already bound each stage.
- **Validation:** ✅ `pio run` SUCCESS; ✅ `pio test -e native` (the Fail→Error→Done→Idle and WifiLost→Offline→WifiUp→Idle transitions are covered by `test_states`). ⚠️ Induced-failure checks (pull Wi-Fi, stall a stage): manual.

### PYR-017: Latency hardening (pre-warm ASR TLS + timing attribution)
**Commit:** a330078 · [#17](https://github.com/ichMaster/pyramid/issues/17) (closed)
- **#2:** `startAsrPrewarm()` opens the ASR TLS handshake on a background task (core 0) at capture start, overlapping the user's speech; `asrTranscribe()` waits for it and reuses the persistent `g_asrClient` (HTTPClient reuse), with a fresh-connect fallback. `parseHost()` (pure, in `asr_api.h`) derives the host. **#5:** `asrMs` is now stamped on every ASR outcome, so re-prompt turns no longer report `asr=0`.
- **Validation:** ✅ `pio test -e native` 25/25 (`test_asr` covers `parseHost`); ✅ `pio run` SUCCESS. ⚠️ **The latency delta and the threaded TLS reuse must be measured on hardware** — the DoD's before/after numbers come from the board; the fresh-connect fallback equals the v1.3 behavior if the warm socket proves flaky.

## Notes

- **Toolchain:** firmware is now **PlatformIO** (`pio run`, `pio run -t upload`, `pio test -e native`). espressif32 6.10.0 (arduino-esp32 2.x). Board profile `esp32-s3-devkitc-1` with AtomS3R USB flags; M5Unified detects the board at runtime.
- **Hardware bring-up done with the user on a connected AtomS3R + Atomic Echo Base.** The headless environment can't press the button, hear the speaker, or reliably read the S3's USB-CDC serial — so the audio acceptance items were confirmed interactively (serial logs + listening). Pure logic (parsers/framing/SSE/audio math) is fully covered by `pio test -e native`.
- **Benign log noise:** `[E] Wire.cpp:137 setPins(): bus already initialized` on each mic↔speaker switch (M5Unified re-touches the ES8311 I2C). No functional impact; the upstream example emits it too.
- **Streaming TTS now targeted at v2.1:** a `ttsSpeak()` that streamed PCM chunks to the speaker (to remove the buffer cap) tore/underran on the single-threaded TLS-read + real-time-play path, and hit a keep-alive read timeout. Reverted to buffered playback (smooth). The latency analysis confirmed this needs the server to pace the audio stream — so sentence-streaming TTS + early playback (recs #3/#4) were moved to **v2.1** in the ROADMAP, not v1.4.
- **TTS length is buffer-bounded:** spoken audio fits the ~5 s `REC_MAX_MS` buffer; the terse persona keeps replies short enough (observed 2.2–4.5 s), so no truncation in practice. The full reply text is always on serial regardless.
- **Secrets:** `src/config.h` holds the user's real Wi-Fi / Anthropic / ElevenLabs / **Deepgram** keys and is gitignored; only `config.example.h` is committed. ElevenLabs keys must include the **`text_to_speech`** permission; Deepgram keys must allow `/v1/listen`.
- **Stale-codec gotcha (v1.3):** after using the board for another project, the mic captured `peak=0` (ES8311 ADC left powered down). A **warm reflash didn't fix it; a cold USB power-cycle did**. Worth remembering for any audio bring-up.
- **µ-law upload (v1.3):** the recorded PCM is encoded to 8-bit µ-law **in place** before the ASR POST — halves the upload, fixing Deepgram `408 SLOW_UPLOAD` on longer clips. Validated end-to-end via curl before firmware.
- **Latency analysis (v1.3 instrumentation):** measured ASR ≈ **61 %** of post-speech latency, dominated by ~3 s fixed connection overhead (`ASR ≈ 3043 ms + 1.14 × clip_ms`). Levers applied: µ-law upload + TTS `eleven_turbo_v2_5` (v1.3), and the **ASR TLS pre-warm** (v1.4, PYR-017). Streaming ASR (#1) + sentence-streaming TTS / early playback (#3/#4) are scoped to **v2.1**.
- **Character:** the default persona was reverted to **Піраміда** for v1 (commit 21770d6) — a terse Ukrainian helper that suits the firmware persona. The authored **Лілі** canon stays in the spec (EMOTION_FACE.md / Name+Canon) and moves server-side into the v2 `Role`.

## Next Steps

- **v1 is code-complete (PYR-004…017 all closed).** v1.4 would release as **`1.4.0`** (per `A.B.C`) — on explicit confirmation only.
- **On-device check for v1.4** before release: flash, then verify LCD states through a turn, VAD pause-cutoff (`end=pause`) and threshold tuning, mid-turn recovery (pull Wi-Fi / stall a stage → clean Idle), and the **pre-warm latency delta** (compare `[latency]` ASR before/after; confirm re-prompt now shows `asr>0`).
- Then **v2** — server proxy (WSS), Name/Canon `Role`, web console, closed access, emoji face — per the updated ROADMAP. The deferred streaming TTS lands there (v2.1).
