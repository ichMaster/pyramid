# Pyramid — Development Status

**Current version:** `1.4.0` — **v1 (Voice) is complete.** The device is still
**direct-to-cloud** (its own server arrives in v2). Default persona: **Піраміда**
(a terse Ukrainian helper).

## Where it stands

The full voice loop runs on the device: **hold the button, speak Ukrainian, hear
a spoken reply** — mic → Deepgram **ASR** → Claude (streamed, with rolling
history, retry, Wi-Fi recovery) → ElevenLabs **TTS** → Echo Base speaker. Typing
over USB serial is an equivalent text path / debug channel. The turn is legible
(LCD state machine), forgiving (pause-based end-of-utterance), and resilient
(clean mid-turn recovery), with per-turn latency + answer-time instrumentation.

## Versions

| Version | Theme | Status |
|---------|-------|--------|
| **v0** | Text chat over serial — device ↔ cloud LLM directly, USB-CDC | ✅ complete (v0.1–v0.3) |
| **v1** | Voice — audio I/O, TTS, ASR, states/UX | ✅ complete (v1.1–v1.4) |
| **v2** | Server platform — WSS/FastAPI, Role/Canon, console, **emoji face**, closed access, **deploy (Fly.io)**, **active listening** | ⏭️ next |
| **v3** | Intelligence & MCP — memory, MCP layer, **agent orchestration**, horoscope-temperament, persona, web search, **sprite face** | 🔜 planned |
| **v4** | Multi-session & administration — **multi-session hub** (shared resources) + **session-admin console** | 🔜 planned |
| **v5** | Devices & presence — Echo Pyramid+halo, M5StickS3, Cardputer, **camera/vision**, Core S3 | 🔜 planned |
| **v6** | Media understanding & translation — **image / audio / video → text** (describe + translate) via a multimodal LLM | 🔜 planned |
| **v7** | Bots & clients — **Telegram bot**, **web voice client + face**, **Meshtastic** LoRa bot | 🔜 planned |

### v1 phases (done)

- **v1.1** — PlatformIO migration + `pio test -e native` host tests; push-to-talk
  record→playback on the ES8311 Echo Base.
- **v1.2** — cloud **TTS** (spoken replies), buffered for smooth playback.
- **v1.3** — cloud **ASR** + the full voice loop; µ-law upload + turbo TTS for latency.
- **v1.4** — **states & UX**: a turn-state machine drives the LCD, pause-based
  end-of-utterance (VAD), mid-turn Wi-Fi / per-stage-timeout recovery, per-turn
  latency + answer-time stats, and an optional on-screen transcript.

## What's next (v2–v7)

**v2 — Server platform.** Our own backend (**WSS / FastAPI**) sits between the
device and the AI; the ASR→LLM→TTS loop moves **server-side** and the device
becomes a streaming client. The persona becomes a configurable **Role** (Name +
authored **Canon**), edited in a **web console**, and the device gains its first
on-screen **emoji face** (v2.4). Access is closed (accounts, activation by code,
allowlist), and it's deployed locally through v2.5, then **to Fly.io with
automated CI/CD** (v2.6); an optional hands-free **active-listening** mode lands
in v2.7.

**v3 — Intelligence & MCP.** The mind: long-term **memory**, the **MCP layer**,
**agent orchestration** (delegate to / control other agents via MCP),
horoscope-**temperament**, persona integration, bounded **web search**, and the
animated **sprite face** (the richer renderer of the v2.4 emotion channel).

**v4 — Multi-session & administration.** The server becomes a true
**multi-session hub** — many devices/clients at once, each its own session, all
sharing one set of per-account resources (Role, pooled providers, and the v3
memory / MCP / temperament) — plus a **session-admin console**. Comes after v3 so
the shared mind already exists to share and observe.

**v5 — Devices & presence.** The M5Stack **board family** — the LED **halo**
(Echo Pyramid), M5StickS3 (two-button gestures + richer UI), Cardputer v1.1 &
ADV (keyboard), **camera/vision** (AtomS3R Camera), and Core S3 (which extends
the v3.7 sprite face to its bigger screen). See
[ROADMAP](specification/ROADMAP.md) §Hardware roadmap for the board matrix.

**v6 — Media understanding & translation.** Hand the assistant an **image,
audio clip, or short video** and it **describes / translates** it to text via a
multimodal LLM — generalizing the v5.4 camera path to audio + video, with
cross-language translation. Server-side; exposed as `image`/`audio`/`video`
inputs + a `media` MCP tool.

**v7 — Bots & clients.** Front-ends on the same server: a **Telegram bot**
(text / voice notes / photos), a **web voice client** that renders the face in
the browser, and a **Meshtastic** LoRa bot — each a thin bridge to the server's
Role/LLM pipeline (intelligence stays server-side).

Carried forward into v2 from the v1 work:
- **Latency wins** that need server-side pacing — streaming ASR, sentence-streaming
  TTS, early/streaming playback (a device-side pre-warm was tried in v1.4 and
  reverted; it belongs server-side).
- The richer **Лілі** Name + Canon, which becomes the server-side Role.

See [specification/ROADMAP.md](specification/ROADMAP.md) §v2–§v6 for the phase plans and DoDs.

## Versioning & releases

Versions follow **`A.B.C`** — `A` = roadmap version (v0→0, v1→1, v2→2, v3→3),
`B` = phase within it (`v1.4` → `1.4.0`), `C` = a post-release fix on that phase.
Releases are cut **per phase**; v1 shipped `1.1.0 → 1.2.0 → 1.3.0 → 1.4.0`. Full
notes are in [RELEASE.txt](RELEASE.txt).
