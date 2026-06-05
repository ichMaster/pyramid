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

For where development currently stands, see **[STATUS.md](STATUS.md)**.

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
- [specification/ROADMAP.md](specification/ROADMAP.md) — the v0–v4 plan (phases, goals, tasks, DoD).

## Build & flash

The firmware is **PlatformIO** (install [PlatformIO Core](https://docs.platformio.org/en/latest/core/)
→ the `pio` CLI). From `firmware/`:

1. **Configure.** `cp src/config.example.h src/config.h`, then fill in your
   credentials (`config.h` is gitignored — it holds your keys, never commit it):
   - **Wi-Fi** — `WIFI_SSID`, `WIFI_PASS`
   - **LLM** (Anthropic) — `LLM_API_KEY` (`sk-ant-…`), `LLM_MODEL`, `LLM_PERSONA`
   - **TTS** (ElevenLabs) — `TTS_API_KEY`, `TTS_VOICE_ID`
   - **ASR** (Deepgram) — `ASR_API_KEY`
2. **Compile** — `pio run` (first build fetches M5Unified + ArduinoJson).
3. **Flash** — `pio run -t upload` (add `--upload-port /dev/cu.usbmodemXXXX` if
   the port isn't auto-detected; close the serial monitor first).
4. **Monitor** — `pio device monitor` (serial @115200).

The full config-field reference and the host tests live in
[firmware/README.md](firmware/README.md).

> **Security:** API keys are compiled into the firmware and are extractable from
> a flashed device — acceptable only under the private allowlist model; never
> publish such firmware.

## Using it

- **Talk:** hold **button A**, speak Ukrainian, then release — *or* just stop
  talking and it ends on the pause (VAD) — and hear the spoken reply.
- **Type:** in the serial monitor, type a line and press Enter for the same
  reply, spoken (a debug channel that stays available in every version).
- **Screen:** the LCD shows the turn state (`idle / listening / thinking /
  replying / error / offline`). Set `SHOW_TRANSCRIPT true` in `config.h` to
  instead show the conversation text + answer-time (`last`/`avg`) in a small,
  Cyrillic-capable font.
- **Serial telemetry:** each turn logs `[stats]` (LLM tokens + latency),
  `[latency]` (press→speak breakdown), and `[answer]` (last/average answer time).

## Repository layout

```
firmware/        # AtomS3R + Echo Base, C++/M5Unified (PlatformIO)
specification/   # MISSION.md, ARCHITECTURE.md, ROADMAP.md + roadmap/implementation issues
                 # server/, mcp/, console/, tests/ are created as each version starts
```

## More

- **[STATUS.md](STATUS.md)** — current development state: what works now, what's next.
- **[specification/ROADMAP.md](specification/ROADMAP.md)** — the full v0–v4 plan.
- **[RELEASE.txt](RELEASE.txt)** — release notes · **[VERSION](VERSION)** — current version.

## License

See [LICENSE](LICENSE).
