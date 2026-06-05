# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Current state

**v1 (Voice) shipped** on the firmware (`1.4.0`); **v2 (Server platform) is in progress** on the `v2-dev` branch. The `/server` Python package now exists (FastAPI + websockets); `/mcp` and `/console` do not yet. Build/run/test commands below.

### Build, run, test

**Server** (`/server`, Python — from the repo root):
```bash
python3 -m venv .venv && . .venv/bin/activate     # once
pip install -r server/requirements.txt            # deps (incl. pytest, ruff)
uvicorn pyramid_server.main:app --app-dir server --reload   # run the WSS server
pytest                                             # unit + contract + integration (tests/)
ruff check server tests                            # lint
```
Tests use **mock** LLM/ASR/TTS and a fake device — no external AI, no secrets in CI. Real keys live in `server/.env` (gitignored; see `server/.env.example`). `pyproject.toml` sets `pythonpath=["server"]` so tests import `pyramid_server`.

**Firmware** (`/firmware`, PlatformIO — from `firmware/`):
```bash
pio run                 # compile (env: atoms3r)
pio test -e native      # host-testable logic (FSM, VAD, codecs, framing)
pio run -t upload       # flash a connected board
pio device monitor      # serial @115200
```

**CI** (`.github/workflows/ci.yml`) runs `ruff` + `pytest` on every push/PR; `main`/`v2-dev` stays green.

The three specs are the source of truth and must be read before writing any code:
- [specification/MISSION.md](specification/MISSION.md) — what is being built and why, plus the hard principles and non-goals.
- [specification/ARCHITECTURE.md](specification/ARCHITECTURE.md) — components, protocols, message contracts, data model, external services.
- [specification/ROADMAP.md](specification/ROADMAP.md) — the v0–v3 plan; each version groups dotted phases `vA.B` (A = version, B = phase, e.g. `v1.2`), each with a **Goal**, a description, a **Tasks** list, and a **DoD**.

When asked to "implement `v1.2`" or "start v0", treat that phase's **DoD** as the acceptance criteria, its **Tasks** as the work list, and the ARCHITECTURE contracts as the interface to honor.

## What "Pyramid" is

A closed, private voice AI assistant running on M5Stack **AtomS3R + Echo Base** hardware. The device is deliberately **thin** (I/O + status screen only); all intelligence (LLM, later ASR/TTS/memory/MCP) lives in the cloud or on a server. The assistant speaks Ukrainian, has a configurable persona ("role"), and later remembers the user and shifts daily mood via a horoscope-derived "temperament". A simple, self-tailored analog of xiaozhi.

## Architecture in one pass

Three tiers that grow across versions: **device** (firmware) ↔ **server** (Python orchestrator + auth + console) ↔ **external AI + MCP services**.

Planned repository layout (create these directories as each version starts):
```
/firmware    # AtomS3R + Echo Base, C++/M5Unified — Arduino IDE in v0, PlatformIO from v1
/server      # Python: FastAPI + websockets, ASR→LLM→TTS orchestrator, auth, console
/mcp         # Python MCP servers: role, memory, knowledge_base, weather (v3)
/console     # minimal web UI for role configuration
/tests       # pytest: unit, contract, integration; fake device + mock LLM/ASR/TTS
/specification  # MISSION.md, ARCHITECTURE.md, ROADMAP.md
.github/workflows/ci.yml  # lint + tests on every push/PR
```
Storage is **SQLite** (accounts/devices/roles/memory) plus files for the knowledge base. Config and keys live in `.env` (never commit).

## Version progression (build in this order — do not skip ahead)

Each version is self-contained and ships on its own. Complexity is added **only by version**, never all at once.

- **v0 — Text chat over serial.** Device talks to a cloud LLM **directly** over HTTPS; I/O is text over USB-CDC serial (115200). No audio, no own server, no ASR/TTS. Persona prompt lives in firmware config. Firmware built in the **Arduino IDE**.
- **v1 — Voice.** Migrate firmware to **PlatformIO**; add I2S audio (16 kHz mono), push-to-talk. Build order is **TTS first** (Ph2: type in serial → spoken reply), **then ASR** (Ph3: full voice loop) — validate the output path before the harder input path. Still direct to cloud; serial stays a debug channel.
- **v2 — Server with role config.** Our own backend (WSS/FastAPI) sits between device and AI. The ASR→LLM→TTS loop moves server-side; device only streams audio/text. Adds the `Role` model, a web console, accounts, device activation by code, and an allowlist (closed access).
- **v3 — Memory, horoscope-temperament, MCP.** Long-term `MemoryItem` storage; an MCP client in the server with `role`/`memory`/`knowledge_base`/`weather` as MCP services; an astro engine (skyfield) computing daily transits → temperament dials.

