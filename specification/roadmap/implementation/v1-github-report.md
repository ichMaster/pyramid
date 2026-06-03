# Version v1 — GitHub Issues Report

**Uploaded:** 2026-06-03
**Repository:** https://github.com/ichMaster/pyramid
**Scope:** phases **v1.1** (Audio I/O + PlatformIO) and **v1.2** (TTS output)
**Total issues:** 7

> v1.3 (ASR) and v1.4 (states/UX) are not yet broken into issues; append their
> rows here when uploaded.

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

## Labels Created

- v1::version:1 — Version v1 — Voice
- v1::size:S, v1::size:M, v1::size:L
- v1::area:firmware — Firmware — AtomS3R + Echo Base (PlatformIO)
