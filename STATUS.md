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
| **v2** | Server with role config — WSS/FastAPI, console, accounts, activation, **deployment (Fly.io)**, + boards (Echo Pyramid halo, Cardputer ADV) | ⏭️ next |
| **v3** | Memory, MCP, horoscope-temperament, web search, sprite face, **vision/camera**, **Core S3** | 🔜 planned |

### v1 phases (done)

- **v1.1** — PlatformIO migration + `pio test -e native` host tests; push-to-talk
  record→playback on the ES8311 Echo Base.
- **v1.2** — cloud **TTS** (spoken replies), buffered for smooth playback.
- **v1.3** — cloud **ASR** + the full voice loop; µ-law upload + turbo TTS for latency.
- **v1.4** — **states & UX**: a turn-state machine drives the LCD, pause-based
  end-of-utterance (VAD), mid-turn Wi-Fi / per-stage-timeout recovery, per-turn
  latency + answer-time stats, and an optional on-screen transcript.

## Next: v2 — server with role configuration

Our own backend (**WSS / FastAPI**) sits between the device and the AI; the
ASR→LLM→TTS loop moves **server-side** and the device becomes a streaming client.
The persona becomes a configurable **Role** (including its **Name** and authored
**Canon**), edited in a **web console**; access is closed (accounts, device
activation by code, allowlist). The server is **developed/tested locally** (the
device points at a LAN server) through v2.4, then **deployed to public hosting
(Fly.io) with automated CI/CD** in v2.5; the device gains its first **emoji
face** in v2.6.

v2 also begins the **hardware family**: the Echo Pyramid base extends the emotion
channel to its LED halo (v2.7), the all-in-one M5StickS3 lands with extra-button
gestures + a richer UI (v2.8), and the Cardputer ADV adds on-device keyboard
input (v2.9). v3 carries the family further — vision/camera on the AtomS3R Camera
(v3.7) and Core S3 with onboard camera + a larger sprite face (v3.8). See
[specification/ROADMAP.md](specification/ROADMAP.md) §Hardware roadmap for the
full board matrix.

Carried forward into v2 from the v1 work:
- **Latency wins** that need server-side pacing — streaming ASR, sentence-streaming
  TTS, early/streaming playback (a device-side pre-warm was tried in v1.4 and
  reverted; it belongs server-side).
- The richer **Лілі** Name + Canon, which becomes the server-side Role.

See [specification/ROADMAP.md](specification/ROADMAP.md) §v2 for the phase plan and DoDs.

## Versioning & releases

Versions follow **`A.B.C`** — `A` = roadmap version (v0→0, v1→1, v2→2, v3→3),
`B` = phase within it (`v1.4` → `1.4.0`), `C` = a post-release fix on that phase.
Releases are cut **per phase**; v1 shipped `1.1.0 → 1.2.0 → 1.3.0 → 1.4.0`. Full
notes are in [RELEASE.txt](RELEASE.txt).
