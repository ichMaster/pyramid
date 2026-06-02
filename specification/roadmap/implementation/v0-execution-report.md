# Version v0 — Execution Report

**Date:** 2026-06-03
**Branch:** main
**Label:** v0::version:0
**Target version:** 0.1.0
**Executed by:** Claude Code
**Scope of this run:** single issue — `--issue PYR-001`

## Summary

| Status | Count |
|--------|-------|
| Completed | 1 |
| Failed | 0 |
| Skipped | 0 |
| Remaining | 2 |

> v0 is **incomplete** — only PYR-001 was executed in this run. No version bump
> or tag (the `0.1.0` release happens once PYR-002 and PYR-003 are also done).

## Issues

| # | PYR ID | Title | Phase | Status | Commit | Files | Tests |
|---|--------|-------|-------|--------|--------|-------|-------|
| 1 | PYR-001 | Device skeleton and serial | v0.1 | completed | 1ef3aed | 7 | pass (host 10/10 + compile) |
| 2 | PYR-002 | Text chat loop | v0.2 | remaining | — | — | — |
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
- `firmware/pyramid/serial_protocol.h` — pure `text_in` parse + echo formatter
- `firmware/pyramid/config.example.h` — config template (`config.h` gitignored)
- `firmware/test/test_line_reader.cpp` — host unit test (10 checks)
- `firmware/README.md` — build/flash + host-test instructions
- `firmware/.gitignore` — ignore `config.h` and build artifacts

**Validation:**
- [x] Host unit tests: `c++ -std=c++17 -I../pyramid test_line_reader.cpp` → `ok - all tests passed` (10/10: single line, no-newline, CRLF, multi-line, empty line, bounded overflow, reset, trim, blank-reject, UTF-8 echo)
- [x] Firmware compile: `arduino-cli compile --fqbn m5stack:esp32:m5stack_atoms3r pyramid` → success (flash 32%, RAM 14%); M5Unified 0.2.16, m5stack:esp32 3.3.7
- [x] Contract consistency: serial `text_in` mapping matches ARCHITECTURE §Protocols / §Contracts (v0 plain-text channel; one line == one `text_in`)
- [x] Acceptance criteria: all 6 satisfied (see issue comment)
- [ ] On-device behavior (Wi-Fi join, LCD, flash-to-board): **deferred to manual upload** — needs the board (ROADMAP §v0.1 DoD)

**Notes:**
- v0 has no `/server` or `/mcp` yet, so there are no pytest / contract / integration suites for this issue. Host-testable firmware logic (line reader, `text_in` parser) is covered by a host C++ test now and folds into PlatformIO's native test env in v1 (`v1.1`).
- Intelligence stays off-device: v0.1 only echoes; no persona/LLM logic (that is PYR-002).
- Secrets: `config.h` (Wi-Fi creds; LLM key from v0.2) is gitignored; only `config.example.h` is committed.

## Next Steps

- **PYR-002 — Text chat loop (v0.2):** depends on PYR-001 (now closed → unblocked). Direct HTTPS call to a cloud LLM (persona + `text_in`), print `reply` to serial; JSON build/parse unit-tested against a mock response.
- **PYR-003 — Quality and UX (v0.3):** depends on PYR-002. Rolling history window, bounded retry on timeout/API error, Wi-Fi auto-reconnect with backoff, optional LCD states.
- Run `/execute-issues v0::version:0` (or `--issue PYR-002`) to continue. When all three are complete, bump to **0.1.0** and tag (via `/release-version 0.1.0`).
