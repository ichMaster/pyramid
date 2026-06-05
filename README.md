# Pyramid

**Version 1.4.0** · A closed, private voice AI assistant on M5Stack hardware.

Pyramid is a self-tailored analog of [xiaozhi](https://github.com/78/xiaozhi-esp32):
a living, configurable persona that runs on an **AtomS3R + Echo Base**, speaks
Ukrainian, and (in later versions) remembers the user, shifts its daily mood by
a horoscope-derived "temperament", and reaches external services through MCP.
The device is deliberately **thin** — I/O and a status screen only; all the
intelligence (LLM, ASR, TTS, and later memory/MCP) lives in the cloud or on a
server.

> Private by design — for the author and a close circle, not a public service.
> Users and devices are added by an allowlist and bound with an activation code.

## Architecture in one pass

Three tiers that grow across versions:

```
device (firmware)  <->  server (Python orchestrator + auth + console)  <->  external AI + MCP
  thin I/O, screen        ASR -> LLM -> TTS, roles, storage                  LLM / ASR / TTS / tools
```

The device stays thin in every version; behavior is defined by a configurable
**role**, never hardcoded on-device.

- [specification/MISSION.md](specification/MISSION.md) — what is being built and why; principles and non-goals.
- [specification/ARCHITECTURE.md](specification/ARCHITECTURE.md) — components, protocols, message contracts, data model.
- [specification/ROADMAP.md](specification/ROADMAP.md) — the v0–v3 plan (phases, goals, tasks, DoD).

## Version progression

Complexity is added **only by version** — each ships standalone, built in order:

| Version | Theme | Status |
|---------|-------|--------|
| **v0** | Text chat over serial — device ↔ cloud LLM directly, text over USB-CDC | **complete** (v0.1–v0.3) |
| **v1** | Voice — I2S audio, push-to-talk; TTS, then ASR; states/UX; PlatformIO | **complete** (v1.1–v1.4) |
| **v2** | Server with role config — own backend (WSS/FastAPI), console, accounts, activation | planned (next) |
| **v3** | Memory, horoscope-temperament, MCP layer | planned |

## Repository layout

```
firmware/        # AtomS3R + Echo Base, C++/M5Unified (Arduino IDE in v0, PlatformIO from v1)
specification/   # MISSION.md, ARCHITECTURE.md, ROADMAP.md + roadmap/implementation issues
                 # server/, mcp/, console/, tests/ are created as each version starts
```

## Current state (v1.4 — full voice loop)

**Hold the button, speak Ukrainian, and hear a spoken reply** — the complete
voice exchange runs on the device: mic → Deepgram **ASR** → Claude (streamed,
with rolling history, retry, Wi-Fi recovery) → ElevenLabs **TTS** (`pcm_16000`)
→ I2S playback on the Atomic Echo Base. Typing a line over USB serial is an
equivalent text path / debug channel.

v1 is complete across four phases:
- **v1.1** — PlatformIO migration + `pio test -e native` host tests; push-to-talk
  record→playback on the ES8311 Echo Base.
- **v1.2** — cloud TTS (spoken replies), buffered for smooth playback.
- **v1.3** — cloud ASR + the full voice loop; µ-law upload + turbo TTS for latency.
- **v1.4** — states & UX: a turn-state machine drives the LCD, pause-based
  end-of-utterance (VAD), mid-turn Wi-Fi/timeout recovery, per-turn latency +
  answer-time stats, and an optional on-screen transcript.

The default persona is **Піраміда** (a terse Ukrainian helper); the device is
still direct-to-cloud — its own server arrives in v2.

Build and flash instructions are in [firmware/README.md](firmware/README.md).
Run the pure-logic host tests:

```sh
cd firmware && pio test -e native
```

## Releases

The current version lives in [VERSION](VERSION); release notes are in
[RELEASE.txt](RELEASE.txt). Versions follow **`A.B.C`** — `A` = roadmap version
(v0→0, v1→1, v2→2, v3→3), `B` = phase within it (`v1.4` → `1.4.0`), `C` =
post-release fix. Releases are cut per phase: v1 shipped as 1.1.0 → 1.2.0 →
1.3.0 → 1.4.0.

## License

See [LICENSE](LICENSE).
