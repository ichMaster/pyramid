# Mission — M5Stack Voice AI Chatbot (codename "Pyramid")

## In one sentence

A closed, online voice AI assistant on M5Stack AtomS3R + Echo Base hardware: a living, configurable persona that speaks Ukrainian, remembers the user, shifts its daily mood by horoscope, and can use external services through MCP.

## What we are building

A simple, self-tailored analog of xiaozhi. The device is thin: input/output and a status screen; all the intelligence (LLM, and later ASR, TTS, memory, MCP) lives in the cloud or on a server. Behavior is defined by a single configurable "role." The product grows across four versions: first a text chat over USB serial, then voice, then our own server with role configuration, and then memory, horoscope-temperament, and MCP to other services.

## For whom

A private service for myself and a close circle. No public access: users and devices are added manually (allowlist), and a device is bound by an activation code.

## Principles

- **Simplicity first.** This is a deliberately simple project; complexity is added only by versions, not all at once.
- **Intelligence off-device.** The device is thin: input/output (text over serial first, then audio) and a status screen. No persona logic on the device.
- **Config is the source of truth.** Behavior is fully defined by the role on the server; the device only executes what it is told.
- **Closed by default.** Access is private; unauthorized connections are rejected.
- **Uniform extensions via MCP.** Role, memory, knowledge, and external services plug into the agent as MCP — one mechanism for everything.
- **Horoscope is an experiment.** Astrology is used only as a generative method for daily mood variation, not as a claim about reality; it affects tone and delivery, never competence.
- **Incremental.** Each version is self-contained and works on its own.

## Non-goals

- Not a public service, not a mass product.
- Not a complex cognitive architecture like the previous project: no planner-facets, no scored portrait, no background self-tuning in v0–v3.
- Early versions do not include: offline wake word, OPUS streaming, OTA, speaker recognition, multi-board support, emotion engine — all of these come later.

## Glossary

- **Role** — the configuration of the assistant's behavior: persona, voice, language, model, memory, services.
- **MCP** — Model Context Protocol; the way to give the agent tools and resources (role, memory, knowledge, weather, etc.).
- **Temperament** — daily mood dials (energy, warmth, verbosity, speech speed, and voice pitch) derived from the role's horoscope.
