# Version v2 — GitHub Issues Report

**Uploaded:** 2026-06-05
**Repository:** https://github.com/ichMaster/pyramid
**Source file:** [v2.1-issues.md](v2.1-issues.md)
**Phase:** v2.1 — Server proxy
**Total issues:** 6

## Issue Mapping

| PYR ID | GitHub # | Title | Phase | Labels | URL |
|--------|----------|-------|-------|--------|-----|
| PYR-018 | #18 | Server skeleton: FastAPI + WSS endpoint + scaffolding | v2.1 | v2::version:2, v2::size:M, v2::area:server | https://github.com/ichMaster/pyramid/issues/18 |
| PYR-019 | #19 | WS device↔server contract: codec/router + hello + error.code set | v2.1 | v2::version:2, v2::size:M, v2::area:server | https://github.com/ichMaster/pyramid/issues/19 |
| PYR-020 | #20 | Server-side ASR→LLM→TTS orchestrator + provider/mock adapters | v2.1 | v2::version:2, v2::size:L, v2::area:server | https://github.com/ichMaster/pyramid/issues/20 |
| PYR-021 | #21 | Sentence-streaming TTS (rec #3) | v2.1 | v2::version:2, v2::size:M, v2::area:server | https://github.com/ichMaster/pyramid/issues/21 |
| PYR-022 | #22 | Per-stage timeouts + enumerated error.code mapping | v2.1 | v2::version:2, v2::size:S, v2::area:server | https://github.com/ichMaster/pyramid/issues/22 |
| PYR-023 | #23 | Firmware WSS client + early/streaming playback (rec #4) | v2.1 | v2::version:2, v2::size:L, v2::area:firmware | https://github.com/ichMaster/pyramid/issues/23 |

## Dependencies (Blocked by)

| Issue | Blocked by |
|-------|------------|
| #19 (PYR-019) | #18 (PYR-018) |
| #20 (PYR-020) | #19 (PYR-019) |
| #21 (PYR-021) | #20 (PYR-020) |
| #22 (PYR-022) | #20 (PYR-020), #19 (PYR-019) |
| #23 (PYR-023) | #19 (PYR-019), #20 (PYR-020) |

PYR-018 has no dependencies (first `/server` code).

## Labels Created

- `v2::version:2` — Version v2 — Server platform
- `v2::size:S`, `v2::size:M`, `v2::size:L`
- `v2::area:server`, `v2::area:firmware`

## Execution order

Dependency-ordered queue for `/execute-issues v2::version:2`:

```
PYR-018  →  PYR-019  →  PYR-020  →  ┬─ PYR-021
                                    ├─ PYR-022
                                    └─ PYR-023 (also needs PYR-019)
```

Critical path: **PYR-018 → PYR-019 → PYR-020 → PYR-023**. After PYR-020, the server track (PYR-021, PYR-022) and the firmware track (PYR-023) run in parallel; PYR-023's end-to-end hardware verification gates on PYR-020.

## Next step

Implement with `/execute-issues v2::version:2` (dependency order; one commit per issue; tests ship with each feature). Phase DoD: the device runs end-to-end through our server with the same voice experience as v1 and a lower time-to-first-audio.
