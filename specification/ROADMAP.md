# Roadmap — M5Stack Voice AI Chatbot ("Pyramid")

Four self-contained versions, built in order: **v0** text → **v1** voice → **v2** server with a role → **v3** memory/horoscope/MCP. Versions are numbered from 0; phases inside a version are numbered `vA.B` (A = version, B = phase), e.g. `v1.2`. Each phase lists a goal, the work, a task list, and a Definition of Done (DoD). Every phase ships with the automated tests that encode its DoD (see ARCHITECTURE §Testing and CI). Firmware toolchain: **v0 in the Arduino IDE**, migrating to **PlatformIO from v1**.

---

## v0 — Text chat over serial

The cheapest possible start: the device talks to a cloud LLM directly, with input and output as plain text over the USB-CDC serial link. No audio, no own server, no ASR/TTS. The aim is to get the full conversational loop working in text and to establish a debug channel that survives into every later version (`debug_serial`). Depends on: nothing — this is the foundation.

### v0.1 — Device skeleton and serial

**Goal:** the device powers on, connects over USB, and echoes in the serial monitor — the I/O channel before any AI exists.

The Arduino-IDE sketch boots the board, brings up Wi-Fi from a config header, and turns the USB-CDC port into a line-oriented text channel. A typed line becomes a `text_in`; everything the firmware wants to log goes to the same port, gated by a `debug_serial` flag so it can be quietened later.

**Tasks:**
- Create the firmware sketch in the **Arduino IDE** with M5Unified; select the AtomS3R + Echo Base board and flash a blinking/boot-message baseline.
- Initialize the board: display on, button, status line on the LCD.
- Bring up Wi-Fi from credentials in a config header (`wifi_ssid`, `wifi_pass`); print connection state to serial.
- Open USB-CDC serial at **115200**; implement a non-blocking line reader (buffer until `\n`).
- Parse a completed line into a `text_in` event; route all logs through a `logf()` helper gated by `debug_serial`.
- Echo received lines back so the round-trip is visible.

**DoD:** you type a line in the serial monitor, it reaches the parser, and the firmware writes a response line back; Wi-Fi state is visible in the log.

### v0.2 — Text chat loop

**Goal:** you type in serial and get a real LLM reply, in Ukrainian, straight from the device.

The firmware makes a direct HTTPS call to a cloud LLM, sending the persona system prompt (from the firmware config) plus the user's `text_in`, and prints the model's `reply` back to serial. The API key lives in the firmware config (acceptable only under the private allowlist model — see ARCHITECTURE §Security).

**Tasks:**
- Add an HTTPS client (TLS) and a minimal JSON builder/parser for the chosen LLM's chat API.
- Store the persona system prompt, model name, endpoint, and API key in the firmware config header.
- Build the request: `system = persona`, `messages = [{user: text_in}]`; send and read the response.
- Extract `reply.text` and print it to serial; surface HTTP/JSON errors as a readable line.
- Keep the call synchronous for now (one turn at a time); show a "thinking…" log while waiting.

**DoD:** a full back-and-forth dialogue in Ukrainian, directly from the device over serial.

### v0.3 — Quality and UX

**Goal:** a text chat that holds up in normal use — short memory, and graceful handling of the network being imperfect.

This phase makes the loop robust: a small rolling history so replies have context, sane behavior on timeouts/API errors, and automatic Wi-Fi recovery. Optional LCD hints make the device's state legible without the serial monitor.

**Tasks:**
- Keep a short in-RAM conversation history (last N turns) and include it in each LLM request; trim by turn count or a rough token budget.
- Handle request timeouts and API errors: bounded retry, then a clear error line; never hang the loop.
- Detect Wi-Fi loss and reconnect with backoff; pause input while offline and report status.
- (Optional) Show coarse states/hints on the LCD: idle / thinking / error.

**DoD:** the chat works reliably across typical scenarios — multi-turn context holds, transient failures recover, and a dropped Wi-Fi link comes back on its own.

---

## v1 — Voice

