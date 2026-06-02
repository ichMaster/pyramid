# Architecture — M5Stack Voice AI Chatbot ("Pyramid")

## Overview

Three tiers: **device** (AtomS3R + Echo Base) ↔ **server** (orchestrator + auth + console) ↔ **external AI and MCP services**. The architecture evolves across versions: in **v0** the device talks to a cloud LLM directly, with input/output as text over USB serial (no audio, no server); in **v1** voice is added (microphone/speaker, ASR→TTS), still direct; in **v2** our own server with a role sits between the device and the AI; in **v3** memory, horoscope-temperament, and the MCP layer are added.

## System diagram

```
                    v0–v1 direct path: device → cloud AI over HTTPS
                    (v0: LLM, text only   ·   v1: + ASR / TTS)
        +-----------------------------------------------------------+
        |                                                           v
+-------+----------+                              +----------------------------+
|      Device      |                              |        External AI         |
| AtomS3R + EchoB. |                              |     LLM  .  ASR  .  TTS     |
|   -- thin I/O -- |                              +----------------------------+
| v0  USB serial   |                                            ^
| v1  + I2S audio  |        WSS (TLS) . v2+                      | HTTPS REST/stream
| v2  + WSS client |     audio + text + JSON ctrl               | (server -> AI, v2+)
| LCD . button     | <----------------------------+  +----------+-----------------+
| (no persona)     |     asr / reply / tts_audio  +->|          Server            |
+------------------+                                 |       (Python, v2+)        |
                                                     |  auth -> turn orchestrator |
                                                     |   (ASR -> LLM -> TTS)      |
                                                     |  web console . SQLite      |
                                                     +-------------+--------------+
                                                                   | MCP . JSON-RPC (v3)
                                                                   v
                                                     +----------------------------+
                                                     |      MCP services (v3)     |
                                                     |  role . memory . astro     |
                                                     |  knowledge_base . weather  |
                                                     +----------------------------+
```

The tiers grow by version: in **v0–v1** the device calls cloud AI directly (no
server of ours); from **v2** the device speaks WSS to our server, which runs the
turn (ASR→LLM→TTS) and holds auth, roles, and storage; in **v3** the server fans
out to MCP services. The device stays thin in every version — I/O and a status
screen, no persona logic.

## Components

- **Device (firmware).** AtomS3R+Echo Base: Wi-Fi, status screen, button. In v0 the I/O is text over USB-CDC serial; audio I/O (I2S, 16 kHz mono) from v1; WSS client from v2. No persona logic.
- **Server (Python).** Auth gateway → turn orchestrator (ASR→LLM→TTS) → MCP client; storage for accounts, devices, roles, memory; web configuration console.
- **MCP layer (from v3).** Separate services: `role`, `memory`, `knowledge_base`, plus `weather`, `music`, `custom`. The agent calls them all the same way.
- **Astro engine (from v3).** The role's natal chart + daily transits → temperament dials in the prompt.
- **Console.** A web UI for role fields, viewing/clearing memory, binding devices; Save/Reset plus a restart signal.

## Protocols

- **Serial (v0):** USB-CDC, a text channel — a line of text as `text_in`, the `reply` and status logs back; it remains a debug channel later too (flag `debug_serial`).
- **Device ↔ server (from v2):** WSS (TLS), one duplex channel — JSON control + binary audio frames + `text_in`/`text_out`. Audio is PCM16 16 kHz mono (OPUS later).
- **Device activation/onboarding:** HTTPS (get the server address, hand over an activation code, receive a token).
- **Console ↔ server:** HTTPS REST with authorization (cookie/JWT).
- **Server ↔ MCP:** MCP over JSON-RPC (stdio or HTTP/SSE).
- **Server ↔ external AI:** HTTPS REST/streaming (ASR, LLM, TTS).
- **v0–v1 exception:** the device talks to cloud AI directly over HTTPS, without our own server (v0 — LLM by text only; v1 — plus ASR/TTS).

## Contracts

### WS device↔server
- device→server: `hello{device_token, proto_ver, audio_fmt}`, `listen_start`, `audio`(bin), `listen_stop`
- device→server: also `text_in{text}` (text mode / serial bridge), `ping`
- server→device: `asr_partial{text}` (interim), `asr{text}` (final), `reply{text}` (may stream as deltas), `text_out{text}`, `tts_audio`(bin), `tts_end`, `error{code,msg}`, `config_updated`, `restart`, `pong`
- `proto_ver` is negotiated in `hello`; the server rejects an unknown major with `error{code:"proto_unsupported"}`. The `error.code` values are the enumerated set in §Error handling and resilience.

### Activation (HTTPS)
- `POST /activate {device_id} → {code}`
- the admin binds `code` to an account in the console → the device receives a `device_token`

### MCP tools/resources
- `role`: `persona.get() → {name, persona, lang, voice{pitch,speed}, recog_patience, model}`
- `memory`: `memory.save(text, meta)`, `memory.recall(query, k) → items[]`, `memory.list()`, `memory.clear()`
- `knowledge_base`: `kb.search(query, k) → passages[]`
- `weather`: `weather.get(location) → {...}`
- `astro` (internal): `temperament.today(role_id) → {energy, warmth, verbosity, speech_speed, pitch}`

