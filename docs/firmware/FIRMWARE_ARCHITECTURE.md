# Firmware Architecture (v1.4)

How the AtomS3R + Atomic Echo Base firmware is structured. This is the
**implementation** view of the device tier; for the product-wide contracts and
the v0–v3 plan see [specification/ARCHITECTURE.md](../../specification/ARCHITECTURE.md)
and [specification/ROADMAP.md](../../specification/ROADMAP.md). For a line-by-line
walkthrough and the per-file inventory, see [INTERNALS.md](INTERNALS.md).

> **Scope:** v1.4 — the device talks **directly to the cloud** (Anthropic /
> Deepgram / ElevenLabs) over HTTPS. There is no Pyramid server yet (that's v2).
> The device is **thin**: I/O + a status screen; no persona/memory/decision
> logic lives here — behavior is defined by `config.h`.

## Two layers

The code is split into two kinds of units:

1. **Pure logic** — header-only, Arduino-free, in `namespace pyramid`. Parsers,
   framing, state transitions, audio math. These have no `M5`/`WiFi`/`HTTPClient`
   dependency, so they compile and run on the host and are unit-tested under
   `pio test -e native`.
2. **Glue modules** — `.h`/`.cpp` pairs in `namespace app` that drive the
   hardware and network (M5Unified, Wi-Fi, HTTP/TLS) and call into the pure
   logic. These are validated by compile + on-device smoke tests.

```
        ┌────────────────────────── namespace app (glue) ──────────────────────────┐
        │  main → ui → net → audio_io → cloud → turn   (+ app_state, log)           │
        └───────────────────────────────┬──────────────────────────────────────────┘
                                         │ calls
        ┌────────────────────────── namespace pyramid (pure) ──────────────────────┐
        │  states · vad · audio · timing · sse · chat_api · tts_api · asr_api ·     │
        │  ulaw · history · backoff · line_reader · serial_protocol                 │
        └───────────────────────────────────────────────────────────────────────── ┘
```

## Module map (glue, `namespace app`)

| Module | Responsibility | Key entry points |
|--------|----------------|------------------|
| [main.cpp](../../firmware/src/main.cpp) | Arduino entry: wiring only | [`setup()`](../../firmware/src/main.cpp#L42), [`loop()`](../../firmware/src/main.cpp#L66) |
| [app_state.h](../../firmware/src/app_state.h) / [.cpp](../../firmware/src/app_state.cpp) | Shared globals + tuning constants | the `g_*` state, `k*Ms` limits, `kMaxSamples` |
| [log.h](../../firmware/src/log.h) | Status logging, gated by `DEBUG_SERIAL` | `app::logf` |
| [ui.h](../../firmware/src/ui.h) / [.cpp](../../firmware/src/ui.cpp) | Turn-state machine + LCD render | [`setState`](../../firmware/src/ui.cpp#L88), [`applyEvent`](../../firmware/src/ui.cpp#L94), [`renderState`](../../firmware/src/ui.cpp#L78) |
| [net.h](../../firmware/src/net.h) / [.cpp](../../firmware/src/net.cpp) | Wi-Fi bring-up + reconnect supervisor | [`connectWiFi`](../../firmware/src/net.cpp#L12), [`serviceWiFi`](../../firmware/src/net.cpp#L34) |
| [audio_io.h](../../firmware/src/audio_io.h) / [.cpp](../../firmware/src/audio_io.cpp) | Echo Base mic capture + speaker playback | [`recordWhileHeld`](../../firmware/src/audio_io.cpp#L26), [`playbackCaptured`](../../firmware/src/audio_io.cpp#L87), [`ensureMicMode`](../../firmware/src/audio_io.cpp#L18) |
| [cloud.h](../../firmware/src/cloud.h) / [.cpp](../../firmware/src/cloud.cpp) | LLM / ASR / TTS HTTPS clients | [`llmTurn`](../../firmware/src/cloud.cpp#L318), [`asrTranscribe`](../../firmware/src/cloud.cpp#L128), [`ttsFetch`](../../firmware/src/cloud.cpp#L22) |
| [turn.h](../../firmware/src/turn.h) / [.cpp](../../firmware/src/turn.cpp) | ASR→LLM→TTS orchestration + recovery | [`handleTurn`](../../firmware/src/turn.cpp#L29), [`voiceTurn`](../../firmware/src/turn.cpp#L106), [`failTurn`](../../firmware/src/turn.cpp#L17), [`rePrompt`](../../firmware/src/turn.cpp#L92) |

**Dependency direction** (who includes/calls whom):

```
main ─┬─ net ──────────────┐
      ├─ audio_io ──┐       ├─ ui  ── (states.h)
      ├─ turn ──────┼─ cloud (chat_api/sse/tts_api/asr_api/ulaw)
      │             └─ audio_io ── (audio.h, vad.h, timing.h)
      └─ ui
all glue ── app_state ── (history, line_reader, states, timing, audio, config)
all glue ── log
```

`audio_io` and `turn` call each other (`playbackCaptured` → `failTurn`;
`failTurn` → `ensureMicMode`) — a deliberate cycle resolved at the header level
(prototypes only), not an include cycle.

## Shared state (`app_state`)

The glue modules are coupled through a handful of mutable globals, declared
`extern` in [app_state.h](../../firmware/src/app_state.h) and defined once in
[app_state.cpp](../../firmware/src/app_state.cpp):

| Global | Owner / writer | Readers |
|--------|----------------|---------|
| `g_pcm[kMaxSamples]`, `g_pcmLen` | `audio_io` (capture), `cloud` (TTS write / ASR encode) | `audio_io` (playback), `turn` (gate) |
| `g_state`, `g_errorSinceMs` | `ui` | `main` (error dwell) |
| `g_offline`, `g_wifiAttempt`, `g_nextWifiTryMs` | `net` | `turn` (input gate), `failTurn` |
| `g_stamps`, `g_voiceActive` | `main` (arm), `audio_io`/`turn` (stamps) | `audio_io` (latency print) |
| `g_userText`, `g_replyText` | `turn` | `ui` (transcript) |
| `g_answerStartMs`, `g_lastAnswerMs`, `g_answerCount`, `g_answerSumMs` | `turn` | `ui` (transcript) |
| `g_reader`, `g_history` | `main` / `turn` | — |

The **single `g_pcm` buffer** is reused across the turn: capture writes it →
ASR encodes it **in place** to µ-law → TTS overwrites it with reply audio →
playback reads it. The ordering in `voiceTurn`/`handleTurn` guarantees each
stage finishes reading before the next overwrites (see [INTERNALS.md](INTERNALS.md)).

## Turn lifecycle

Two entry paths share one **`handleTurn`** chain:

```
VOICE  BtnA held → recordWhileHeld (mic, VAD) ─┐
                                               ├→ voiceTurn → ASR → handleTurn ┐
TEXT   serial line → parseTextIn ──────────────────────────────→ handleTurn ──┤
                                                                              │
                handleTurn:  llmTurn (stream) → ttsFetch → playbackCaptured ──┘
                state:       Listening → Thinking ───────────→ Replying → Idle
```

- **Voice** ([loop](../../firmware/src/main.cpp#L66) → [voiceTurn](../../firmware/src/turn.cpp#L106)):
  capture into `g_pcm`, gate out silence ([`shouldTranscribe`](../../firmware/src/audio.h#L48)),
  transcribe ([`asrTranscribe`](../../firmware/src/cloud.cpp#L128)), then `handleTurn`.
- **Text** ([loop](../../firmware/src/main.cpp#L66) → [`parseTextIn`](../../firmware/src/serial_protocol.h#L23) → handleTurn):
  the typed line is the user turn directly.
- **Shared tail** ([handleTurn](../../firmware/src/turn.cpp#L29)): build request from
  rolling history → `llmTurn` (streamed) → `ttsFetch` → `playbackCaptured`. The
  full reply is always on serial; a TTS failure degrades to text.

## Turn-state machine

One pure state machine ([states.h](../../firmware/src/states.h)) is the single
source of truth for "what is the device doing", rendered to the LCD by `ui`:

```
Idle ──Listen──▶ Listening ──Think──▶ Thinking ──Reply──▶ Replying ──Done──▶ Idle
  ▲                                      │ Fail
  └──────────── Done (after dwell) ◀──── Error
WifiLost (from any) ─▶ Offline ──WifiUp──▶ Idle      // input paused while Offline
```

Transitions are pure + host-tested ([`nextState`](../../firmware/src/states.h#L39),
`test_states`). `ui` maps each state to an LCD color + label, or — when
`SHOW_TRANSCRIPT` — to a small-font conversation transcript with answer-time.

## Audio & the shared bus

The Echo Base's **ES8311 codec** drives both mic and speaker over one shared
I2S bus, enabled by `cfg.external_speaker.atomic_echo` in [setup](../../firmware/src/main.cpp#L42).
Only one side can own the bus, so `playbackCaptured` ends the mic, begins the
speaker, plays, then restores the mic; [`ensureMicMode`](../../firmware/src/audio_io.cpp#L18)
is the recovery-safe reset used on any abort.

Capture is **continuous-queue** (never drained mid-capture) with VAD fed off the
real-time-filled portion — draining per chunk crashed M5's `mic_task`
(`i2s_stop` on an uninstalled port). See the note in
[recordWhileHeld](../../firmware/src/audio_io.cpp#L26).

## External services (v1, direct HTTPS)

| Stage | Provider | Format | Pure parser |
|-------|----------|--------|-------------|
| LLM | Anthropic Messages API (SSE stream) | JSON / `text/event-stream` | [sse.h](../../firmware/src/sse.h), [chat_api.h](../../firmware/src/chat_api.h) |
| ASR | Deepgram prerecorded | raw 8-bit µ-law POST | [ulaw.h](../../firmware/src/ulaw.h), [asr_api.h](../../firmware/src/asr_api.h) |
| TTS | ElevenLabs | `pcm_16000` (raw PCM16) | [tts_api.h](../../firmware/src/tts_api.h) |

TLS uses `setInsecure()` (no cert pinning) — acceptable only under the private
allowlist model. Keys live in the gitignored `config.h`.

## Concurrency model

The firmware is **single-threaded**: everything runs in the Arduino `loop()`
(core 1). Network calls are **synchronous** with bounded connect/read timeouts,
so the loop never hangs. The only other task is M5Unified's internal `mic_task`
(DMA), which the capture loop must not starve/drain mid-flight. A background-task
ASR pre-warm was tried in v1.4 and **reverted** (it crashed using mbedTLS from a
fresh task); connection pre-warm is deferred to the v2 server.

## Compile-time configuration

All behavior is config-driven via [config.example.h](../../firmware/src/config.example.h)
(copied to the gitignored `config.h`): Wi-Fi, the three API keys + models, the
persona, audio sizing, capture/ASR gates, VAD thresholds, and UI toggles
(`DEBUG_SERIAL`, `SHOW_TRANSCRIPT`). A few values are **compile-time** because
they size the static `g_pcm` buffer (`AUDIO_SAMPLE_RATE`, `REC_MAX_MS` →
`kMaxSamples`). In v2 the operational config moves server-side into the `Role`.

## Testing

The pure layer is fully host-tested (Unity, `pio test -e native`) — one suite per
header (parsers, framing, state machine, VAD, latency math, µ-law). The glue is
covered by compile + manual on-device DoD checks (mic/speaker, LCD, Wi-Fi, a live
round-trip), since it needs the board, real keys, and the network. See
[INTERNALS.md](INTERNALS.md) for the suite list.
