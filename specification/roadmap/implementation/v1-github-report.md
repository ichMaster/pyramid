# Version v1 — GitHub Issues Report

**Uploaded:** 2026-06-03
**Repository:** https://github.com/ichMaster/pyramid
**Scope:** phases **v1.1** (Audio I/O + PlatformIO), **v1.2** (TTS output), **v1.3** (ASR / full voice loop), **v1.4** (states / UX)
**Total issues:** 14
**Last updated:** 2026-06-03 — appended v1.4 (PYR-014…017)

## Issue Mapping

| PYR ID | GitHub # | Title | Phase | Labels | URL |
|--------|----------|-------|-------|--------|-----|
| PYR-004 | #4 | PlatformIO migration | v1.1 | v1::version:1, v1::size:M, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/4 |
| PYR-005 | #5 | Native test environment | v1.1 | v1::version:1, v1::size:S, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/5 |
| PYR-006 | #6 | I2S audio capture (push-to-talk) | v1.1 | v1::version:1, v1::size:M, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/6 |
| PYR-007 | #7 | I2S audio playback (record → playback loop) | v1.1 | v1::version:1, v1::size:M, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/7 |
| PYR-008 | #8 | Cloud TTS client (Ukrainian → PCM16) | v1.2 | v1::version:1, v1::size:M, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/8 |
| PYR-009 | #9 | TTS → playback pipeline (serial → LLM → TTS → speaker) | v1.2 | v1::version:1, v1::size:M, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/9 |
| PYR-010 | #10 | TTS robustness (timeout / fallback / max length) | v1.2 | v1::version:1, v1::size:S, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/10 |
| PYR-011 | #11 | Cloud ASR client (Ukrainian PCM16 → transcript) | v1.3 | v1::version:1, v1::size:M, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/11 |
| PYR-012 | #12 | Full voice loop (button → ASR → LLM → TTS → speaker) | v1.3 | v1::version:1, v1::size:M, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/12 |
| PYR-013 | #13 | ASR robustness (empty/failed recognition, timeout, noise gate) | v1.3 | v1::version:1, v1::size:S, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/13 |
| PYR-014 | #14 | Turn-state machine + LCD states | v1.4 | v1::version:1, v1::size:S, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/14 |
| PYR-015 | #15 | Pause-based end-of-utterance (VAD) | v1.4 | v1::version:1, v1::size:M, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/15 |
| PYR-016 | #16 | Mid-turn resilience (Wi-Fi loss + per-stage timeouts) | v1.4 | v1::version:1, v1::size:S, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/16 |
| PYR-017 | #17 | Latency hardening (pre-warm ASR TLS + timing attribution) | v1.4 | v1::version:1, v1::size:S, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/17 |

## Dependencies

**v1.1**
- #5 (PYR-005) blocked by #4 (PYR-004)
- #6 (PYR-006) blocked by #4 (PYR-004)
- #7 (PYR-007) blocked by #4 (PYR-004) and #6 (PYR-006)

PYR-004 gates v1.1; after it, PYR-005 and PYR-006 run in parallel, and PYR-006 + PYR-007 deliver the v1.1 DoD (press-records / release-plays-back).

**v1.2**
- #9 (PYR-009) blocked by #8 (PYR-008)
- #10 (PYR-010) blocked by #9 (PYR-009)

v1.2 is a chain (client → pipeline → robustness); the DoD (typed prompt → spoken Ukrainian reply) is met at PYR-009.

**v1.3**
- #12 (PYR-012) blocked by #11 (PYR-011)
- #13 (PYR-013) blocked by #12 (PYR-012)

v1.3 is a chain (ASR client → voice loop → robustness); the DoD (speak → hear a Ukrainian reply) is met at PYR-012. Provider decided: Deepgram (raw PCM16).

**v1.4**
- #15 (PYR-015) blocked by #14 (PYR-014)
- #16 (PYR-016) blocked by #14 (PYR-014)
- #17 (PYR-017) blocked by #14 (PYR-014)

PYR-014 (turn-state machine) is the foundation; once it lands, PYR-015/016/017 run in parallel. The DoD (state always visible + reliable + correct/lower latency) is met when all four are done; PYR-015 (VAD) carries the bulk.

## Labels Created

- v1::version:1 — Version v1 — Voice
- v1::size:S, v1::size:M, v1::size:L
- v1::area:firmware — Firmware — AtomS3R + Echo Base (PlatformIO)