### LLM call
- input: `system = persona + temperament_block` + `messages[]` + `tools = MCP`
- output: `reply.text` plus possible tool calls (memory/kb/weather)

### Data model
- `Account{id, login, pass_hash, created_at}` — `pass_hash` via argon2id.
- `Device{id, token, account_id, role_id, status, last_seen}` — an account may own several devices.
- `Role{id, name, persona, lang, voice{pitch,speed}, recog_patience, model, memory_type, natal_chart, updated_at}` — `memory_type ∈ {none, session, longterm}`.
- `Session{id, device_id, account_id, started_at, ended_at}` — one connection / turn-loop.
- `Message{id, session_id, role:"user"|"assistant", text, ts}` — short rolling history; window/truncation policy in §Sessions and history.
- `MemoryItem{id, account_id, text, meta, embedding?, ts}` — long-term; `embedding` present only when `memory_type=longterm`.
- `KBDoc{id, account_id, title, chunks[{text, embedding}]}`.
- `natal_chart` is a fixed JSON snapshot (timestamp + geo + computed positions) written once at role creation.

## Turn lifecycle

A turn is half-duplex (barge-in is deferred to "beyond v0–v3"). Voice path (v1+):

```
button↓ → listen_start → audio(bin)… → button↑ / VAD → listen_stop
  → ASR → asr_partial* / asr → LLM (stream) → reply deltas → TTS (stream) → tts_audio(bin)… → tts_end
```

Text path (v0 / serial): `text_in` → LLM → `reply` / `text_out`. End-of-utterance in v1 is the button release; v1 later adds pause-based VAD bounded by `Role.recog_patience`.

**Streaming for latency.** The pipeline streams at every stage. ASR emits `asr_partial` interims and one final `asr`. The LLM streams token deltas (the `reply` deltas); the server buffers them to **clause/sentence boundaries** and hands each completed phrase to TTS, so speech starts before the LLM finishes. TTS returns audio incrementally, sent as `tts_audio` frames and closed by `tts_end`. Target first-audio < ~1.5 s after `listen_stop`; per-stage budgets are in §Error handling and resilience.

## Device states

`boot → wifi_connecting → idle → listening → thinking → replying → idle`, with `offline` (no Wi-Fi/server) and `error` reachable from any state. The LCD reflects the current state (v1). The device holds no persona logic — transitions are driven by the button and by server messages.

## Error handling and resilience

Enumerated `error.code`: `wifi_lost`, `server_unreachable`, `proto_unsupported`, `unauthorized`, `rate_limited`, `asr_failed`, `llm_timeout`, `llm_failed`, `tts_failed`, `internal`.

- **Per-stage timeouts:** ASR ≤ 5 s, LLM first token ≤ 8 s, TTS first audio ≤ 3 s. On breach the server emits `error` and the device returns to `idle`.
- **Reconnection:** the device→server WSS reconnects with exponential backoff (0.5→8 s, jittered); on Wi-Fi loss the device shows `offline` and retries. An interrupted turn is abandoned, not queued.
- **Server unreachable (v0–v1 direct path):** the device surfaces `error` on LCD/serial and drops the turn; conversation history is preserved.
- **Idempotency:** `listen_start`/`listen_stop` received outside the expected state are ignored; `restart` always returns the device to `boot`.

## Sessions and history

Short conversation history is per-`Session`, held in RAM on the live connection and trimmed to a rolling window (last N turns or a token budget) before each LLM call. Persistence follows `Role.memory_type`: `none`/`session` keep history only for the live session; `longterm` additionally writes salient facts via `memory.save` (v3). Audio frames are never persisted. A session ends on disconnect, `restart`, or idle timeout.

## Security, auth, and secrets

- **Console** auth is cookie/JWT over HTTPS; `pass_hash` is argon2id; login and `/activate` are rate-limited.
- **Activation:** `/activate` returns a single-use `code` with a short TTL; the admin binds it in the console; the device stores the issued `device_token` in NVS. Tokens are revocable — clearing/rotating one invalidates the device.
- **Allowlist:** only bound devices/accounts may connect; an unknown `device_token` → `error{unauthorized}` and the socket closes.
- **Secrets** (LLM/ASR/TTS keys) live in server `.env` from v2. In v0–v1 the key sits in firmware config and is extractable from the device — acceptable only under the private allowlist model; never publish such firmware.

## MCP runtime (v3)

- **Transport choice:** internal services (`role`, `memory`, `astro`) run in-process or over stdio; networked/third-party ones (`weather`, `custom`) use HTTP/SSE. The agent calls all of them identically.
- **Tool loop:** an LLM turn may call tools; the server caps it at a small max-iteration count, feeds tool results back as tool messages, and on tool error/timeout returns a degraded reply instead of failing the turn.
- **Supervision & auth:** the server launches and monitors stdio MCP processes; HTTP MCP endpoints authenticate with a per-service token. `astro`'s `temperament.today` is internal and never exposed to third parties.

