# Version v0 — GitHub Issues Report

**Uploaded:** 2026-06-03
**Repository:** https://github.com/ichMaster/pyramid
**Total issues:** 3

## Issue Mapping

| PYR ID | GitHub # | Title | Phase | Labels | URL |
|--------|----------|-------|-------|--------|-----|
| PYR-001 | #1 | Device skeleton and serial | v0.1 | v0::version:0, v0::size:M, v0::area:firmware | https://github.com/ichMaster/pyramid/issues/1 |
| PYR-002 | #2 | Text chat loop | v0.2 | v0::version:0, v0::size:M, v0::area:firmware | https://github.com/ichMaster/pyramid/issues/2 |
| PYR-003 | #3 | Quality and UX | v0.3 | v0::version:0, v0::size:M, v0::area:firmware | https://github.com/ichMaster/pyramid/issues/3 |

## Dependencies

- #2 (PYR-002) blocked by #1 (PYR-001)
- #3 (PYR-003) blocked by #2 (PYR-002)

v0 is a strict chain: PYR-001 → PYR-002 → PYR-003 (no parallel tracks).

## Labels Created

- v0::version:0 — Version v0 — Text chat over serial
- v0::size:S, v0::size:M, v0::size:L
- v0::area:firmware — Firmware — AtomS3R + Echo Base (Arduino IDE)
