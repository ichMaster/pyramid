"""Structured logging keyed by ``session_id`` / ``turn_id``.

Observability per ARCHITECTURE §Cross-cutting concerns: every line carries the
session (and, mid-turn, the turn) it belongs to, so a conversation can be traced
end to end. Serial stays the device-side debug channel.
"""

from __future__ import annotations

import json
import logging
import sys
import time

_CONFIGURED = False


class _JsonFormatter(logging.Formatter):
    """Render records as one JSON object per line."""

    def format(self, record: logging.LogRecord) -> str:
        payload = {
            "ts": round(record.created, 3),
            "level": record.levelname,
            "logger": record.name,
            "msg": record.getMessage(),
        }
        for key in ("session_id", "turn_id"):
            value = getattr(record, key, None)
            if value is not None:
                payload[key] = value
        if record.exc_info:
            payload["exc"] = self.formatException(record.exc_info)
        return json.dumps(payload, ensure_ascii=False)


def configure_logging(level: str = "INFO") -> None:
    """Install the JSON formatter on the root logger (idempotent)."""
    global _CONFIGURED
    if _CONFIGURED:
        return
    handler = logging.StreamHandler(sys.stderr)
    handler.setFormatter(_JsonFormatter())
    root = logging.getLogger()
    root.handlers[:] = [handler]
    root.setLevel(level)
    _CONFIGURED = True


def get_logger(name: str = "pyramid") -> logging.Logger:
    return logging.getLogger(name)


def bind(logger: logging.Logger, **context: object) -> logging.LoggerAdapter:
    """Return a logger adapter that stamps ``session_id`` / ``turn_id`` on every line."""
    return logging.LoggerAdapter(logger, dict(context))


def now_ms() -> int:
    """Monotonic-ish wall clock in ms (for turn ids / latency)."""
    return int(time.time() * 1000)
