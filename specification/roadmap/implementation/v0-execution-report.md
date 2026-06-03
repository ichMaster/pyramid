# Version v0 — Execution Report

**Date:** 2026-06-03
**Branch:** main
**Label:** v0::version:0
**Target version:** 0.1.0 nominal — see release note below
**Executed by:** Claude Code
**Scope:** single-issue runs — `--issue PYR-001`, `--issue PYR-002`, `--issue PYR-003`

## Summary

| Status | Count |
|--------|-------|
| Completed | 3 |
| Failed | 0 |
| Skipped | 0 |
| Remaining | 0 |

> **v0 — Text chat over serial is COMPLETE** (all 3 phases). 🎉
>
> **No auto version bump.** The skill maps v0 → `0.1.0`, but `0.1.0` (partial,
> PYR-001) and `0.2.0` (PYR-002) were already tagged on a minor-bump-per-phase
> cadence. Cut the v0-complete release as the next free minor: `/release-version
> 0.3.0`.

## Issues

| # | PYR ID | Title | Phase | Status | Commit | Files | Tests |
|---|--------|-------|-------|--------|--------|-------|-------|
| 1 | PYR-001 | Device skeleton and serial | v0.1 | completed | 1ef3aed | 7 | pass (host 10 + compile) |
| 2 | PYR-002 | Text chat loop | v0.2 | completed | c53f6cc | 8 | pass (host 7 + 9 regression + compile) |
| 3 | PYR-003 | Quality and UX | v0.3 | completed | 12e4f3e | 10 | pass (host 9+5+8+11 + compile) |

> Note: between PYR-002 and PYR-003 the LLM integration was switched from an
> OpenAI-compatible API to the **Anthropic Messages API** (commit 5de2a58),
> at the user's request. Claude is a listed LLM in ARCHITECTURE §External
> services, so no contract changed.

## Detailed Results

### PYR-001: Device skeleton and serial

**Status:** completed · **Phase:** v0.1 · **GitHub:** [#1](https://github.com/ichMaster/pyramid/issues/1) (closed) · **Commit:** 1ef3aed

**Files (7, new):** `pyramid.ino`, `line_reader.h`, `serial_protocol.h`, `config.example.h`, `test/test_line_reader.cpp`, `README.md`, `.gitignore`.

**Validation:**
- [x] Host unit tests: 10/10 (line reader + text_in parser)
- [x] Firmware compile: `arduino-cli` for `m5stack_atoms3r` → success (flash 32%)
- [x] Contract consistency: serial `text_in` mapping matches ARCHITECTURE §Protocols
- [x] Acceptance criteria: all 6 satisfied
- [ ] On-device behavior (Wi-Fi join, LCD, flash): deferred to manual upload

---

### PYR-002: Text chat loop

**Status:** completed · **Phase:** v0.2 · **GitHub:** [#2](https://github.com/ichMaster/pyramid/issues/2) (closed) · **Commit:** c53f6cc (LLM provider later switched in 5de2a58)

**Files (8 — 2 new, 6 modified):** `chat_api.h` (new), `test/test_chat_api.cpp` (new), `pyramid.ino`, `config.example.h`, `serial_protocol.h`, `test/test_line_reader.cpp`, `README.md`, `.gitignore`.

**Validation:**
- [x] Host unit tests: `test_chat_api` 7/7 (build round-trip, UTF-8, success, API error, malformed, missing/empty); regression green
- [x] Firmware compile: success (flash 36%); HTTPClient + NetworkClientSecure + ArduinoJson resolved
- [x] Contract consistency: LLM call shape + `text_in → LLM → reply` match ARCHITECTURE §Contracts
- [x] Acceptance criteria: all 6 satisfied (JSON build/parse + error handling host-tested)
- [ ] Live: HTTPS success, Ukrainian reply, multi-turn back-and-forth — deferred to manual upload

---

### PYR-003: Quality and UX

**Status:** completed · **Phase:** v0.3 · **GitHub:** [#3](https://github.com/ichMaster/pyramid/issues/3) (closed) · **Commit:** 12e4f3e

**Files (10 — 4 new, 6 modified):** `history.h` (new), `backoff.h` (new), `test/test_history.cpp` (new), `test/test_backoff.cpp` (new), `chat_api.h`, `pyramid.ino`, `config.example.h`, `test/test_chat_api.cpp`, `README.md`, `.gitignore`.

**Validation:**
- [x] Host unit tests: `test_line_reader` 9, `test_history` 5, `test_backoff` 8, `test_chat_api` 11 — all pass
- [x] Firmware compile: success (flash 36%, RAM 15%)
- [x] Contract consistency: rolling RAM-only per-session history matches ARCHITECTURE §Sessions and history; no wire-format change
- [x] Acceptance criteria: all satisfied (history windowing, retry classification, backoff host-tested)
- [ ] Live: multi-turn context, Wi-Fi loss/recovery, LCD visuals, reliability pass — deferred to manual upload

**Notes (whole version):**
- v0 has no `/server` or `/mcp`, so there are no pytest / contract / integration suites; host-testable firmware logic is covered by host C++ tests and folds into PlatformIO's native test env in v1 (`v1.1`).
- Intelligence stays off-device: persona + history shape the request, but no decisions/memory persist on-device. Long-term memory is a v3 server concern.
- Secrets: `config.h` (Wi-Fi creds + LLM key) is gitignored; only `config.example.h` is committed. TLS uses `setInsecure()` in v0 (private allowlist model).

## Next Steps

- **Release v0:** run `/release-version 0.3.0` to cut the v0-complete release (the `0.1.x` slots are already used).
- **v1 — Voice** is the next version: migrate the firmware to **PlatformIO** + add a native test env (`v1.1`), then I2S audio, TTS-first (`v1.2`), ASR (`v1.3`), and states/UX (`v1.4`). Start by drafting `specification/roadmap/implementation/v1-issues.md`, then `/upload-issues` it.
- **On hardware:** flash the firmware to the AtomS3R + Echo Base and run the manual DoD checks for v0.1–v0.3 (Wi-Fi, live Ukrainian chat, multi-turn context, error/offline recovery, LCD states).
