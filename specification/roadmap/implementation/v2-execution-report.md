# Version v2 — Execution Report

**Date:** 2026-06-05
**Branch:** v2-dev
**Label:** v2::version:2
**Phase:** v2.1 — Server proxy
**Target version:** 2.1.0 (not yet released — awaiting confirmation)
**Executed by:** Claude Code

## Summary

| Status | Count |
|--------|-------|
| Completed | 6 |
| Failed | 0 |
| Skipped | 0 |
| Remaining | 0 |

All six v2.1 issues implemented, validated, committed (one per issue), pushed to
`v2-dev`, and closed on GitHub. The phase opened the entire `/server` package,
the `/tests` suite, CI, and converted the firmware into a WSS client.

## Issues

| # | PYR ID | Title | Phase | Status | Commit | Files | Tests |
|---|--------|-------|-------|--------|--------|-------|-------|
| 1 | PYR-018 | Server skeleton: FastAPI + WSS endpoint + scaffolding | v2.1 | completed | 8ba197a | 12 | pass (3) |
| 2 | PYR-019 | WS device↔server contract: codec/router + hello + error.code | v2.1 | completed | f0cfcf8 | 6 | pass (31) |
| 3 | PYR-020 | Server-side ASR→LLM→TTS orchestrator + provider/mock adapters | v2.1 | completed | 3d0620b | 11 | pass (40) |
| 4 | PYR-021 | Sentence-streaming TTS (rec #3) | v2.1 | completed | e8dec67 | 5 | pass (54) |
| 5 | PYR-022 | Per-stage timeouts + enumerated error.code mapping | v2.1 | completed | 1e6a945 | 7 | pass (71) |
| 6 | PYR-023 | Firmware WSS client + early/streaming playback (rec #4) | v2.1 | completed | 67b4f84 | 15 | pass (35 native) |

Test counts for the server (pytest) are cumulative suite totals after each issue;
the final server suite is **71 passing**. Firmware host tests: **35 native cases**
(13 suites incl. the new `test_ws_codec`).

## Detailed Results

### PYR-018: Server skeleton — FastAPI + WSS endpoint + scaffolding
**Status:** completed · **Commit:** 8ba197a
**Files:** `server/pyramid_server/{__init__,config,logging_conf,session,main}.py`, `server/requirements.txt`, `server/.env.example`, `tests/{__init__,test_smoke}.py`, `pyproject.toml`, `.github/workflows/ci.yml`, `CLAUDE.md`
**Validation:** ruff pass · pytest 3 passed (WS connect, ping→pong, Session lifecycle, healthz) · import/uvicorn path checked
**Acceptance:** all 5 criteria met

### PYR-019: WS device↔server contract — codec/router + hello + error.code
**Status:** completed · **Commit:** f0cfcf8
**Files:** `server/pyramid_server/{protocol,router,main}.py`, `tests/fakes/fake_device.py`, `tests/contract/{test_ws_messages,test_error_codes,test_hello,test_idempotency}.py`
**Validation:** ruff pass · pytest 31 passed — every message shape, error-code set == ARCHITECTURE, hello accept/reject+close, idempotency, fake device
**Acceptance:** all 5 criteria met

### PYR-020: Server-side ASR→LLM→TTS orchestrator + provider/mock adapters
**Status:** completed · **Commit:** 3d0620b
**Files:** `server/pyramid_server/providers/{base,mock,real}.py`, `orchestrator.py`, `history.py`, `prompt.py`, `session.py`, `main.py`, `tests/integration/{test_turn_text,test_turn_voice}.py`, `tests/unit/test_history.py`
**Validation:** ruff pass · pytest 40 passed — full text + voice pipelines over mocks with asserted stage order; history windowing; audio never persisted
**Acceptance:** all 5 criteria met. Note: v2.1 firmware uploads the clip post-`listen_stop`, so the real ASR path is batch (one final `asr`); the `asr_partial` contract path is exercised via the mock.

### PYR-021: Sentence-streaming TTS (rec #3)
**Status:** completed · **Commit:** e8dec67
**Files:** `server/pyramid_server/sentence.py`, `orchestrator.py`, `tests/unit/test_sentence_splitter.py`, `tests/integration/test_streaming_tts.py`, `tests/fakes/fake_device.py`
**Validation:** ruff pass · pytest 54 passed — splitter boundary cases (incl. abbreviations, decimals, max-flush, Ukrainian punctuation); first `tts_audio` precedes the final reply delta
**Acceptance:** all 4 criteria met

### PYR-022: Per-stage timeouts + enumerated error.code mapping
**Status:** completed · **Commit:** 1e6a945
**Files:** `server/pyramid_server/errors.py`, `orchestrator.py`, `providers/base.py`, `providers/real.py`, `tests/unit/test_error_mapping.py`, `tests/integration/test_stage_timeouts.py`
**Validation:** ruff pass · pytest 71 passed — every stage timeout → correct code; provider failures mapped; abort rolls back history; no `tts_end` on abort; session survives over WS
**Acceptance:** all 4 criteria met

### PYR-023: Firmware WSS client + early/streaming playback (rec #4)
**Status:** completed · **Commit:** 67b4f84
**Files:** `firmware/src/ws_protocol.h`, `ws_client.{h,cpp}`, `turn.{h,cpp}`, `audio_io.{h,cpp}`, `main.cpp`, `app_state.{h,cpp}`, `config.example.h`, `platformio.ini`, `test/test_ws_codec/test_ws_codec.cpp`; removed `cloud.{h,cpp}`
**Validation:** `pio test -e native` 35/35 (new `test_ws_codec` 10 cases) · `pio run -e atoms3r` SUCCESS (RAM 63.8%, Flash 38.0%)
**Acceptance:** 5/6 criteria met; **on-hardware end-to-end deferred** to a manual DoD check (mic/speaker/WSS need the board)

## Phase DoD (ROADMAP §v2.1)

> The device runs end-to-end through our server, with the same voice experience as v1 and a lower time-to-first-audio — playback starts on the first streamed TTS chunk.

- Server side: **met and tested** — the full turn runs server-side (text + voice) over mocks; sentence-streaming + first-chunk playback verified structurally (first `tts_audio` before LLM-done).
- Firmware side: **implemented + compiles + host-tested**; the literal "runs end-to-end on the device through the server with lower first-audio" is the **deferred manual hardware check** (flash, point at a running `uvicorn`, confirm first-chunk playback vs the v1 baseline).

## Notes / carry-forward

- **Streaming ASR interims (rec #1):** the contract + mock exercise `asr_partial`; the real path is batch because the firmware uploads the clip after `listen_stop`. True streaming ASR (forward chunks to a streaming ASR API) is a later optimization.
- **Real providers** (Deepgram/Anthropic/ElevenLabs) are implemented but not exercised in CI (mocks only — no external AI, no secrets).
- **TLS:** firmware defaults to `ws://` for LAN dev; `wss://` cutover with real certs is v2.7 (deploy).
- The local firmware `config.h` was given placeholder v2.1 server macros for the compile check (gitignored — not committed).

## Next steps

- v2.1 issues are all complete. **Version not bumped** (per policy — no auto-bump). To release: `/release-version 2.1.0`.
- Hardware bring-up: flash a board, run `uvicorn pyramid_server.main:app --app-dir server` with real keys in `server/.env`, set the firmware `config.h` server host/token, and confirm the end-to-end voice loop + first-chunk playback.
- Next phase: **v2.2 — Role, Name & Canon** (replaces the firmware-era persona seam with the server-side Role; adds `memory_type` ahead of v2.4 memory).