## Contracts that cross tiers (keep these stable)

These are defined in ARCHITECTURE.md and are the integration seams — match them exactly when implementing either side.

- **WS device↔server (v2+):** device→server `hello{device_token,proto_ver,audio_fmt}`, `listen_start`, `audio`(bin), `listen_stop`, `text_in{text}`, `ping`; server→device `asr_partial{text}`, `asr{text}`, `reply{text}`, `text_out{text}`, `tts_audio`(bin), `tts_end`, `error{code,msg}`, `config_updated`, `restart`, `pong`. Audio is PCM16 16 kHz mono (OPUS is deferred). `error.code` is an enumerated set (see ARCHITECTURE §Error handling).
- **Activation (v2):** `POST /activate {device_id} → {code}`; admin binds the code in the console; device receives a `device_token`.
- **MCP (v3):** `role.persona.get()`, `memory.save/recall/list/clear`, `kb.search`, `weather.get`, and internal `temperament.today(role_id) → {energy,warmth,verbosity,speech_speed,pitch}`.
- **Data model:** `Account`, `Device`, `Role`, `MemoryItem`, `KBDoc` (see ARCHITECTURE.md §Data model for fields).

## Principles that constrain implementation

These come from MISSION.md and should override convenience when they conflict:

- **Intelligence off-device.** Never put persona logic, memory, or decision-making in the firmware. The device executes what the server/config tells it.
- **Config is the source of truth.** Behavior is fully defined by the role (firmware config in v0–v1, server `Role` from v2). Don't hardcode behavior that belongs in the role.
- **Simplicity first / incremental.** Don't pull v2/v3 concerns (server, MCP, memory, astro) into an earlier version. Each version works standalone.
- **Closed by default.** Reject unauthorized devices/users; access is via manual allowlist + activation code.
- **MCP is the one extension mechanism (v3+).** Role, memory, knowledge, and external services all plug in as MCP — don't invent parallel mechanisms.
- **Horoscope affects tone/voice only**, never competence or willingness to help. It is an experimental generative method for daily variation, not an astrological claim.

## Expected toolchain (once code exists)

Not yet present — these are the stacks the spec commits to, for when directories are created:
- **Server / MCP:** Python, FastAPI + websockets; later `skyfield` for the astro engine. Use a virtualenv (`.venv`) — already gitignored.
- **Firmware:** C++/M5Unified for AtomS3R + Echo Base. **Arduino IDE in v0**, then **PlatformIO from v1** (native build, libraries, on-host test env).
- **Testing (required per phase):** `pytest` for `/server` and `/mcp` — unit + **contract tests** that pin the WS/activation/MCP wire formats, plus integration tests over a **fake device** and **mock LLM/ASR/TTS** (no paid APIs in CI). Firmware host-testable logic uses PlatformIO's native test env. CI runs lint + tests on every push/PR; `main` stays green. See ARCHITECTURE §Testing and CI.
- **External services:** OpenAI-compatible / DeepSeek / Qwen / Claude (LLM), Whisper or cloud (ASR, Ukrainian), cloud voice or Piper (TTS, Ukrainian).

Each ROADMAP phase ships with the tests that encode its DoD. When you add the first server/firmware code, also add the concrete build/run/test commands to this file.

## Workflow skills

A spec → issues → execute → release pipeline lives in `.claude/skills/` (ported from the sibling pc-pet project, retargeted to pyramid):
- **`/upload-issues <version-issues-file>`** — split a version's phases into `PYR-xxx` GitHub issues with `vN::` labels and dependencies; writes `specification/roadmap/implementation/vN-github-report.md`.
- **`/execute-issues <label>`** — implement each issue in dependency order: code → `pytest`/`pio test` validation → one commit per issue → close → `specification/roadmap/implementation/vN-execution-report.md`. Tests ship with each feature; contracts and ARCHITECTURE.md stay in sync.

Issue files live under `specification/roadmap/implementation/` (`vN-issues.md`), mirroring the modernization-demo layout.
- **`/release-version <x.y.z>`** — bump version, write `RELEASE.txt`, commit, annotated-tag, push. **Version notation `A.B.C`**: `A` = global/roadmap version (v0→0, v1→1, v2→2, v3→3), `B` = phase within that version (`v1.1`→B=1), `C` = post-release fix on that phase. So roadmap phase `vA.B` → semver `A.B.0`, and a fix after it bumps `C` (e.g. v0.3 → `0.3.0`, its streaming fix → `0.3.1`; v1.1 → `1.1.0`). Releases are cut per phase. **Never bump the version without explicit user confirmation** — no auto-bump (including `/execute-issues` on completion).
