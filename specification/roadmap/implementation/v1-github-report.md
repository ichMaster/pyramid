# Version v1 — GitHub Issues Report

**Uploaded:** 2026-06-03
**Repository:** https://github.com/ichMaster/pyramid
**Scope:** phase **v1.1 — Audio I/O and PlatformIO migration** (from `v1.1-issues.md`)
**Total issues:** 4

> v1.2–v1.4 are not yet broken into issues; this report covers v1.1 only. As
> later phases are uploaded, append their rows here.

## Issue Mapping

| PYR ID | GitHub # | Title | Phase | Labels | URL |
|--------|----------|-------|-------|--------|-----|
| PYR-004 | #4 | PlatformIO migration | v1.1 | v1::version:1, v1::size:M, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/4 |
| PYR-005 | #5 | Native test environment | v1.1 | v1::version:1, v1::size:S, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/5 |
| PYR-006 | #6 | I2S audio capture (push-to-talk) | v1.1 | v1::version:1, v1::size:M, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/6 |
| PYR-007 | #7 | I2S audio playback (record → playback loop) | v1.1 | v1::version:1, v1::size:M, v1::area:firmware | https://github.com/ichMaster/pyramid/issues/7 |

## Dependencies

- #5 (PYR-005) blocked by #4 (PYR-004)
- #6 (PYR-006) blocked by #4 (PYR-004)
- #7 (PYR-007) blocked by #4 (PYR-004) and #6 (PYR-006)

PYR-004 gates the phase; after it, PYR-005 and PYR-006 can run in parallel, and PYR-006 + PYR-007 together deliver the v1.1 DoD (press-records / release-plays-back).

## Labels Created

- v1::version:1 — Version v1 — Voice
- v1::size:S, v1::size:M, v1::size:L
- v1::area:firmware — Firmware — AtomS3R + Echo Base (PlatformIO)