Add a microphone, a speaker, and a voice loop; the device still talks to cloud AI directly. The build order is deliberately **output first, then input**: get TTS playback working from a typed prompt (v1.2) before tackling the harder speech-recognition path (v1.3), so each half is validated in isolation. Serial remains the debug channel throughout. Depends on: v0 (the LLM loop and serial are reused).

### v1.1 — Audio I/O and PlatformIO migration

**Goal:** the device can capture and play audio — the hardware foundation for voice — on a real build system.

This phase moves the firmware off the Arduino IDE onto **PlatformIO** (so audio code, multiple source files, libraries, and on-host tests are manageable) and brings up the Echo Base I2S path in both directions with push-to-talk.

**Tasks:**
- Migrate the firmware to **PlatformIO**: add `platformio.ini` (board, framework, libraries), move sources into `firmware/`, confirm an identical build/flash; keep a one-line note of where the `.ino` moved.
- Add a **native test environment** (`pio test -e native`) for host-testable logic (parsers, framing, state machine).
- Bring up **I2S capture** at 16 kHz mono through the Echo Base mic; record into a PCM16 buffer while the button is held (push-to-talk).
- Bring up **I2S playback** through the Echo Base speaker; play a test clip on release.
- Add basic level/clipping checks and a fixed maximum record duration.

**DoD:** the project builds and flashes from PlatformIO, and press-records / release-plays-back works through the Echo Base.

### v1.2 — TTS output (spoken replies)

**Goal:** you type in serial and *hear* the reply spoken — the output path proven before any ASR exists.

The device takes the LLM `reply` from the v0 loop, sends it to a cloud Ukrainian TTS, and plays the returned audio through the Echo Base. Input is still text over serial; only the output becomes voice.

**Tasks:**
- Add a cloud **TTS** client (Ukrainian voice); send `reply.text`, receive PCM16/encoded audio.
- Decode/stream the audio into the I2S playback path from v1.1.
- Wire the pipeline `text_in` (serial) → LLM → TTS → playback; keep `text_in`/`reply` in serial for debugging.
- Handle TTS timeouts/errors with a spoken-or-logged fallback; respect a max reply length.

**DoD:** a typed prompt produces a spoken Ukrainian reply from the device.

### v1.3 — ASR input (full voice loop)

**Goal:** you speak and hear a reply — the complete voice exchange.

With playback already solid, this phase adds the input half: push-to-talk capture feeds a cloud Ukrainian ASR, whose transcript drives the same LLM→TTS chain from v1.2.

**Tasks:**
- Capture held-button audio (v1.1) into a buffer; on release, send it to a cloud **ASR** (Ukrainian).
- Feed the ASR transcript into the LLM (same persona config) → existing TTS → playback.
- Keep `text_in`/`reply` mirrored to serial for debugging the whole chain.
- Handle empty/failed recognition gracefully (re-prompt rather than calling the LLM with noise).

**DoD:** a full spoken exchange in Ukrainian, end to end, from the device.

### v1.4 — States and UX

**Goal:** the voice interaction is legible, survives a flaky network, and feels responsive.

Add pause-based end-of-utterance so the user need not time the button perfectly, surface the turn state on the LCD, harden the loop against Wi-Fi loss and stage timeouts, and shave the felt latency that the v1.3 timing instrumentation exposed (ASR dominates the wait, mostly per-call connection overhead).

