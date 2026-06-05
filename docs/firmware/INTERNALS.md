# Firmware Internals (v1.4)

A line-by-line walkthrough of how the firmware works, with code references, plus
a complete file inventory. Pair with [FIRMWARE_ARCHITECTURE.md](FIRMWARE_ARCHITECTURE.md) for the
structural view. Paths link into [`firmware/src/`](../../firmware/src/).

---

## 1. Boot — [`setup()`](../../firmware/src/main.cpp#L42)

1. `M5.config()` with `cfg.external_speaker.atomic_echo = true` so M5Unified
   brings up the Echo Base **ES8311** codec (I2C + I2S); `M5.begin(cfg)`.
2. Start in **mic mode**: `M5.Speaker.end(); M5.Mic.begin();` (the shared bus is
   switched per turn later).
3. `Serial.begin(115200)` and wait briefly for USB-CDC to enumerate.
4. [`setState(Offline)`](../../firmware/src/ui.cpp#L88) — render the initial screen
   (not connected yet).
5. [`connectWiFi()`](../../firmware/src/net.cpp#L12) — blocking, bounded by
   `kWifiTimeoutMs`; on success fires `WifiUp` → **Idle**, else stays **Offline**
   (the loop keeps retrying).

## 2. The main loop — [`loop()`](../../firmware/src/main.cpp#L66)

Each tick, in order:

1. `M5.update()` — refresh button/SoC state.
2. [`serviceWiFi()`](../../firmware/src/net.cpp#L34) — non-blocking reconnect
   supervisor; flips `g_offline` and fires `WifiLost`/`WifiUp`.
3. **Error dwell** — if `g_state == Error` and `kErrorDwellMs` elapsed since
   `g_errorSinceMs`, fire `Done` → Idle (never stuck on Error).
4. **Text path** — drain serial bytes through [`g_reader`](../../firmware/src/line_reader.h);
   a completed line → [`parseTextIn`](../../firmware/src/serial_protocol.h#L23) →
   [`handleTurn`](../../firmware/src/turn.cpp#L29).
5. **Voice path** — if `BtnA` is pressed: arm `g_stamps`/`g_voiceActive`, stamp
   `pressMs`, [`recordWhileHeld()`](../../firmware/src/audio_io.cpp#L26), stamp
   `recEndMs`, [`voiceTurn()`](../../firmware/src/turn.cpp#L106), then **drain a
   still-held button** so a VAD-ended turn doesn't immediately re-trigger.

## 3. Voice turn — [`voiceTurn()`](../../firmware/src/turn.cpp#L106)

1. If `g_offline`, bail (input paused).
2. **Noise/length gate** — [`analyzePcm`](../../firmware/src/audio.h#L33) +
   [`shouldTranscribe`](../../firmware/src/audio.h#L48); too short/quiet → `Done`,
   no network call.
3. Fire `Think`, then [`asrTranscribe`](../../firmware/src/cloud.cpp#L128).
   `g_stamps.asrMs` is stamped on **every** outcome (so re-prompts don't
   mis-report `asr=0`).
4. On ASR failure: if Wi-Fi is down → [`failTurn`](../../firmware/src/turn.cpp#L17)
   (Offline); else [`rePrompt`](../../firmware/src/turn.cpp#L92) (spoken nudge).
   On low confidence → `rePrompt`.
5. Set `g_answerStartMs = recEndMs` (so the voice answer time spans ASR+LLM+TTS)
   and call `handleTurn(transcript)`.

### Capture — [`recordWhileHeld()`](../../firmware/src/audio_io.cpp#L26)

- Fire `Listen`; ensure mic mode.
- Loop while `BtnA` held and buffer not full: queue 512-sample chunks into
  `g_pcm` via `M5.Mic.record()` (**never drained mid-capture**).
- VAD: feed the [`Endpointer`](../../firmware/src/vad.h#L26) the peak of each chunk
  the real-time DMA has *already filled* (tracked by elapsed time) — ends on a
  trailing pause (`VAD_HANGOVER_MS`) or the `RECOG_PATIENCE_MS` cap.
- One clean `while (M5.Mic.isRecording()) delay(1)` drain at the end; log
  `end=pause|release`.

## 4. Text turn / shared tail — [`handleTurn()`](../../firmware/src/turn.cpp#L29)

1. If `g_offline`, bail.
2. Stamp the answer clock (typed: now; voice: already set), set `g_userText`,
   clear `g_replyText`, fire `Think`.
3. Build the request: [`g_history.turns()`](../../firmware/src/history.h) + the new
   user turn → [`llmTurn`](../../firmware/src/cloud.cpp#L318) (streams tokens to
   serial). `g_stamps.llmMs` recorded.
4. On success: print `[stats]`, commit the turn to history, set `g_replyText`
   (and re-render in transcript mode), then [`ttsFetch`](../../firmware/src/cloud.cpp#L22)
   → [`playbackCaptured`](../../firmware/src/audio_io.cpp#L87). Record the
   `[answer]` last/avg.
5. On TTS failure: degrade to on-serial text, fire `Done`. On LLM failure:
   `failTurn`.

### LLM — [`llmTurn`](../../firmware/src/cloud.cpp#L318) / [`llmAttempt`](../../firmware/src/cloud.cpp#L193)

- `llmTurn` = bounded retry + backoff around `llmAttempt`.
- `llmAttempt` POSTs [`buildChatRequest`](../../firmware/src/chat_api.h#L33)
  (persona as `system`, history, `stream:true`) and reads the **SSE stream**:
  raw bytes → [`Dechunker`](../../firmware/src/sse.h#L53) → lines →
  [`extractSseData`](../../firmware/src/sse.h#L75) →
  [`parseStreamEvent`](../../firmware/src/sse.h#L99). Captures time-to-first-token
  and [`Usage`](../../firmware/src/chat_api.h#L62); tokens print as they arrive.

### TTS — [`ttsFetch`](../../firmware/src/cloud.cpp#L22)

[`clampUtf8`](../../firmware/src/tts_api.h#L24) bounds the reply to fit the buffer,
[`buildTtsRequest`](../../firmware/src/tts_api.h#L36) builds the body, POSTs to
ElevenLabs `output_format=pcm_16000`, and reads raw PCM16 into `g_pcm` (handles
Content-Length and chunked via the same `Dechunker`).

### ASR — [`asrTranscribe`](../../firmware/src/cloud.cpp#L128)

Encodes `g_pcm` to **8-bit µ-law in place** ([`ulawEncode`](../../firmware/src/ulaw.h#L15))
— halving the upload to fix Deepgram `408 SLOW_UPLOAD` — POSTs `encoding=mulaw`
with bounded retry, and parses with [`parseAsrTranscript`](../../firmware/src/asr_api.h#L22).

## 5. Failure & recovery — [`failTurn`](../../firmware/src/turn.cpp#L17)

Centralizes mid-turn aborts: log, [`ensureMicMode`](../../firmware/src/audio_io.cpp#L18)
(restore the shared bus), then → **Offline** if Wi-Fi dropped (input paused,
`serviceWiFi` recovers) or **Error** (auto-returns to Idle after the dwell). The
bounded per-stage timeouts (`kHttpReadMs`/`kAsrReadMs`/`kTtsReadMs`) guarantee a
stalled stage reaches here instead of hanging.

## 6. Instrumentation

- **`[latency]`** (voice only, printed in [`playbackCaptured`](../../firmware/src/audio_io.cpp#L87)):
  `press→speak` split via [`computeLatency`](../../firmware/src/timing.h#L43) into
  speech / asr / llm / tts / other, from the stamps in
  [`VoiceStamps`](../../firmware/src/timing.h#L20).
- **`[stats]`** (per turn): LLM time-to-first-token, total, token usage.
- **`[answer]`** (per spoken reply): last + session-average answer time
  (request-ready → first audio); also shown on the LCD in transcript mode.

## 7. The shared `g_pcm` lifecycle

One static buffer ([app_state](../../firmware/src/app_state.cpp), sized by
`kMaxSamples`) is reused across a voice turn — safe because each stage finishes
before the next overwrites:

```
recordWhileHeld  ──writes PCM16──▶ g_pcm
asrTranscribe    ──reads, then encodes µ-law IN PLACE──▶ g_pcm   (recording no longer needed)
ttsFetch         ──overwrites with reply PCM16──▶ g_pcm
playbackCaptured ──reads──▶ speaker
```

---

## File inventory

### Glue modules (`namespace app`)

| File | Lines | What's inside |
|------|------:|---------------|
| [main.cpp](../../firmware/src/main.cpp) | 107 | `setup()` (board/Wi-Fi/serial bring-up) and `loop()` (Wi-Fi service, error dwell, text path, push-to-talk path). Wiring only. |
| [app_state.h](../../firmware/src/app_state.h) | 81 | `extern` declarations of the shared `g_*` globals + all tuning constants (`k*Ms`, retries, backoff, `kErrorDwellMs`, `kMaxSamples`). |
| [app_state.cpp](../../firmware/src/app_state.cpp) | 31 | The single definition of each shared global (incl. the `g_pcm` buffer and `g_history`). |
| [log.h](../../firmware/src/log.h) | 30 | `app::logf` — `printf`-style status logging gated by `DEBUG_SERIAL` (named off the C library's `::logf`). |
| [ui.h](../../firmware/src/ui.h) | 18 | Public UI API: `renderState`, `setState`, `applyEvent`. |
| [ui.cpp](../../firmware/src/ui.cpp) | 96 | State→color/label render; `renderState` (state screen or `renderTranscript`), `setState` (stamps Error dwell), `applyEvent` (drives `nextState`). |
| [net.h](../../firmware/src/net.h) | 10 | `connectWiFi`, `serviceWiFi`. |
| [net.cpp](../../firmware/src/net.cpp) | 63 | Initial blocking connect; non-blocking reconnect supervisor with backoff; drives Wi-Fi state events. |
| [audio_io.h](../../firmware/src/audio_io.h) | 13 | `ensureMicMode`, `recordWhileHeld`, `playbackCaptured`. |
| [audio_io.cpp](../../firmware/src/audio_io.cpp) | 120 | Mic capture (continuous-queue + VAD) and speaker playback on the shared ES8311 bus; bus-restore safety; the `[latency]` print. |
| [cloud.h](../../firmware/src/cloud.h) | 40 | `Attempt` struct; `llmTurn`, `ttsFetch`, `asrTranscribe` prototypes. |
| [cloud.cpp](../../firmware/src/cloud.cpp) | 335 | The three HTTPS clients: ElevenLabs TTS, Deepgram ASR (µ-law + retry), Anthropic LLM (`llmAttempt` SSE stream + `llmTurn` retry). |
| [turn.h](../../firmware/src/turn.h) | 26 | `failTurn`, `handleTurn`, `rePrompt`, `voiceTurn`. |
| [turn.cpp](../../firmware/src/turn.cpp) | 148 | Orchestration: the shared LLM→TTS chain, the voice gate→ASR→chain, the re-prompt nudge, and centralized failure recovery. |

### Pure logic (`namespace pyramid`, header-only, host-tested)

| File | Lines | What's inside |
|------|------:|---------------|
| [states.h](../../firmware/src/states.h) | 85 | `TurnState` / `TurnEvent` enums, `nextState` transition, `label`. |
| [vad.h](../../firmware/src/vad.h) | 58 | `Endpointer` — pause-based end-of-utterance (silence threshold + hangover + patience cap). |
| [audio.h](../../firmware/src/audio.h) | 56 | `samplesForMs`/`capSamples`, `PcmStats` + `analyzePcm` (peak/clip), `shouldTranscribe` (capture gate). |
| [timing.h](../../firmware/src/timing.h) | 55 | `VoiceStamps`, `LatencyBreakdown`, `elapsed` (wrap-safe), `computeLatency`. |
| [sse.h](../../firmware/src/sse.h) | 134 | `Dechunker` (HTTP chunked decode), `extractSseData`, `StreamEvent` + `parseStreamEvent` (Anthropic SSE). |
| [chat_api.h](../../firmware/src/chat_api.h) | 112 | `buildChatRequest`, `parseChatReply`, `Usage`, `isRetryableHttpStatus`. |
| [tts_api.h](../../firmware/src/tts_api.h) | 46 | `clampUtf8` (UTF-8 boundary-safe), `buildTtsRequest`. |
| [asr_api.h](../../firmware/src/asr_api.h) | 65 | `parseAsrTranscript` (Deepgram JSON), `parseHost`. |
| [ulaw.h](../../firmware/src/ulaw.h) | 37 | `ulawEncode` — G.711 µ-law for the ASR upload. |
| [history.h](../../firmware/src/history.h) | 59 | `Turn` + `History` — short rolling conversation window. |
| [backoff.h](../../firmware/src/backoff.h) | 24 | `backoffDelayMs` — capped exponential backoff. |
| [line_reader.h](../../firmware/src/line_reader.h) | 51 | `LineReader` — non-blocking serial line assembly. |
| [serial_protocol.h](../../firmware/src/serial_protocol.h) | 32 | `TextIn` + `parseTextIn` — serial `text_in` parse. |

### Config & project

| File | Lines | What's inside |
|------|------:|---------------|
| [config.example.h](../../firmware/src/config.example.h) | 119 | Config **template** — copy to `config.h` (gitignored) and fill in Wi-Fi, the 3 API keys + models, persona, audio sizing, capture/ASR gates, VAD, and `DEBUG_SERIAL`/`SHOW_TRANSCRIPT`. |
| `config.h` | — | The real config (gitignored; holds keys). Not committed. |
| [platformio.ini](../../firmware/platformio.ini) | — | `[env:atoms3r]` (device) + `[env:native]` (host tests); board flags + `lib_deps`. |
| [test/](../../firmware/test/) | — | One Unity suite per pure header: `line_reader, chat_api, history, backoff, sse, audio, tts, asr, ulaw, vad, timing, states`. Run with `pio test -e native`. |

> Line counts are a v1.4 snapshot and will drift; the structure is the durable part.
