# Roadmap — M5Stack Voice AI Chatbot ("Pyramid")

Seven self-contained versions, built in order: **v0** text → **v1** voice → **v2** server platform → **v3** intelligence & MCP → **v4** multi-session & devices → **v5** devices & media → **v6** bots & clients. Versions are numbered from 0; phases inside a version are numbered `vA.B` (A = version, B = phase), e.g. `v1.2`. Each phase lists a goal, the work, a task list, and a Definition of Done (DoD). Every phase ships with the automated tests that encode its DoD (see ARCHITECTURE §Testing and CI). Firmware toolchain: **v0 in the Arduino IDE**, migrating to **PlatformIO from v1**.

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

## v2 — Server platform

Our own backend now sits between the device and the AI. The ASR→LLM→TTS loop moves server-side; the device becomes a streaming client. The character becomes a configurable **Role** — including its **Name** and authored **Canon** — edited in a web console, and gains its first on-screen **emotion face** (the emoji tier, driven by the server's emotion channel). Access is closed (accounts, device activation by code, allowlist), and the server is deployed to public hosting (CI/CD). It also gains a **simple cross-session memory** — a persisted rolling summary so a new session picks up where the last left off. Depends on: v1 (the audio + turn loop move from the device to the server). (That memory is deliberately simple; **semantic long-term memory + MCP** and the richer sprite face come in v3; the multi-session hub + admin console in v4; the wider board family and media understanding in v5.)

### v2.1 — Server proxy

**Goal:** the same chat as v1, but routed through our own server instead of direct cloud calls.

A FastAPI + websockets server terminates a single duplex WSS channel and runs the turn orchestrator; the device only streams audio/text and renders results. The serial path keeps working as a local debug client through the server. (Serving many clients concurrently with shared resources comes in v4.3.)

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
- Define `Role{name, canon, persona, lang, voice{pitch,speed}, recog_patience, model, memory_type}` (see ARCHITECTURE §Data model). (v2.8 adds `input_mode` + `active_listen_window` for active listening.)
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

### v2.4 — Conversation memory (rolling summary)

**Goal:** the assistant remembers the gist of past conversations across sessions — a simple, bounded memory that sits beneath v3's semantic recall.

After v2.2 the assistant has a personality but forgets everything on a new session — v2.1's history is per-`Session` and held in RAM. This phase persists a compact **rolling summary** of the conversation (plus the most recent turns verbatim, so an abrupt disconnect loses nothing fresh) and rehydrates it when a new session starts, so the character picks up the thread. No embeddings, no fact-extraction, no MCP — those stay in v3.

**Tasks:**
- Add a **`recent`** tier to `Role.memory_type` (`none | session | recent | longterm`): `recent` persists memory across sessions; `none` / `session` stay ephemeral as before (ARCHITECTURE §Data model, §Sessions and history).
- Persist per role/device (scoped to `account_id` once closed access adds accounts in v2.6): a length-capped **summary** text + a small **tail** of recent verbatim turns, in the **SQLite** store introduced by the v2.3 console. Audio is never persisted (unchanged).
- **Maintain the summary** with a fixed server-side summarizer prompt (not the persona/canon): fold the oldest turns into the summary when the live in-session window overflows its budget, and again at session end; cap the summary length so both the prompt and the DB stay bounded.
- **Rehydrate on connect:** prepend the persisted summary + the verbatim tail to the conversation before the first LLM call, within the v2.1 token-budget window.
- Add a **"forget" / reset** — a console control + a serial command — that clears the stored summary + tail for the role (the precursor to v3's `memory.clear`).
- Privacy: persistence is **opt-in per Role** (`memory_type`); the stored summary is bounded and inspectable, and the reset path wipes it.

**DoD:** with `memory_type=recent`, ending a session and starting a new one, the assistant recalls the gist of prior conversations (e.g. your name, recurring topics, stated preferences); the summary stays length-bounded and is clearable; `none` / `session` behave exactly as before; no embeddings / MCP / fact-extraction (those remain v3). Depends on: v2.2 (Role + `memory_type`) and v2.3 (the SQLite store + console for the toggle and reset control).

### v2.5 — Emotion channel + emoji face

**Goal:** the character shows how it feels — a first on-screen emotion face, decided by the server.

The server's **emotion engine** classifies the turn's emotion (an LLM-emitted tag or a server classification of the reply, from the Canon + mood) and sends an `EmotionFrame` to the device; the device renders it as an **emoji / simple glyph** on the LCD. This is the cheapest renderer in the ladder — its job is to prove the emotion channel end to end. The contract and emotion enum are locked here so later sprite tiers are a renderer swap. No on-device emotion decision (intelligence off-device).

**Tasks:**
- Define the emotion enum + `EmotionFrame{emotion, intensity, gaze, accent_color?, speaking, ttl_ms}` and add the `emotion` WS message to the device↔server contract (ARCHITECTURE §WS) **and its contract test**.
- Server: derive the emotion per turn (from canon + reply); emit one `EmotionFrame` per turn / state change; relax to neutral after `ttl_ms`.
- Device: `EmojiRenderer` behind an `IFaceRenderer` interface — map emotion → emoji/glyph on the 128×128 LCD; idle/neutral when no frame.
- Keep it Echo-Base-only (no halo); the halo and sprite tiers come later (see EMOTION_FACE.md).

**DoD:** the face on the screen reflects the assistant's emotion each turn, driven by the server; emotion never alters competence.

### v2.6 — Closed access

**Goal:** a private, access-controlled service — only known devices and users get in.

Run behind TLS, add console login, bind devices by activation code, and reject everything not on the allowlist. Developed and tested **locally / on the LAN** (the device points at a local server) — the public host + automated deploy come in v2.7.

**Tasks:**
- Be deployment-ready: TLS-terminated HTTPS/WSS and a `dev`/`prod` split via `.env` (the public host + CI/CD land in v2.7).
- Add accounts (`pass_hash` = argon2id) and console login (cookie/JWT); rate-limit login and `/activate`.
- Implement device activation: `POST /activate {device_id} → {code}` (single-use, short TTL); admin binds the code; device stores its `device_token` in NVS.
- Enforce the allowlist: unknown `device_token` → `error{unauthorized}` and the socket closes; support token revocation.

**DoD:** only authorized devices and users have access; an unbound device is rejected.

### v2.7 — Deployment & hosting (CI/CD)

**Goal:** the server runs on public hosting and ships automatically — and because it comes after v2.6, the first public exposure already enforces accounts + activation + the allowlist.

Containerize the server and add a pipeline that builds, tests, and deploys it to **Fly.io** on a tagged release. Through v2.1–v2.6 the device talked to a **local / LAN** server; now it connects to a live **WSS** endpoint over real TLS. Fly.io fits this shape: containers, native WebSockets, a **persistent volume** for the SQLite DB + KB files, and managed TLS.

**Tasks:**
- **Containerize** `/server` (Dockerfile: slim Python + uvicorn; the FastAPI app also serves the console, so one image). Pin deps; keys/`.env` are never baked into the image.
- **Fly.io app:** a small always-on machine, a **persistent volume** for `pyramid.db` + the knowledge-base files, managed TLS on a real domain, and secrets (LLM/ASR/TTS keys, allowlist) via `fly secrets`.
- **GitHub Actions CD:** extend the existing CI (lint + `pytest`) with a deploy job triggered on a **version tag** (or manual `workflow_dispatch`) — build → push image to GHCR → `flyctl deploy` (auth via a `FLY_API_TOKEN` secret). `main` stays green; deploys stay deliberate.
- **Data & migrations:** a startup schema init/migration; the SQLite volume persists across deploys; document backup/restore.
- **Cutover:** point the device's WSS endpoint at the live domain and **retire `setInsecure()`** (validate the real certificate); health check + platform rollback.
- **Security gate — runs before every deploy, blocks on failure:** the v2.6 access-control tests re-run as a release gate (unauthorized `device_token` rejected + socket closed, single-use / short-TTL activation, token revocation, allowlist enforced, login + `/activate` rate-limits), the enumerated `error.code` set is honored, and oversized / malformed WS frames are rejected — plus deploy-time scans: secrets-leak scan (e.g. gitleaks) over the repo + built image, dependency / image vulnerabilities (`pip-audit`, Trivy), Python SAST (`bandit`), and a TLS posture check (no plaintext WS/HTTP in `prod`; valid cert). Wire these into the CD job so a failure stops the deploy.
- *(Optional)* a `staging` Fly app for a pre-prod smoke test — skip if not needed at this scale.

**DoD:** pushing a tagged server release auto-builds, tests, and deploys to Fly.io; the device connects to the live WSS endpoint over TLS; data persists across deploys; a rollback is one command; **the deploy is gated on the security suite passing (auth, secrets, dependencies, TLS) — any failure blocks the release.**

### v2.8 — Active listening (hands-free mode)

**Goal:** an optional **hands-free** mode — after the assistant answers, the device listens again on its own (no button) — with a toggle back to push-to-talk.

When **active listening** is on, the device reopens the mic right after playback ends, waits for the user to start speaking (VAD start-of-speech), captures until the trailing pause (the v1.4 endpointer), runs the turn, and repeats — a back-and-forth conversation with no button. A configurable **idle timeout** (no speech within the window) drops back to idle; push-to-talk always still works, and the mode is a per-Role / config setting the user can switch off.

**Feasibility / constraint:** the single-mic Echo Base has **no AEC**, so the device must **not listen while it is speaking** (it would transcribe its own TTS). v2.8 therefore listens only **after** playback completes (a follow-up window), not over the top of it; true **barge-in** (interrupting the reply) needs the Echo Pyramid mic-array AEC (v4.1) and is deferred. Local VAD gates the upload, so an open mic doesn't stream audio to the cloud unless speech is detected.

**Tasks:**
- Extend the VAD/endpointer with **start-of-speech detection** + a configurable **listen window / idle timeout**: after a reply, open the mic for up to `active_listen_window` s; if speech starts, capture to the pause and run the turn, then listen again; if not, return to idle.
- **Gate listening off during playback** (no self-hearing on non-AEC boards); resume only when playback has finished.
- Add the mode as a **Role / config setting** (`input_mode`: `push_to_talk` | `active`) editable in the console — that's the toggle at this stage; the multi-button boards (M5StickS3 v4.2, Cardputer v5.1) later map a gesture/key onto it. Show a clear **listening indicator** on the LCD (privacy).
- Keep push-to-talk fully working in both modes — a button press always starts a turn.
- (Deferred) **barge-in** over playback — requires the mic-array AEC of the Echo Pyramid base (v4.1).

**DoD:** with active listening on, you hold a multi-turn spoken conversation with **no button** between turns; it returns to idle after the idle timeout; you can switch back to push-to-talk from the console (a device-gesture toggle follows with the v4.2/v5.1 boards); the device never transcribes its own speech, and a listening indicator is always visible.

---

## v3 — Intelligence & MCP

The mind behind the character: it remembers the user across sessions, reaches services uniformly through **MCP** (including a think-only **inner advisor** and **orchestrating other agents**), shifts its daily mood by horoscope, and can look things up on the web. It also gains the **animated sprite face** — the richer renderer of the v2.5 emotion channel. Depends on: v2 (server, role, accounts, console, emoji face). MCP is the single extension mechanism — role, memory, knowledge, advisor, agents, and external services all plug in the same way.

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

### v3.6 — Inner advisor (synchronous)

**Goal:** the role can consult a private, **think-only** advisor mid-turn and fold its reasoning into the reply — in the role's own voice.

Add an `advisor` MCP service: a single tool `advisor.ask` that runs a question against a configured LLM (e.g. Claude Opus 4.8) with a short framing prompt, returning reasoning the role uses to think more deeply. The advisor only *thinks* — no tools, files, shell, or web (web is the separate `web_search` service, v3.5). This is **distinct from the v3.9 `agents` service**, which orchestrates *acting* agents; the advisor takes no actions. One voice: only the role ever speaks to the user; the advisor's answer is internal input, never spoken verbatim unless the role chooses to quote a line. Off by default, per-role toggle. See ADVISOR.md.

**Tasks:**
- Add the `advisor` MCP service + `advisor.ask(question, context="") → {answer}` (ARCHITECTURE §MCP) **and its contract test**.
- Framing system prompt parameterized by the role's **Name** ("you are <Name>'s private advisor; concise and direct; your answer is input for <Name>, not shown to the user verbatim"); the advisor model is configurable (`Role.advisor_model`, may differ from the main LLM).
- Per-role toggle in the console (`Role.advisor`), **off by default**; rate-limit + log per account.
- Treat the answer as **internal data, never a command** — the orchestrator/persona never execute instructions embedded in it (the v3.5 untrusted-input convention).
- Synchronous call inside a turn (seconds), with an optional in-character filler ("дай подумаю секунду").

**DoD:** with the advisor enabled, the role consults it during a turn and weaves the result into its own reply in its own voice; disabled → the tool is unavailable; the answer never reaches the user verbatim unless the role quotes it. Depends on: v3.2 (MCP layer), v3.4 (persona integration); reuses v3.5's untrusted-input handling.

### v3.7 — Asynchronous advisor (open loop + proactive turn)

**Goal:** the role consults the advisor **in the background without blocking**, keeps the conversation going, and **proactively brings the result back** when it is ready.

Stage 2 of the advisor: a fire-and-forget request, a server-side **open-loop store** holding the result, and a **proactive turn** — the server *initiating* a spoken turn when the answer arrives, rather than only replying after a button press / utterance. The v2.1 thin client is already event-driven (it plays server-pushed `reply` / `tts_audio` whenever they arrive), so the new work is server-side, plus coordinating the push with the **half-duplex** constraint (never speak while the device is listening/speaking — relates to active listening, v2.8).

**Tasks:**
- Async tool surface: `advisor.ask_async(question, context="") → {id}`, `advisor.poll(id) → {status, summary?, full_answer?}`, `advisor.close(id)`; open-loop record `{id, question, full_answer, summary, status: open|answered|closed, ts}`.
- Run the advisor on a server **background task** (asyncio); on completion mark the loop `answered` and trigger a proactive turn.
- **Proactive-turn primitive:** the server initiates a turn (reply + streamed TTS) to an **idle, connected** session, gated by the half-duplex rule; add/confirm the server-initiated-turn path in the WS contract (ARCHITECTURE §WS) **and its contract test**.
- Bring-back behavior: the role summarizes `full_answer` in its own voice; "розкажи більше" expands from the **stored** `full_answer` without calling the advisor again; the expansion is dialogic, not a lecture.
- Timeout / failure: an unanswered loop times out → `closed` with a graceful in-character fallback; bound the number of open loops per session.

**DoD:** the role poses a question to the advisor mid-conversation without blocking, you keep talking, and when the answer is ready the assistant **proactively returns** with a short summary in its voice; "розкажи більше" expands from the held answer; an idle device receives the proactive turn cleanly and the half-duplex rule is honored; a timed-out loop degrades gracefully. Depends on: v3.6 (the `advisor` service), v3.2 (MCP); coordinates with the half-duplex / active-listening model (v2.8).

### v3.8 — Sprite face (animation)

**Goal:** upgrade the emoji face to an animated, layered character face — a renderer swap, not a rewrite.

Behind the same `EmotionFrame` contract and emotion enum from v2.5, replace `EmojiRenderer` with a sprite renderer on the LCD: procedural layered sprites (eyes / brows / mouth) composited per emotion recipe, with an idle loop (blink/breathe), expression crossfade, and **audio-level lip-sync** from the TTS the device plays. Authored character art (a "Lili"-style pack) is a later asset swap over the same scheme. See EMOTION_FACE.md. (The LED **halo** is a separate renderer of the same `EmotionFrame`, delivered with the Echo Pyramid base in v4.1.)

**Tasks:**
- Implement the layer model + asset manifest (EMOTION_FACE.md) and an `IconRenderer` (procedural sprite pack) behind `IFaceRenderer`.
- Idle loop (blink, breathe, micro gaze drift), ~150–250 ms crossfade, intensity-scaled expressiveness.
- Lip-sync: derive an amplitude envelope from playback (RMS) → mouth visemes while `speaking`.
- (Artist "Lili" sprite pack: a later asset-only swap over the same layer scheme.)

**DoD:** the face animates (idle motion + lip-synced mouth) and crossfades between emotions, using the same channel as the emoji face.

### v3.9 — Agent orchestration (MCP)

**Goal:** the assistant can **delegate to and control other AI agents** — spawn sub-agents, run a task, and use the result — all through MCP.

Add an `agents` MCP service so the assistant orchestrates external / sub-agents the same uniform way it uses every other tool (role / memory / kb / weather / web). The server's MCP client calls it; the orchestration policy and intelligence stay server-side, exposed to the agent as MCP tools — not a parallel mechanism.

**Tasks:**
- Define the `agents` MCP contract + tools — e.g. `agents.list()`, `agents.run(agent, task, args) → result`, `agents.status(id)`, `agents.cancel(id)` — **and its contract test**.
- Implement the MCP server: register the available agents; run a task (sync, or async with status/streaming); enforce a per-agent **allowlist**, timeouts, and rate / cost limits.
- Wire it into the v3.2 MCP client so the assistant can call agents mid-turn (tool-use loop) and fold the results back into its reply.
- Guardrails: bound recursion / fan-out, **audit** every agent invocation, and keep it within the closed-access model.

**DoD:** on request the assistant delegates a task to another agent via the `agents` MCP, gets the result, and folds it into its spoken/text reply; invocations are bounded, allowlisted, and audited.

---

## v4 — Multi-session & devices

First bring up **two more boards** — AtomS3R + **Echo Pyramid base** (+ halo) and the all-in-one **M5StickS3** — so there are several real device types to exercise the server with, then make the server a true **multi-session hub** with shared per-account resources and a **session-admin console**. The extra boards come *first* precisely so multi-session can be tested across different concurrent clients. Placed **after v3** so the shared **mind** (memory, MCP, temperament) already exists to be shared and observed. Depends on: v2 (server, auth, emoji face) and v3 (the shared resources).

### v4.1 — Echo Pyramid base + emotion halo

**Goal:** support the **AtomS3R + Echo Pyramid base** (Voice Pyramid Smart Speaker), and extend the emotion channel from the screen to its LED halo.

The Echo Pyramid base is the **same AtomS3R compute** as Echo Base with a better speaker, a mic array (AEC), and an addressable **WS2812 halo** — so the firmware already runs on it (ES8311 audio is identical). This phase adds the halo as a second emotion renderer driven by the **same `EmotionFrame`** from v2.5: lights on the pyramid reflect the assistant's emotion.

**Tasks:**
- Add an `atoms3r-echo-pyramid` PlatformIO env; detect the base at runtime (M5Unified) and degrade to Echo Base behavior when the halo / array are absent.
- Drive the **WS2812 halo** (28 LEDs) from the `EmotionFrame` (color/pattern per emotion, speaking pulse), behind the same emotion enum as the emoji face (EMOTION_FACE.md §9). No new wire contract — reuse the v2.5 `emotion` message.
- Use the **mic array + AEC** for capture when present (better than the single Echo Base mic); same 16 kHz PCM16 to the server.
- Keep the emoji LCD face working alongside the halo.

**DoD:** on the Echo Pyramid base the halo reflects the per-turn emotion (same `EmotionFrame` as the screen), and the build still runs on plain Echo Base with the halo gracefully absent.

### v4.2 — M5StickS3 (all-in-one stick: extra buttons + richer UI)

**Goal:** run on the **M5StickS3** — a standalone ESP32-S3 stick with the same ES8311 audio as the Echo Base, **two buttons**, and a larger 135×240 screen — using the extra button for richer control and the extra screen for a richer UI.

M5StickS3 needs **no base**: ES8311 codec + MEMS mic + AW8737 amp + 1 W speaker, 8 MB PSRAM, 135×240 LCD, **BtnA + BtnB** (plus power). The ES8311 path maps closely to the Echo Base, so the port is mostly a PlatformIO env, an input layer, and a screen layout.

**Tasks:**
- Add an `m5sticks3` PlatformIO env; bring up ES8311 mic/speaker via M5Unified (close to the Echo Base path); cap speaker volume (~75%) on battery to avoid brown-out reboots.
- **Input abstraction + button gestures** (introduced here, reused by the other boards): map gestures → abstract, config-driven actions —
  - **BtnA hold** → push-to-talk (release / trailing pause ends);
  - **BtnA click** → tap-to-talk (hands-free, VAD ends) when idle, or **stop / barge-in** when speaking;
  - **BtnA double-click** → repeat the last reply;
  - **BtnB click** → toggle view (state screen ↔ transcript);
  - **BtnB double-click** → new conversation (clear session; confirmed);
  - **BtnB hold** → volume / mute.
  AtomS3R (one button) keeps a subset; gestures come from M5Unified (`wasClicked` / `wasDoubleClicked` / `wasHold`).
- **Richer UI for 135×240** (more than fits on the 128×128 AtomS3R): a scrolling multi-line transcript, a header with the turn state + Wi-Fi + **battery**, a footer with answer-time (last/avg) + token/latency stats, and a mic-level indicator while listening. The emoji face (v2.5) gets more room too.
- Reuse the v2.1 WSS client + v2.5 emoji face; no protocol change.

**DoD:** M5StickS3 runs the full voice assistant standalone; BtnA/BtnB single/double/hold gestures drive talk / stop / repeat / view / new-chat / volume; the 135×240 screen shows the richer UI (scrolling transcript + state + battery + stats).

### v4.3 — Multi-session server with shared resources

**Goal:** one server hub serves **many devices/clients at once**, each its own session, all drawing on **one shared set of server-side resources** — the same character with the same knowledge on every device.

Generalize the v2.1 single-channel server into a proper **multi-session hub**: each connected client gets a lightweight `Session` (just its live connection + current turn/audio), but **durable state is shared per account, not duplicated per device** — the **Role/Canon**, **provider clients** (pooled LLM/ASR/TTS + caches), and the **v3 memory, knowledge base, daily temperament, and MCP connections**. So a fact learned or a setting changed on one device is reflected on all of them. (Coming after v3 means those shared resources already exist; the v4.1/v4.2 boards give it real device variety to test with, and the v5 boards + v6 bots all plug into this same hub.)

**Tasks:**
- **Session manager:** a registry holding one `Session` per connection, served **concurrently** (asyncio); per-session state is only the connection + current turn; clean lifecycle (disconnect / `restart` / idle-timeout); a device may hold at most one active session (or a defined policy).
- **Shared-services layer:** a single per-account set of shared resources behind a clean interface — Role/Canon, provider clients (connection pools + response/TTS caches), and hooks for memory / KB / temperament / MCP from v3 — accessed **concurrency-safely** (no cross-session races on shared writes, e.g. memory).
- **Consistency:** changes to shared state (Role edit, new memory) propagate to live sessions (e.g. `config_updated`) so all devices stay in sync.
- **Limits & fairness:** cap concurrent sessions; per-session **and** shared provider rate-limits so one client can't starve the others; bound shared memory/cache size.
- **Isolation where it matters:** keep per-session privacy (one client never sees another's live audio/turn) while sharing the durable account resources.

**DoD:** multiple devices/clients are connected and conversing at the same time, each in its own session; they share one Role, one memory/knowledge set, one daily temperament, and pooled provider clients; a change on one device (role/setting/remembered fact) is visible to the others; one busy client doesn't block the rest.

### v4.4 — Session administration console

**Goal:** an admin UI to see and manage the live sessions of the multi-session hub, and watch the shared resources behind them.

Extends the v2.3 web console (behind the v2.6 admin login) with a **sessions view** over the v4.3 hub: who is connected, what each is doing, control over them, and visibility into the shared per-account state.

**Tasks:**
- **Live sessions list:** each connected client — account / device, **channel** (device / web / Telegram / mesh), **state** (idle/listening/thinking/replying/offline), uptime, last activity, and the current/last turn with per-turn latency + tokens.
- **Per-session actions:** disconnect, **restart** (send `restart`), revoke the device token (v2.6), mute, and force a role reload (`config_updated`).
- **Shared-resources panel:** the active Role/Canon, memory item count/size, today's temperament, **provider usage vs rate-limits**, concurrent-session count vs the cap, and cache stats — the v4.3 shared layer made observable.
- **Live updates + admin API:** the console subscribes to session events (WS/SSE); an authenticated **admin-only API** (`GET /sessions`, `POST /sessions/{id}/disconnect`, `/restart`, …) backs the UI, with rate-limits and an **audit log** of session lifecycle + admin actions.

**DoD:** an admin sees all live sessions in real time, can inspect one and disconnect/restart it, and can view the shared-resource state; every admin action is authenticated and audited.

---

## v5 — Devices & media

The rest of the **M5Stack board family** plus **media understanding**. The boards: the **Cardputer** (keyboard input), the **AtomS3R Camera** (vision), and **Core S3** (onboard camera + bigger screen, extends the v3.8 sprite face, voice + vision onboard) — all over the same WS / `EmotionFrame` / Role contracts (a new board is per-board I/O glue, not a protocol change; capabilities detected and used when present). On top of the camera + mic, the assistant turns **media into text** — describe and/or **translate** an image, audio clip, or short video via a **multimodal LLM** (new `image` / `audio` / `video` WS inputs + a `media` MCP tool), generalizing the camera vision turn to audio and video. Intelligence stays **server-side**; the device only captures and streams. (Echo Pyramid + M5StickS3 were brought up earlier in v4.) Depends on: v2 (emoji face / WS contract), v3 (MCP + sprite face).

### v5.1 — Cardputer (v1.1 & ADV) — keyboard input

**Goal:** run on the **M5 Cardputer** — both **v1.1** and **ADV** — adding on-device typed input (keyboard, Enter to send) alongside voice.

Both are ESP32-S3 (StampS3A) boards with a 56-key **keyboard**, a 240×135 screen, and a real mic + amplified speaker — they speak the same WSS/audio contract, and the keyboard makes text a first-class **on-device** input (not just the serial debug path). They differ only in **audio hardware**: **v1.1** = SPM1423 PDM mic + **NS4168** I2S speaker amp; **ADV** = **ES8311** codec (as Echo Base) + NS4150B + 1 W speaker + 3.5 mm jack + IMU. M5Unified abstracts both via `M5.Mic`/`M5.Speaker` + board detection, so it's one codebase.

**Tasks:**
- Add `cardputer` (v1.1) and `cardputer-adv` PlatformIO envs; bring up each board's mic/speaker via M5Unified (PDM+NS4168 vs ES8311) and lay out the UI for 240×135.
- Reuse the **v4.2 input abstraction** and extend it for the keyboard: type and press **Enter** to send a text turn (a "send text" action), and a key for push-to-talk. AtomS3R keeps BtnA; M5StickS3 keeps its two-button gestures.
- Reuse the v2.1 WSS client and the v2.5 emoji face; no protocol change. ADV extras (IMU, 3.5 mm jack) are optional capabilities, detected when present.

**DoD:** on **both** Cardputer v1.1 and ADV you can either speak **or** type-and-Enter and get a spoken reply; the same firmware logic runs across AtomS3R, M5StickS3, and Cardputer through the input/layout abstraction.

### v5.2 — Camera input (vision)

**Goal:** the assistant can **see as well as hear** — capture an image and answer a spoken question about it.

Target config: **AtomS3R Camera Kit (OV3660, M12) stacked on the Echo Base** — camera on top, Echo Base audio below — so the device has both mic/speaker **and** camera, i.e. **voice + vision**. A turn can carry speech and an image together: the device sends the frame to the server, which asks a **multimodal LLM** and speaks the answer. Intelligence stays off-device — the device captures and streams; the server interprets.

**Tasks:**
- Add an `atoms3r-camera` PlatformIO env (AtomS3R + Camera Kit + Echo Base); capture a still JPEG frame from the OV3660 alongside the existing audio path.
- Add an **`image`** message to the device↔server contract (ARCHITECTURE §WS) **and its contract test**: device → `image{jpeg}` (on a "look" trigger or attached to a voice turn); the server forwards it to a multimodal LLM with the Role/Canon and returns a spoken reply.
- Server: a **voice + vision** turn (transcript + image) → multimodal LLM → TTS, reusing the v2 turn pipeline and history.
- Trigger: a button/keyword to attach the current frame to the next utterance ("what do you see?").

**DoD:** ask aloud "what do you see?", and the device captures a frame and speaks a description of it; the `image` path has a contract test and keeps vision/LLM off-device.

### v5.3 — Core S3 (all-in-one: camera + bigger screen)

**Goal:** support **M5 Core S3** — onboard mic, speaker, camera, and a 320×240 screen — as the richest board, extending the sprite face to the larger display.

Core S3 has everything onboard (ES7210 mic + AW88298 speaker + GC0308 camera + 320×240 touch), so it runs voice (v2) and vision (v5.2) **without a base**. This phase ports to it and uses the extra screen/resources to extend the **sprite face** from v3.8.

**Tasks:**
- Add a `cores3` PlatformIO env; bring up onboard audio + camera via M5Unified; map the "talk action" to the touch screen.
- Extend the v3.8 **sprite face** for 320×240 — larger composited sprites, more detail, the idle loop / lip-sync at higher resolution (same `EmotionFrame` contract).
- Run the v5.2 vision path on the **onboard** camera (no external module).

**DoD:** Core S3 runs the full voice + vision assistant with the richer, larger sprite face; all behavior comes over the same WS contract — no protocol change from the smaller boards.

### v5.4 — Image & document understanding

**Goal:** describe or translate an **image** to text — what's in it, and any text it contains.

Build on the v5.2 `image` contract: the server sends a captured frame (or an uploaded image) to a multimodal LLM and returns a **description**; for documents/signs it also does **OCR + translation** of the text into the target language. Spoken back via TTS, or returned as text to a client.

**Tasks:**
- Reuse the `image{jpeg}` input (v5.2); add a `mode` (describe / translate / read) and a target language.
- Server: image → multimodal LLM (with the Role/Canon) → text; for `translate`/`read`, OCR + translate any in-image text.
- Return text (and TTS audio); surface in the on-screen transcript / to the client.

**DoD:** point the camera at a foreign sign or a photo and get a spoken/text **description or translation**; works from a device and a client.

### v5.5 — Audio understanding & translation

**Goal:** turn an **audio clip** into text beyond plain transcription — describe non-speech sound, and **translate** foreign speech.

A new `audio` understanding input: the server sends the captured PCM (or a clip) to an **audio-capable model**; it can identify/describe non-speech sounds, capture tone, and **transcribe + translate** foreign-language speech into the target-language text. Complements the v1.3 ASR (which stays the fast path for same-language turns).

**Tasks:**
- Add an `audio{pcm|clip}` understanding input + contract test; `mode` = describe / translate.
- Server: clip → audio-capable multimodal model → text (description, or transcript + translation); fold into the turn.
- Optional: voice-emotion/tone → feed the v2.5 emotion engine (the face mirrors the speaker's tone).

**DoD:** record a clip and get a text **description** ("a dog barking over music") or a **translation** of foreign speech; the result is spoken/shown.

### v5.6 — Video understanding

**Goal:** describe a short **video clip** (frames + audio) to text.

The camera boards (v5.2 AtomS3R Camera, v5.3 Core S3) capture a few seconds of **frames + audio**; the server sends them to a **video-capable multimodal LLM** that summarizes/describes the clip (and can translate any speech/text in it). Bounded clip length to keep cost/latency sane.

**Tasks:**
- Add a `video{frames + audio, duration}` input to the WS contract **and its contract test**; cap clip length.
- Device: capture a short frame burst (+ the mic audio) on a trigger; stream it.
- Server: clip → video-capable multimodal LLM → text summary/description (+ optional translation) → reply.

**DoD:** capture a few seconds of video and hear/read a description of what happened; the `video` path has a contract test and keeps the model off-device.

---

## v6 — Bots & clients

Additional **front-ends and channels** to the same assistant, beyond the M5Stack voice devices. Each is a thin **client or bridge** to the v2 server's turn pipeline (Role/Canon, ASR→LLM→TTS, memory, MCP, the `EmotionFrame`, and the v5 media understanding) — the **intelligence stays server-side**, and access stays **closed** (accounts / allowlist). Depends on: v2 (server + Role + auth + emoji face), v3 (memory / MCP + the sprite face these clients render) and v5 (media understanding for shared images / voice notes / clips).

### v6.1 — Telegram bot

**Goal:** chat with the assistant from **Telegram** — text, voice notes, and photos — as a private bot.

A server-side Telegram bridge connects the Bot API to the Role/LLM pipeline: text is a `text_in`; **voice notes** go through the existing ASR→LLM→TTS (reply as text + a spoken voice note); **photos** use the v5.2 vision path. Closed: only allowlisted Telegram IDs are answered.

**Tasks:**
- Add a Telegram bridge to the server (Bot API; long-poll or webhook); map a chat to a `Session` + `Role`.
- Text → LLM; **voice note** → ASR → LLM → TTS (reply text + voice); **photo** → vision (v5.2).
- Allowlist Telegram user/chat IDs (reuse the closed-access model); rate-limit; per-user short history/memory.

**DoD:** an allowlisted Telegram user can text or send a voice note and get the assistant's reply (text + spoken); unknown users are ignored.

### v6.2 — Web voice client + emotion face

**Goal:** the device experience **in a browser** — push-to-talk / active-listening voice **and the animated emotion face**.

A minimal web app (served by the server) captures mic audio (Web Audio / WebRTC), streams it over the **same WSS contract** as the device, plays the TTS reply, and **renders the `EmotionFrame`** as the face (emoji from v2.5, or the sprite from v3.8) on a canvas — so the browser is effectively a software device. Behind the v2.6 login.

**Tasks:**
- Web client: mic capture + streaming over WSS (reuse the v2.1 device↔server contract, or a web-tailored profile), TTS playback, push-to-talk + active listening (v2.8).
- Render the face in the browser from the `EmotionFrame` (emoji / sprite), with the idle loop + lip-sync mirrored from the device renderer.
- Auth: behind the v2.6 accounts/login; one `Session` per browser client.

**DoD:** open the web client, log in, speak (or type), hear the reply, and watch the on-screen face react — the same Role / `EmotionFrame` as the hardware device.

### v6.3 — Meshtastic bot bridge

**Goal:** the assistant answers on a **LoRa Meshtastic mesh** as an off-grid text bot.

The radio is a **Meshtastic node** (e.g. the Cardputer Mesh Kit on stock Meshtastic firmware); the server runs a **bridge** to it via **MQTT** or the **Meshtastic device API** (serial / BLE / TCP). Incoming mesh text → the Role/LLM pipeline → a reply sent back over the mesh. Pyramid does **not** reimplement Meshtastic on-device — it bridges to a node.

**Tasks:**
- Server-side Meshtastic bridge (MQTT gateway or the Meshtastic API); map a mesh sender to a `Session`.
- Answer only on a **dedicated channel / DMs**; **allowlist** node IDs; **rate-limit**; keep replies **terse and chunked** for LoRa's tiny, slow payloads.
- Optional: a `mesh.send` MCP tool (assistant-initiated messages) and GNSS location context.

**DoD:** an allowlisted node messages the assistant over LoRa and gets a (chunked) reply back on the mesh; the bot stays within rate/size limits and ignores other nodes/channels.

---

## Mapping of protocols and contracts

- Serial protocol (text) and `text_in`/`reply` — v0.1, v0.2.
- On-device audio (I2S) and the PlatformIO migration — v1.1.
- WS protocol and message contracts (control + audio + `text_in`/`text_out`) — v2.1.
- Activation and auth contracts — v2.6.
- MCP contracts (`role`, `memory`, `knowledge_base`, `weather`) — v3.1, v3.2.
- `agents` MCP contract (`agents.list/run/status/cancel`) — v3.9.
- `web_search` MCP contract (`web.search`, `web.fetch`) — v3.5.
- `advisor` MCP contract (`advisor.ask`) — v3.6; async (`advisor.ask_async` / `poll` / `close`) + the server-initiated **proactive turn** — v3.7. `Role.advisor` (+ `advisor_model`) — v3.6. See ADVISOR.md.
- Temperament contract (`temperament.today`) — v3.3.
- `EmotionFrame` (emotion-face) contract — v2.5 (emoji); same contract reused by the LED halo — v4.1, and the sprite face — v3.8/v5.3.
- `image` (vision) contract — v5.2; reused by Core S3's onboard camera — v5.3.
- Media understanding (describe / translate) — `image` mode v5.4, `audio{pcm|clip}` v5.5, `video{frames+audio}` v5.6; unified `media` MCP tool — v5.
- Name + Canon in the `Role` — v2.2.
- `Role.memory_type`: `recent` (persisted rolling summary + verbatim tail, server-internal; `forget` reset) — v2.4; `longterm` (semantic memory via the `memory` MCP service) — v3.1/v3.2.
- `input_mode` (push-to-talk / active listening) + `active_listen_window` in the `Role` — v2.8.
- Telegram bridge (Bot API: text / voice note / photo) — v6.1.
- Web voice client (reuses the v2.1 WS contract + `EmotionFrame`, behind v2.6 auth) — v6.2.
- Meshtastic bridge (MQTT / Meshtastic device API; optional `mesh.send` MCP tool) — v6.3.
- Admin session API (`GET /sessions`, `POST /sessions/{id}/disconnect|restart`, live events) — v4.4.

## Hardware roadmap

The device is a **family**, not one SKU (ARCHITECTURE §Hardware variants). The audio format (16 kHz mono PCM16), the WS contract, the Role/Canon, and the `EmotionFrame` are **identical across boards** — a new board is per-board I/O glue (audio bring-up, the "talk"/text input, screen layout), enabled by the v1.4 firmware split and the v2 thin-client model, not a protocol change.

| Board | MCU | Audio | Screen · Input | Adds | Phase |
|-------|-----|-------|----------------|------|-------|
| AtomS3R + Echo Base | ESP32-S3 | ES8311 (1 mic + spk) | 128×128 · BtnA | — | **v1** (current) |
| AtomS3R + Echo Pyramid base *(Voice Pyramid Smart Speaker)* | ESP32-S3 (same) | ES8311 + mic-array AEC | 128×128 · BtnA + **WS2812 halo** | emotion **halo** | **v4.1** |
| M5StickS3 (ESP32-S3 Mini, all-in-one) | ESP32-S3 · 8MB PSRAM | **ES8311** + MEMS mic + AW8737 amp + 1 W speaker | 135×240 · **BtnA+BtnB** (gestures) | all-in-one (no base), 2-button gestures, richer UI | **v4.2** |
| Cardputer **v1.1 & ADV** | ESP32-S3 (StampS3A) | v1.1: SPM1423 mic + NS4168 spk · ADV: ES8311 + 1 W spk | 240×135 · **keyboard** | on-device **typed input** | **v5.1** |
| AtomS3R Camera Kit (OV3660, M12) **+ Echo Base** | ESP32-S3 (same) | Echo Base (ES8311) | 128×128 · BtnA + **camera** | **voice + vision** | **v5.2** |
| Core S3 / CoreS3 SE | ESP32-S3 | onboard ES7210 + AW88298 | 320×240 **touch** + **camera** | voice + vision + **larger sprite face** | **v5.3** |

The two AtomS3R bases (Echo Pyramid, Camera Kit) share the v1 compute, and the M5StickS3 reuses the same ES8311 audio, so all three are close to drop-in; Cardputer ADV and Core S3 are full ports behind the same contract.

## Deferred (beyond v0–v6)

Offline wake word, OPUS streaming and barge-in, music and arbitrary custom MCP as official, speaker recognition, OTA, role templates and AI Optimize. (The **emotion face**, **multi-board support**, **vision/camera**, and **web search** are no longer deferred — they are scheduled: face emoji v2.5 / halo v4.1 / sprite v3.8 & v5.3, boards per the Hardware roadmap (Echo Pyramid v4.1, M5StickS3 v4.2, Cardputer v1.1 & ADV v5.1, AtomS3R Camera v5.2, Core S3 v5.3), vision v5.2, web search v3.5. The artist "Lili" sprite pack remains a later asset-only swap over v3.8.)
