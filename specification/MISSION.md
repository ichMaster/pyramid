# Mission — M5Stack Voice AI Chatbot (codename "Pyramid")

## In one sentence

A closed, online voice AI assistant on M5Stack hardware: a living, **named** character (with an authored **canon**) that speaks Ukrainian, shows an on-screen emotional face, remembers the user, shifts its daily mood by horoscope, and can use external services — including bounded web search — through MCP.

## What we are building

A simple, self-tailored analog of xiaozhi. The device is thin: input/output and a screen; all the intelligence (LLM, and later ASR, TTS, memory, MCP) lives in the cloud or on a server. Behavior is defined by a configurable **Role** — including the character's **Name** and an authored **Canon** (its character bible). The product grows across eight versions: first a text chat over USB serial, then voice, then our own **server** with role/canon configuration (and the first on-screen **emoji face**), then the **mind** (memory, MCP — including agent orchestration — horoscope-temperament, web search, and the animated face), then a **multi-session hub** + admin console, then the wider **M5Stack device family** (boards, halo, camera), then **media understanding & translation** (audio / image / video → text), and finally additional **clients & bots** (a Telegram bot, a web voice client with the face, and a Meshtastic LoRa bot) on top of the same server.

The device is one of a **family of M5Stack boards**, not a single SKU: v1 targets **AtomS3R + Echo Base** (ES8311 audio, 128×128 LCD); later boards — **Echo Pyramid base** (mic array + LED halo, v5.1), **M5StickS3** (all-in-one stick, v5.2), **Cardputer v1.1 & ADV** (keyboard, v5.3), **AtomS3R Camera** (vision, v5.4) and **Core S3** (onboard camera + larger screen, v5.5) — add capabilities the firmware detects and uses when present, degrading gracefully when absent. The audio / WS / Role / `EmotionFrame` contracts are identical across boards (ROADMAP §Hardware roadmap).

## For whom

A private service for myself and a close circle. No public access: users and devices are added manually (allowlist), and a device is bound by an activation code.

## Principles

- **Simplicity first.** This is a deliberately simple project; complexity is added only by versions, not all at once.
- **Intelligence off-device.** The device is thin: input/output (text over serial first, then audio) and a screen. No persona/canon logic and no emotion *decision* on the device — it only renders the face/state it is told to.
- **Config is the source of truth.** Behavior is fully defined by the role (Name, Canon, persona, voice…) on the server; the device only executes what it is told.
- **Character is authored, not computed.** The Name and Canon are written by hand — a character bible, not a facet engine or scored portrait. Temperament and the on-screen emotion color *presentation* (tone, voice, face), never competence or willingness to help.
- **Closed by default.** Access is private; unauthorized connections are rejected.
- **Uniform extensions via MCP.** Role, memory, knowledge, and external services plug into the agent as MCP — one mechanism for everything.
- **Horoscope is an experiment.** Astrology is used only as a generative method for daily mood variation, not as a claim about reality; it affects tone and delivery, never competence.
- **Incremental.** Each version is self-contained and works on its own.

## Non-goals

- Not a public service, not a mass product.
- Not a complex cognitive architecture like the previous project: no planner-facets, no scored portrait, no background self-tuning. The **Canon is authored content**, not a computed model.
- Deferred (not in any planned version yet): offline wake word, OPUS streaming, OTA, speaker recognition.
- Planned but not early: the **emotion face** (from v2), **additional hardware** beyond Echo Base (Echo Pyramid v5.1, M5StickS3 v5.2, active listening v2.7, Cardputer v1.1 & ADV v5.3, AtomS3R Camera v5.4, Core S3 v5.5), **vision/camera** (v5.4), and **web search** (v3.5). Each lands in its version, not before.

## Glossary

- **Role** — the configuration of the assistant's behavior: Name, Canon, persona, voice, language, model, memory, services.
- **Name** — the character's name (e.g. "Lili"); part of the Role.
- **Canon** — the authored character bible: lore, traits, and behavioral rules the system prompt is built from. Static, hand-written content (server-side from v2), distinct from the dynamic temperament.
- **MCP** — Model Context Protocol; the way to give the agent tools and resources (role, memory, knowledge, weather, web search, etc.).
- **Temperament** — daily mood dials (energy, warmth, verbosity, speech speed, and voice pitch) derived from the role's horoscope.
- **Emotion face** — the on-screen animated face (and, where the hardware has it, an LED halo) that renders the character's current emotion. The server decides emotion (from canon + mood); the device renders it. From v2.
- **Web search** — an optional, off-by-default MCP service letting the assistant look things up on the open internet within strict bounds (see WEB_SEARCH.md). v3.