**Tasks:**
- Detect end-of-utterance by pause (silence threshold + hangover), bounded by `recog_patience` from the config.
- Drive LCD states through the turn: listening / thinking / replying / error.
- Handle Wi-Fi loss and per-stage (ASR/LLM/TTS) timeouts mid-turn; recover to idle cleanly.
- Tune the push-to-talk vs. pause-detection interplay so both feel natural.
- **Pre-warm the ASR connection** (carried over from the v1.3 latency analysis, rec. #2): open/establish the TLS connection to the ASR host at button-press so the handshake overlaps the user's speech instead of sitting on the critical path after release. Targets the ~3 s fixed ASR overhead measured in v1.3 (ASR ≈ 3043 ms + 1.14 × clip_ms; ~61 % of post-speech latency).
- **Complete the latency instrumentation** (carried over, rec. #5): attribute the ASR call duration even when it returns empty / low-confidence (today `asrMs` is only stamped on success, so re-prompt turns mis-report `asr=0` and dump the time into `other`). Stamp `asrMs` right after `asrTranscribe` regardless of outcome so the `[latency]` breakdown stays trustworthy as latency work proceeds.

**DoD:** the voice chat works reliably, the current state is always visible on the screen, the `[latency]` breakdown is correct on every turn (including re-prompts), and pre-warming measurably cuts the ASR stage's fixed overhead versus the v1.3 baseline.

---

## v2 — Server with role configuration

Our own backend now sits between the device and the AI. The ASR→LLM→TTS loop moves server-side; the device becomes a streaming client. The character becomes a configurable **Role** — including its **Name** and authored **Canon** — edited in a web console, and the device gains its first **emotion face** (emoji tier) driven by the server. Access is closed: accounts, device activation by code, and an allowlist. Depends on: v1 (the audio + turn loop move from the device to the server).

### v2.1 — Server proxy

**Goal:** the same chat as v1, but routed through our own server instead of direct cloud calls.

A FastAPI + websockets server terminates a single duplex WSS channel and runs the turn orchestrator; the device only streams audio/text and renders results. The serial path keeps working as a local debug client through the server.

**Tasks:**
- Stand up a **WSS** server (FastAPI + `websockets`), TLS-terminated.
- Implement the device↔server contract: device→ `hello`, `listen_start`, `audio`(bin), `listen_stop`, `text_in`; server→ `asr`/`asr_partial`, `reply`, `text_out`, `tts_audio`(bin), `tts_end`, `error`, `config_updated`, `restart` (see ARCHITECTURE §WS device↔server).
- Move the **ASR→LLM→TTS** orchestration server-side; stream per stage.
- **Sentence-streaming TTS** (carried over from the v1.3 latency analysis, rec. #3): synthesize speech clause/sentence by sentence as the LLM streams tokens, instead of waiting for the full reply, so the first `tts_audio` chunk leaves the server while the model is still generating. This overlaps the TTS stage (~20 % of post-speech latency in v1.3) with LLM generation; it lives on the server because the v1.2 device-side attempt was choppy under single-thread TLS+audio contention.
- **Early/streaming playback** (carried over, rec. #4): the firmware plays each `tts_audio` chunk as it arrives over WSS (paced by the server) rather than buffering the whole reply — so audio starts on the first chunk. The server paces the stream, sidestepping the on-device underrun that forced buffered playback in v1.2.
- Convert the firmware into a WSS client that streams mic audio and plays returned TTS chunk-by-chunk; serial bridges as a local debug client.
- Add per-stage timeouts and the enumerated `error.code` set.

**DoD:** the device runs end-to-end through our server, with the same voice experience as v1 and a lower time-to-first-audio — playback starts on the first streamed TTS chunk rather than after the full reply is synthesized.

### v2.2 — Role, Name & Canon

**Goal:** the assistant is a configurable, named character with an authored canon.

Introduce the `Role` model and build the system prompt from it. The role carries the character's **Name** and an authored **Canon** (a character bible — lore, traits, behavioral rules), plus persona, LLM choice, and voice parameters. Canon is hand-written content, not a facet engine (MISSION non-goal).

**Tasks:**
- Define `Role{name, canon, persona, lang, voice{pitch,speed}, recog_patience, model, memory_type}` (see ARCHITECTURE §Data model).
- Assemble the system prompt from **canon + persona**; pass voice params to TTS and `recog_patience` to end-of-utterance.
- Keep short in-session history (per `Session`) and feed it to the LLM with windowing.
- Allow LLM selection per role; load the active role at connection time.
- Author Name + Canon in the console (next phase) — the canon is the single source of the character's identity.

**DoD:** changing the role (incl. Name and Canon) visibly changes the assistant's identity, behavior, and voice.

### v2.3 — Web console

**Goal:** manage the assistant without reflashing.

A minimal web UI edits the role fields and persists them; saving signals the device to pick up the change.

**Tasks:**
- Build a web UI (the `/console`) with the role fields, Save / Reset.
- Persist roles in **SQLite**; load the active role on connect.
- On save, emit `config_updated` / `restart` to the bound device so the change applies.
- Validate inputs (ranges for pitch/speed/`recog_patience`, model allowlist).

**DoD:** the role is edited in the console and applied after a restart, no firmware changes needed.

### v2.4 — Closed access

**Goal:** a private, access-controlled service — only known devices and users get in.

Run behind TLS, add console login, bind devices by activation code, and reject everything not on the allowlist. Developed and tested **locally / on the LAN** (the device points at a local server) — the public host + automated deploy come in v2.5.

**Tasks:**
- Be deployment-ready: TLS-terminated HTTPS/WSS and a `dev`/`prod` split via `.env` (the public host + CI/CD land in v2.5).
- Add accounts (`pass_hash` = argon2id) and console login (cookie/JWT); rate-limit login and `/activate`.
- Implement device activation: `POST /activate {device_id} → {code}` (single-use, short TTL); admin binds the code; device stores its `device_token` in NVS.
- Enforce the allowlist: unknown `device_token` → `error{unauthorized}` and the socket closes; support token revocation.

**DoD:** only authorized devices and users have access; an unbound device is rejected.

### v2.5 — Deployment & hosting (CI/CD)

**Goal:** the server runs on public hosting and ships automatically — and because it comes after v2.4, the first public exposure already enforces accounts + activation + the allowlist.

Containerize the server and add a pipeline that builds, tests, and deploys it to **Fly.io** on a tagged release. Through v2.1–v2.4 the device talked to a **local / LAN** server; now it connects to a live **WSS** endpoint over real TLS. Fly.io fits this shape: containers, native WebSockets, a **persistent volume** for the SQLite DB + KB files, and managed TLS.

**Tasks:**
- **Containerize** `/server` (Dockerfile: slim Python + uvicorn; the FastAPI app also serves the console, so one image). Pin deps; keys/`.env` are never baked into the image.
- **Fly.io app:** a small always-on machine, a **persistent volume** for `pyramid.db` + the knowledge-base files, managed TLS on a real domain, and secrets (LLM/ASR/TTS keys, allowlist) via `fly secrets`.
- **GitHub Actions CD:** extend the existing CI (lint + `pytest`) with a deploy job triggered on a **version tag** (or manual `workflow_dispatch`) — build → push image to GHCR → `flyctl deploy` (auth via a `FLY_API_TOKEN` secret). `main` stays green; deploys stay deliberate.
- **Data & migrations:** a startup schema init/migration; the SQLite volume persists across deploys; document backup/restore.
- **Cutover:** point the device's WSS endpoint at the live domain and **retire `setInsecure()`** (validate the real certificate); health check + platform rollback.
- **Security gate — runs before every deploy, blocks on failure:** the v2.4 access-control tests re-run as a release gate (unauthorized `device_token` rejected + socket closed, single-use / short-TTL activation, token revocation, allowlist enforced, login + `/activate` rate-limits), the enumerated `error.code` set is honored, and oversized / malformed WS frames are rejected — plus deploy-time scans: secrets-leak scan (e.g. gitleaks) over the repo + built image, dependency / image vulnerabilities (`pip-audit`, Trivy), Python SAST (`bandit`), and a TLS posture check (no plaintext WS/HTTP in `prod`; valid cert). Wire these into the CD job so a failure stops the deploy.
- *(Optional)* a `staging` Fly app for a pre-prod smoke test — skip if not needed at this scale.

**DoD:** pushing a tagged server release auto-builds, tests, and deploys to Fly.io; the device connects to the live WSS endpoint over TLS; data persists across deploys; a rollback is one command; **the deploy is gated on the security suite passing (auth, secrets, dependencies, TLS) — any failure blocks the release.**

### v2.6 — Emotion channel + emoji face

**Goal:** the character shows how it feels — a first on-screen emotion face, decided by the server.

The server's **emotion engine** classifies the turn's emotion (an LLM-emitted tag or a server classification of the reply, from the Canon + mood) and sends an `EmotionFrame` to the device; the device renders it as an **emoji / simple glyph** on the LCD. This is the cheapest renderer in the ladder — its job is to prove the emotion channel end to end. The contract and emotion enum are locked here so later sprite tiers are a renderer swap. No on-device emotion decision (intelligence off-device).

**Tasks:**
- Define the emotion enum + `EmotionFrame{emotion, intensity, gaze, accent_color?, speaking, ttl_ms}` and add the `emotion` WS message to the device↔server contract (ARCHITECTURE §WS) **and its contract test**.
- Server: derive the emotion per turn (from canon + reply); emit one `EmotionFrame` per turn / state change; relax to neutral after `ttl_ms`.
- Device: `EmojiRenderer` behind an `IFaceRenderer` interface — map emotion → emoji/glyph on the 128×128 LCD; idle/neutral when no frame.
- Keep it Echo-Base-only (no halo); the halo and sprite tiers come later (see EMOTION_FACE.md).

**DoD:** the face on the screen reflects the assistant's emotion each turn, driven by the server; emotion never alters competence.

---

## v3 — Memory, horoscope-temperament, and MCP

A living persona: it remembers the user across sessions, shifts its daily mood by horoscope, and reaches services uniformly through MCP. Depends on: v2 (server, role, accounts, console). MCP becomes the single extension mechanism — role, memory, knowledge, and external services all plug in the same way.

### v3.1 — Long-term memory

**Goal:** the assistant remembers things from previous sessions.

Persist salient facts as `MemoryItem`s scoped to the account, recall them during a turn, and let the user inspect/clear memory from the console. The role's `memory_type` gates the behavior.

**Tasks:**
- Add `MemoryItem{id, account_id, text, meta, embedding?, ts}` storage in SQLite (vectors via `sqlite-vec` when `memory_type=longterm`).
- Save salient facts during conversation; recall by query before assembling the prompt (keyword first, embeddings within the phase).
- Surface memory in the console: list and clear; honor `memory_type ∈ {none, session, longterm}`.

**DoD:** the assistant recalls facts stated in earlier sessions, and memory can be viewed/cleared.

### v3.2 — MCP layer

**Goal:** the assistant uses services uniformly, calling tools itself.

Introduce an MCP client in the server and move `role`, `memory`, `knowledge_base` behind MCP; integrate tool-calling into the LLM turn and add a user knowledge base (optionally `weather`).

**Tasks:**
- Add an **MCP client** to the server; define transport per service (in-process/stdio for internal, HTTP/SSE for networked).
- Move `role`, `memory`, `knowledge_base` into MCP services exposing the contracts in ARCHITECTURE §MCP tools/resources.
- Integrate the **tool loop** into the turn: bounded iterations, tool results fed back, degraded reply on tool error/timeout.
- Build the knowledge base: ingest user docs, chunk + embed, `kb.search(query,k)`.
- (Optional) add a `weather` MCP service.

**DoD:** the agent calls memory, knowledge, and weather through MCP on its own.

### v3.3 — Horoscope-temperament

**Goal:** the character's tone and voice vary, livingly, day to day.

Fix a natal chart on the role; an astro engine computes daily transits into temperament dials that color the prompt and the TTS voice — without touching competence.

**Tasks:**
- Add a fixed `natal_chart` (JSON snapshot) to the role at creation.
- Build the astro engine (**skyfield**): once per local day, compute transits → dials (energy, warmth, verbosity, speech speed, pitch), each bounded; cache per day.
- Inject a temperament block into the system prompt and map the dials onto TTS pitch/speed.
- Bias the **emotion baseline** by temperament (e.g. higher warmth → more `warm`/`affection`; higher energy → brighter) so the face shifts with the day — colouring presentation only.
- Isolate from competence: dials never change willingness or correctness; expose `temperament.today(role_id)` internally.

**DoD:** tone, voice, and the emotion face noticeably differ across days without degrading answer quality.

### v3.4 — Persona integration

**Goal:** one coherent living character, drawing on everything at once.

Combine the role canon, the day's temperament, recalled memory, and MCP results into a single reply with a clear priority order, and open the door to custom MCP endpoints.

**Tasks:**
- Assemble one prompt from: role canon + temperament block + recalled memory + available MCP tools.
- Define and enforce priority/reconciliation (canon and competence outrank temperament; memory informs but doesn't override the persona).
- Allow connecting custom MCP endpoints to a role.
- End-to-end check that all parts cooperate while one consistent character is presented outward.

**DoD:** role, temperament, memory, and MCP all contribute to one reply, and the assistant still reads as a single coherent persona.

### v3.5 — Web search (optional)

**Goal:** the assistant can look things up on the open internet, within strict bounds.

A `web_search` MCP service lets the agent answer from fresh web results when a role allows it — off by default, treated as untrusted data, and kept clear of personal/memory information. Full boundaries in WEB_SEARCH.md.

**Tasks:**
- Add a `web_search` MCP service: `web.search(query, k) → results[]` and `web.fetch(result_id) → page`, with `fetch` limited to ids from this turn's prior `search` results.
- Per-role toggle in the console (`Role.web_search`), **off by default**.
- Treat page content as **untrusted data** — never follow embedded instructions/links.
- Keep personal/memory data out of queries; rate-limit and log searches and fetches.

**DoD:** when enabled, the assistant answers from fresh web results **with sources**; when disabled, it has no internet access beyond the LLM's own knowledge.

### v3.6 — Sprite face (animation)

**Goal:** upgrade the emoji face to an animated, layered character face — a renderer swap, not a rewrite.

Behind the same `EmotionFrame` contract and emotion enum from v2.6, replace `EmojiRenderer` with a sprite renderer: procedural layered sprites (eyes/brows/mouth/halo) composited per emotion recipe, with an idle loop (blink/breathe), expression crossfade, and **audio-level lip-sync** from the TTS the device plays. Authored character art (a "Lili"-style pack) is a later asset swap over the same scheme. See EMOTION_FACE.md.

**Tasks:**
- Implement the layer model + asset manifest (EMOTION_FACE.md) and an `IconRenderer` (procedural sprite pack) behind `IFaceRenderer`.
- Idle loop (blink, breathe, micro gaze drift), ~150–250 ms crossfade, intensity-scaled expressiveness.
- Lip-sync: derive an amplitude envelope from playback (RMS) → mouth visemes while `speaking`.
- On Echo Pyramid base hardware, drive the LED halo from the same `EmotionFrame`. (Artist "Lili" sprite pack: a later asset-only swap.)

**DoD:** the face animates (idle motion + lip-synced mouth) and crossfades between emotions, using the same channel as the emoji face.

---

## Mapping of protocols and contracts

- Serial protocol (text) and `text_in`/`reply` — v0.1, v0.2.
- On-device audio (I2S) and the PlatformIO migration — v1.1.
- WS protocol and message contracts (control + audio + `text_in`/`text_out`) — v2.1.
- Activation and auth contracts — v2.4.
- MCP contracts (`role`, `memory`, `knowledge_base`, `weather`) — v3.1, v3.2.
- `web_search` MCP contract (`web.search`, `web.fetch`) — v3.5.
- Temperament contract (`temperament.today`) — v3.3.
- `EmotionFrame` (emotion-face) contract — v2.6 (emoji); same contract reused by the sprite face — v3.6.
- Name + Canon in the `Role` — v2.2.

## Hardware roadmap

The device is a family, not one SKU (ARCHITECTURE §Hardware variants): **v1 → AtomS3R + Echo Base** (ES8311 audio, 128×128 LCD; no halo/mic-array). Later targets — **AtomS3R + Echo Pyramid base** (adds a mic array + AEC and a WS2812 halo) and **Core S3** (larger screen/resources) — add capabilities the firmware detects and uses when present. The audio format, WS contract, Role/Canon, and `EmotionFrame` are identical across boards.

## Deferred (beyond v0–v3)

Offline wake word, OPUS streaming and barge-in, music and arbitrary custom MCP as official, speaker recognition, OTA, role templates and AI Optimize. (The **emotion face**, **multi-board support**, and **web search** are no longer deferred — they are scheduled: face emoji v2.6 / sprite v3.6, boards per the Hardware roadmap, web search v3.5. The artist "Lili" sprite pack remains a later asset-only swap over v3.6.)
