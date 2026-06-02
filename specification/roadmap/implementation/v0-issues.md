# v0 — GitHub Issues

Issues for version **v0 — Text chat over serial**. One issue per phase (`v0.1`–`v0.3`); the work comes from the per-phase Tasks lists in [ROADMAP.md](../../ROADMAP.md) (§v0). Each issue ships with the tests that encode its DoD (see [ARCHITECTURE.md](../../ARCHITECTURE.md) §Testing and CI). Firmware toolchain for v0 is the **Arduino IDE** (PlatformIO + a native test env arrive in v1).

## Issues Summary Table

| # | ID | Title | Size | Phase | Dependencies |
|---|----|-------|------|-------|--------------|
| 1 | PYR-001 | Device skeleton and serial | M | v0.1 | -- |
| 2 | PYR-002 | Text chat loop | M | v0.2 | PYR-001 |
| 3 | PYR-003 | Quality and UX | M | v0.3 | PYR-002 |

**Size legend:** S = 1–2 days, M = 3–5 days, L = 5–8 days

---

## Dependency Tree

```
        PYR-001 (device skeleton + serial)
            |
        PYR-002 (text chat loop — LLM over HTTPS)
            |
        PYR-003 (quality & UX — history, errors, reconnect)
```

**Parallelization hints:** v0 is a strict chain — each phase builds directly on the previous one (serial I/O → LLM reply → robustness). No parallel tracks.

---

## v0.1 — Device skeleton and serial

### PYR-001 — Device skeleton and serial

**Description:**
Bring up the AtomS3R + Echo Base in the Arduino IDE and turn the USB-CDC port into a line-oriented text channel — the I/O path that exists before any AI. Establishes the `debug_serial` logging convention reused by every later version.

**What needs to be done:**
- Create the firmware sketch in the **Arduino IDE** with M5Unified; select the AtomS3R + Echo Base board and flash a boot-message baseline.
- Initialize the board: display on, button, status line on the LCD.
- Bring up Wi-Fi from credentials in a config header (`wifi_ssid`, `wifi_pass`); print connection state to serial.
- Open USB-CDC serial at **115200**; implement a non-blocking line reader (buffer until `\n`).
- Parse a completed line into a `text_in` event; route all logs through a `logf()` helper gated by `debug_serial`.
- Echo received lines back so the round-trip is visible.

**Dependencies:** None

**Expected result:**
A device that boots, connects to Wi-Fi, and round-trips typed lines over serial, with gated debug logging.

**Acceptance criteria:**
- [ ] Sketch builds in the Arduino IDE and flashes to AtomS3R + Echo Base
- [ ] Wi-Fi connects from the config header; connection state is logged to serial
- [ ] A typed line in the serial monitor reaches the parser as a `text_in` event
- [ ] The firmware writes a response line back (echo)
- [ ] The `debug_serial` flag toggles status logging on/off
- [ ] The line reader / parser is pure and host-testable (formally covered by the v1 native test env); manual on-device check passes

---

## v0.2 — Text chat loop

### PYR-002 — Text chat loop

**Description:**
Make a direct HTTPS call from the device to a cloud LLM, sending the persona system prompt plus the user's `text_in`, and print the model's `reply` to serial. The API key lives in the firmware config — acceptable only under the private allowlist model (see ARCHITECTURE §Security, auth, and secrets).

**What needs to be done:**
- Add an HTTPS client (TLS) and a minimal JSON builder/parser for the chosen LLM's chat API.
- Store the persona system prompt, model name, endpoint, and API key in the firmware config header.
- Build the request: `system = persona`, `messages = [{user: text_in}]`; send and read the response.
- Extract `reply.text` and print it to serial; surface HTTP/JSON errors as a readable line.
- Keep the call synchronous (one turn at a time); show a "thinking…" log while waiting.

**Dependencies:** PYR-001

**Expected result:**
A full back-and-forth text dialogue in Ukrainian, directly from the device over serial.

**Acceptance criteria:**
- [ ] Direct HTTPS (TLS) call to the cloud LLM succeeds
- [ ] Persona system prompt, model, endpoint, and key are loaded from the firmware config
- [ ] A typed prompt returns the model's reply on serial, in Ukrainian
- [ ] HTTP/JSON errors surface as a readable line without crashing the loop
- [ ] A multi-turn dialogue works end to end over serial
- [ ] JSON request/response building is unit-tested against a recorded/mock LLM response

---

## v0.3 — Quality and UX

### PYR-003 — Quality and UX

**Description:**
Harden the text loop for normal use: a short rolling history so replies have context, graceful handling of timeouts and API errors, and automatic Wi-Fi recovery. Optional LCD hints make the device's state legible without the serial monitor.

**What needs to be done:**
- Keep a short in-RAM conversation history (last N turns) and include it in each LLM request; trim by turn count or a rough token budget.
- Handle request timeouts and API errors: bounded retry, then a clear error line; never hang the loop.
- Detect Wi-Fi loss and reconnect with backoff; pause input while offline and report status.
- (Optional) Show coarse states/hints on the LCD: idle / thinking / error.

**Dependencies:** PYR-002

**Expected result:**
A text chat that holds up across typical scenarios — multi-turn context holds, transient failures recover, and a dropped Wi-Fi link returns on its own.

**Acceptance criteria:**
- [ ] Multi-turn context is retained within a session (history window applied to each request)
- [ ] Timeouts and API errors are handled with bounded retry, then a clear message — the loop never hangs
- [ ] Wi-Fi loss auto-recovers with exponential backoff; input is paused while offline and status is reported
- [ ] (Optional) LCD shows idle / thinking / error
- [ ] History windowing and error-mapping logic are unit-tested; a reliability pass over typical scenarios succeeds

---

## v0 scope notes

**Total effort:** ~2–3 weeks for a single developer (a strict chain, little parallelism).

**Critical path:** PYR-001 → PYR-002 → PYR-003 (the whole version is the critical path).

**Firmware toolchain:** Arduino IDE (M5Unified). The PlatformIO migration and the native test env land in v1 (`v1.1`), so v0 host-testable logic (line parser, JSON building, history windowing) should be written as pure functions now and formally covered by tests once the native env exists.

**Companion documents:**
- [ROADMAP.md](../../ROADMAP.md) — v0 goals, per-phase tasks, and DoDs.
- [ARCHITECTURE.md](../../ARCHITECTURE.md) — serial contract, turn lifecycle, error taxonomy, and the testing strategy.
- Generated on upload: `v0-github-report.md` (PYR-xxx → GitHub #), then `v0-execution-report.md`.
