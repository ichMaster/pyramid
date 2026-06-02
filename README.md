# Pyramid

**Version 0.2.0** · A closed, private voice AI assistant on M5Stack hardware.

Pyramid is a self-tailored analog of [xiaozhi](https://github.com/78/xiaozhi-esp32):
a living, configurable persona that runs on an **AtomS3R + Echo Base**, speaks
Ukrainian, and (in later versions) remembers the user, shifts its daily mood by
a horoscope-derived "temperament", and reaches external services through MCP.
The device is deliberately **thin** — I/O and a status screen only; all the
intelligence (LLM, and later ASR/TTS/memory/MCP) lives in the cloud or on a
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
| **v0** | Text chat over serial — device ↔ cloud LLM directly, text over USB-CDC | **in progress** (v0.1–v0.2 done; v0.3 pending) |
| **v1** | Voice — I2S audio, push-to-talk; TTS first, then ASR; PlatformIO | planned |
| **v2** | Server with role config — own backend (WSS/FastAPI), console, accounts, activation | planned |
| **v3** | Memory, horoscope-temperament, MCP layer | planned |

## Repository layout

```
firmware/        # AtomS3R + Echo Base, C++/M5Unified (Arduino IDE in v0, PlatformIO from v1)
specification/   # MISSION.md, ARCHITECTURE.md, ROADMAP.md + roadmap/implementation issues
                 # server/, mcp/, console/, tests/ are created as each version starts
```

## Current state (v0.2)

The firmware boots the board, connects Wi-Fi, and turns the USB-CDC port into a
line channel: a typed line becomes a `text_in` event, is sent to a cloud LLM
over direct HTTPS (persona from config), and the reply is printed back — a full
text chat in Ukrainian over serial. Status logging is gated by `DEBUG_SERIAL`.
Rolling history and Wi-Fi auto-reconnect land in v0.3.

Build and flash instructions are in [firmware/README.md](firmware/README.md).
Quick host test of the pure serial logic:

```sh
cd firmware/test
c++ -std=c++17 -I../pyramid test_line_reader.cpp -o test_line_reader && ./test_line_reader
```

## Releases

The current version lives in [VERSION](VERSION); release notes are in
[RELEASE.txt](RELEASE.txt). Version mapping: v0 → 0.1.0, v1 → 0.2.0,
v2 → 0.3.0, v3 → 1.0.0.

## License

See [LICENSE](LICENSE).
