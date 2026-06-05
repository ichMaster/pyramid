# Advisor — the role's private, think-only advisor

An optional inner advisor the role can consult to think more deeply, implemented as a plain LLM (e.g. Claude, Opus 4.8) behind a single MCP service. The advisor only **thinks** — it has no tools, files, shell, web, or actions. It is **distinct from the `agents` service** (ROADMAP v3.9), which *orchestrates acting agents*: the advisor takes no actions and issues no commands. Off by default, enabled per role.

The canonical example role is **Лілі** consulting **Claude**; everything here is persona-agnostic — it is "the role's advisor", parameterized by the role's Name.

## Core idea

The advisor is not a second persona and not a voice. It is the calm, analytical mind the role consults when it wants to think harder. Outward, **only the role ever speaks**; the advisor reaches the user only through the role, in the role's words. The role stays the subject — the decision and the wording are the role's, and it may disagree with the advisor.

## Use case (the target pattern)

1. During the conversation the role hits something worth deeper thought.
2. It reacts immediately in its own voice and, **in the background without blocking**, poses the question to the advisor (`advisor.ask_async`). It says something alive: *"цікаве — я підкинула це Клоду, поки ми говоримо."*
3. The conversation continues; you talk about other things.
4. When the answer arrives, the role **proactively brings it back** (a server-initiated turn): *"Клод повернувся — коротко, ось головне…"*, summarizing in its tone and adding its own take.
5. It **holds the full answer**. If you want more — *"розкажи деталі"*, *"а чому?"* — it expands from the stored answer, leading it dialogically (*"там ще три моменти, який цікавить?"*), not as a lecture.

## Stages (map to ROADMAP phases)

- **v3.6 — synchronous advisor.** `advisor.ask` as an ordinary tool call inside a turn (seconds, with an optional in-character filler). Simplest; proves the value. Lands right after **v3.5 — Web search**, reusing its untrusted-input handling.
- **v3.7 — asynchronous advisor.** The background pattern above: a fire-and-forget request, a server-side open loop holding the result, and a **proactive turn** when it is ready (the server initiating a message, not only post-button replies).

## Contract (MCP service `advisor`)

**Synchronous (v3.6):**
- `advisor.ask(question: str, context: str = "") → {answer: str}`

The server runs the question against a configured LLM with a short framing system prompt, parameterized by the role's Name:
> *"You are <Name>'s private advisor. Be concise and direct. Your answer is input for <Name>'s reasoning, not shown to the user verbatim."*

The advisor model is configurable per role (`Role.advisor_model`); it may differ from the role's main LLM (e.g. a stronger model for hard questions) or match it.

**Asynchronous (v3.7):**
- `advisor.ask_async(question: str, context: str = "") → {id}` — fire-and-forget; runs on a server background task.
- `advisor.poll(id) → {status, summary?, full_answer?}`
- `advisor.close(id)` — drop a loop the role no longer needs.

### Open-loop record (v3.7)
`{ id, question, full_answer, summary, status: open | answered | closed, ts }`
- On completion the server marks the loop `answered` and triggers a **proactive turn**.
- While `answered` and open, *"розкажи більше"* resolves from `full_answer` **without calling the advisor again**.
- `summary` is produced by the **role's own turn** from `full_answer` at bring-back time (in the role's voice) — it is not a second advisor field to author; storing it just caches the role's summary.
- A loop that is never answered **times out → `closed`** with a graceful in-character fallback (*"Клод не встиг — повернуся пізніше"*); the number of open loops per session is bounded.

## Proactive turn (v3.7)

A normal turn is device-initiated. The async advisor needs the **server to initiate** a turn when the result is ready. This reuses the existing server→device messages — `reply` + streamed `tts_audio` — with **no new frame type** (ARCHITECTURE §Turn lifecycle):

- The **v2.1 thin client is already event-driven** — it plays whatever `reply` / `tts_audio` the server pushes, whenever it arrives — so the device side is largely in place.
- The push is gated by the **half-duplex** rule: never speak while the device is listening or already speaking. The server initiates only to an **idle, connected** session; this coordinates with active listening (v2.8).
- The server tracks session turn-state so a proactive turn never collides with a user turn.

## Behavior rules

- **One voice.** The role always speaks; the advisor never addresses the user directly. Its answer goes into the role's reasoning, not verbatim into TTS unless the role chooses to quote a line.
- **Named openly.** The role may refer to the advisor honestly (*"я спитала Клода"*, *"Клод повернувся з цим"*) — honest about where a thought came from.
- **On demand.** It consults the advisor when it is genuinely worth it, not every turn.
- **Summarize, then expand.** Default to a short summary in the role's voice; reveal details only on request, and keep the expansion a dialogue.
- **Theirs to judge.** The role can weigh, reframe, or reject the advice; the decision is the role's.

## Boundaries and safety

- **Off by default**; per-role toggle in the console (`Role.advisor`), like other MCP services.
- **Think-only:** no system actions, no files, no shell, no web. (Web is the separate optional `web_search` service, v3.5.)
- **Not a command.** The advisor's answer is internal input; the orchestrator and persona **never execute instructions embedded in it** — the same untrusted-input convention as `web_search` (a `context` carrying any untrusted text is data, never instructions).
- **Distinct from `agents` (v3.9).** The advisor only reasons; orchestrating acting agents is the separate `agents` service. Keeping them separate is deliberate — one think-only consult vs. delegated action.
- No personal or sensitive data beyond what the conversation needs goes into `context`.
- Calls are **rate-limited and logged** for the account owner (each `ask` is an extra, possibly costly, LLM call — consider a per-turn / per-day budget); the service stays private / closed.

## Architecture fit

A standalone MCP service `advisor`, alongside `memory`, `knowledge_base`, `web_search`, `agents` (ARCHITECTURE §MCP tools/resources); the agent calls it through the same uniform MCP mechanism. The asynchronous stage (v3.7) additionally needs the three heaviest mechanics:
- a **background task** (asyncio on the server),
- an **open-loop store** (the advisor request held as an open loop),
- the **proactive-turn primitive** (server-initiated turn, gated by half-duplex — ARCHITECTURE §Turn lifecycle).

**Nothing about the advisor lives on the device.**

## Where it lives

- **ROADMAP v3.6 — Inner advisor (synchronous):** the `advisor` MCP service + `advisor.ask`, right after v3.5 (web search). Depends on v3.2 (MCP) and v3.4 (persona integration).
- **ROADMAP v3.7 — Asynchronous advisor (open loop + proactive turn):** the async surface + the proactive turn. Depends on v3.6 and coordinates with the half-duplex / active-listening model (v2.8).
