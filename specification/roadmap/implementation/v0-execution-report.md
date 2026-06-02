# Version v0 — Execution Report

**Date:** 2026-06-03
**Branch:** main
**Label:** v0::version:0
**Target version:** 0.1.0 (see note below)
**Executed by:** Claude Code
**Scope of these runs:** single-issue — `--issue PYR-001`, then `--issue PYR-002`

## Summary

| Status | Count |
|--------|-------|
| Completed | 2 |
| Failed | 0 |
| Skipped | 0 |
| Remaining | 1 |

> v0 is **incomplete** — PYR-003 remains. No new version bump from these runs.
> Note: `0.1.0` was already tagged early (as a partial milestone, PYR-001 only)
> via `/release-version`, so it no longer maps cleanly to "v0 complete". When
> PYR-003 lands, cut the completion release under a fresh patch (e.g. `0.1.1`).

## Issues

| # | PYR ID | Title | Phase | Status | Commit | Files | Tests |
|---|--------|-------|-------|--------|--------|-------|-------|
| 1 | PYR-001 | Device skeleton and serial | v0.1 | completed | 1ef3aed | 7 | pass (host 10/10 + compile) |
| 2 | PYR-002 | Text chat loop | v0.2 | completed | c53f6cc | 8 | pass (host 7/7 + 9 regression + compile) |
| 3 | PYR-003 | Quality and UX | v0.3 | remaining | — | — | — |

## Detailed Results

### PYR-001: Device skeleton and serial

**Status:** completed
**Phase:** v0.1
**GitHub:** [#1](https://github.com/ichMaster/pyramid/issues/1) (closed)
**Commit:** 1ef3aed

**Files changed (7, all new):**
- `firmware/pyramid/pyramid.ino` — board + Wi-Fi + serial glue (the sketch)
- `firmware/pyramid/line_reader.h` — pure non-blocking line reader
- `firmware/pyramid/serial_protocol.h` — pure `text_in` parse
- `firmware/pyramid/config.example.h` — config template (`config.h` gitignored)
- `firmware/test/test_line_reader.cpp` — host unit test
- `firmware/README.md` — build/flash + host-test instructions
- `firmware/.gitignore` — ignore `config.h` and build artifacts

**Validation:**
- [x] Host unit tests: 10/10 (single line, no-newline, CRLF, multi-line, empty line, bounded overflow, reset, trim, blank-reject, UTF-8 echo)
- [x] Firmware compile: `arduino-cli compile --fqbn m5stack:esp32:m5stack_atoms3r pyramid` → success (flash 32%, RAM 14%)
- [x] Contract consistency: serial `text_in` mapping matches ARCHITECTURE §Protocols/§Contracts
- [x] Acceptance criteria: all 6 satisfied
- [ ] On-device behavior (Wi-Fi join, LCD, flash-to-board): deferred to manual upload

---

### PYR-002: Text chat loop

**Status:** completed
**Phase:** v0.2
**GitHub:** [#2](https://github.com/ichMaster/pyramid/issues/2) (closed)
**Commit:** c53f6cc

**Files changed (8 — 2 new, 6 modified):**
- `firmware/pyramid/chat_api.h` (new) — pure ArduinoJson v7 request build / reply parse + error mapping
- `firmware/pyramid/pyramid.ino` — `llmTurn()` synchronous HTTPS POST (TLS) + reply/error handling in `loop()`
- `firmware/pyramid/config.example.h` — `LLM_ENDPOINT`/`LLM_MODEL`/`LLM_API_KEY`/`LLM_PERSONA` (Ukrainian)
- `firmware/pyramid/serial_protocol.h` — dropped the obsolete v0.1 echo helper
- `firmware/test/test_chat_api.cpp` (new) — 7 host checks vs recorded mock responses
- `firmware/test/test_line_reader.cpp` — removed the obsolete `formatReply` case
- `firmware/README.md`, `firmware/.gitignore` — v0.2 docs / ignore new test binary

**Validation:**
- [x] Host unit tests: `test_chat_api` 7/7 (build round-trip, quotes/UTF-8, success, API error, malformed, missing/empty); `test_line_reader` regression 9 pass
- [x] Firmware compile: `arduino-cli compile --fqbn m5stack:esp32:m5stack_atoms3r pyramid` → success (flash 36%, RAM 15%); HTTPClient + NetworkClientSecure + ArduinoJson 7.0.4 resolved
- [x] Contract consistency: LLM call shape + `text_in → LLM → reply` match ARCHITECTURE §Contracts (no wire-format change)
- [x] Acceptance criteria: all 6 satisfied (JSON build/parse host-tested; error handling host-tested)
- [ ] On-device / live: HTTPS success, the reply being in Ukrainian, multi-turn back-and-forth — deferred to manual upload (needs board + real key + network)

**Notes:**
- v0 has no `/server` or `/mcp` yet, so there are no pytest / contract / integration suites for these issues. Host-testable firmware logic is covered by host C++ tests now and folds into PlatformIO's native test env in v1.
- Intelligence stays off-device: the persona is config (`LLM_PERSONA`), not logic; no memory/decisions on-device. Rolling history + Wi-Fi auto-reconnect are PYR-003.
- Secrets: `config.h` (Wi-Fi creds + LLM key) is gitignored; only `config.example.h` is committed. TLS uses `setInsecure()` in v0 (private allowlist model).

## Next Steps

- **PYR-003 — Quality and UX (v0.3):** depends on PYR-002 (now closed → unblocked). Rolling in-RAM history window included in each request; bounded retry on timeout/API error then a clear line (never hang); Wi-Fi loss detection + reconnect with exponential backoff (pause input while offline); optional LCD idle/thinking/error states. Tests: history windowing + error-mapping logic.
- Run `/execute-issues v0::version:0 --issue PYR-003` to finish v0.
- On completion, cut the v0-complete release as a fresh patch (`0.1.1`) since `0.1.0` was already used for the partial PYR-001 milestone.