## Memory and knowledge base (v3)

- **Long-term memory.** `memory.recall(query, k)` is semantic: `MemoryItem.embedding` is computed at `memory.save` time and queried by vector similarity. Vectors live in SQLite via a vector extension (e.g. `sqlite-vec`), scoped by `account_id` so memory never crosses accounts. v3 Ph1 may ship **keyword-only** recall first and add embeddings within the same phase once the store exists. `memory.clear` deletes an account's items and their vectors.
- **Knowledge base.** `KBDoc` is chunked on ingest (≈200–500 token chunks with small overlap) and each chunk is embedded; `kb.search(query, k)` returns the top passages by similarity. Source files live on disk under the account; chunk vectors sit alongside the memory store.
- **Embeddings.** A single embedding model is configured server-side (in `.env`) and used for both memory and KB so their vectors are comparable; changing it requires a re-index.
- **Gated by role.** `Role.memory_type` controls writes/recall: `none` (no recall), `session` (in-session history only, no writes), `longterm` (save + recall across sessions). The knowledge base is independent of `memory_type`.

## Cross-cutting concerns

- **Protocol versioning:** `proto_ver` (major.minor) in `hello`; the server supports the current major and rejects unknown majors.
- **Observability:** structured logs keyed by `session_id`/`turn_id`; per-stage latency (ASR/LLM/TTS) is recorded; serial stays the on-device debug channel (`debug_serial`).
- **Deployment:** server + MCP behind nginx/Caddy with TLS; `dev`/`prod` separated by `.env`; stdio MCP servers are child processes of the server, HTTP MCP servers run standalone.
- **Privacy/retention:** audio is processed in memory and discarded; sessions/history are ephemeral unless `memory_type=longterm`; `memory.clear` wipes long-term memory for an account.

## External services

- **LLM:** OpenAI-compatible / DeepSeek / Qwen / Claude (via a key in the server config; in v0 — on the device).
- **ASR:** Whisper (locally on the server) or cloud; Ukrainian.
- **TTS:** Ukrainian — a cloud voice or Piper.
- **Weather/Music:** external APIs via MCP (v3).
- **Infrastructure:** VPS/cloud, TLS certificate, reverse proxy (nginx/Caddy).

## Horoscope-temperament (v3)

The role's natal chart is fixed at creation (a JSON snapshot). The astro engine (skyfield) computes daily transits to it **once per local day** — recomputed at the role's local midnight, cached, and a turn keeps the temperament it started with even across a day boundary. Transits map to dials — energy, warmth, verbosity, speech speed, and voice pitch — each normalized to a bounded range (e.g. 0–1 or a small signed band). The dials are injected into the system prompt and drive the TTS voice; they do not affect competence or willingness to help. The transit→dial mapping is a tunable function and may start coarse. This is an experimental generative method for variation, not an astrological claim.

## Stack and repository layout

```
/firmware    # AtomS3R + Echo Base, C++/M5Unified — Arduino IDE in v0, PlatformIO from v1
/server      # Python: FastAPI + websockets, orchestrator, auth, console
/mcp         # Python MCP servers: role, memory, knowledge_base, weather
/console     # web UI for role configuration (minimal)
/tests       # automated tests (pytest): unit, contract, integration; fakes & mocks
/specification        # MISSION.md, ARCHITECTURE.md, ROADMAP.md
.github/workflows/ci.yml   # lint + tests on every push/PR
```

Storage — SQLite (accounts/devices/roles/memory) + files for the knowledge base; config and keys — in `.env`.

**Firmware toolchain.** v0 is built and flashed from the **Arduino IDE** (M5Unified, simplest start). From **v1 the firmware migrates to PlatformIO** (audio, multiple source files, libraries, and reproducible builds) and stays there for v2–v3.

## Testing and CI

Automated tests are part of every version, not an afterthought: each ROADMAP phase ships with tests that encode its DoD, and `main` stays green.

- **Unit tests** (pytest) for server logic — prompt assembly (persona + temperament + history), role loading, history windowing/truncation, error mapping, the MCP tool loop, and the astro dial mapping.
- **Contract tests** pin the wire formats so device and server cannot drift: the WS message set (§WS device↔server), the activation flow, and each MCP tool schema (§MCP tools/resources). Changing a contract must change its test.
- **Fakes instead of hardware and paid APIs:** a **fake device** drives the WS/serial protocol in tests (the serial bridge doubles as this harness), and **mock LLM/ASR/TTS** adapters return canned output so the turn pipeline runs deterministically and offline. External AI is never called in CI.
- **Integration tests** run a full turn end-to-end against the fakes: `text_in → reply` (v0/v2) and `audio → tts_end` (v1+), asserting state transitions and the error paths (timeout, unauthorized, server_unreachable).
- **Firmware** logic that can be hosted (the serial parser, the state machine, framing) is unit-tested on-host — PlatformIO's native test env from v1; hardware I/O is covered by the manual DoD checks in ROADMAP.
- **CI** (`.github/workflows/ci.yml`) runs lint + the full pytest suite on every push/PR; merges require green, with coverage tracked for `/server` and `/mcp`.
