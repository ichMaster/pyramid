"""The device↔server wire contract — the integration seam that must not drift.

Pure (no I/O): message-type constants, the enumerated ``error.code`` set, a
text-frame codec, and ``hello`` proto-version negotiation. Binary frames carry
raw **PCM16 16 kHz mono** with no JSON envelope (device→server = ``audio``,
server→device = ``tts_audio``); their meaning is fixed by direction + state.

Pinned by the contract tests in ``tests/contract/`` — changing anything here
changes a test (ARCHITECTURE §WS device↔server, §Error handling, §Testing).
"""

from __future__ import annotations

import json
from dataclasses import dataclass

# --- audio format (the only one in v2.1; OPUS is deferred) -------------------
AUDIO_SAMPLE_RATE = 16000
AUDIO_CHANNELS = 1
AUDIO_FORMAT = "pcm16"


# --- message types -----------------------------------------------------------
class In:
    """device → server message types (text frames)."""

    HELLO = "hello"
    LISTEN_START = "listen_start"
    LISTEN_STOP = "listen_stop"
    TEXT_IN = "text_in"
    PING = "ping"
    # `audio` is a *binary* frame (no JSON envelope).


class Out:
    """server → device message types (text frames)."""

    ASR_PARTIAL = "asr_partial"
    ASR = "asr"
    REPLY = "reply"
    TEXT_OUT = "text_out"
    TTS_END = "tts_end"
    ERROR = "error"
    CONFIG_UPDATED = "config_updated"
    RESTART = "restart"
    PONG = "pong"
    # `tts_audio` is a *binary* frame (no JSON envelope).


IN_TYPES = frozenset(
    {In.HELLO, In.LISTEN_START, In.LISTEN_STOP, In.TEXT_IN, In.PING}
)
OUT_TYPES = frozenset(
    {
        Out.ASR_PARTIAL, Out.ASR, Out.REPLY, Out.TEXT_OUT, Out.TTS_END,
        Out.ERROR, Out.CONFIG_UPDATED, Out.RESTART, Out.PONG,
    }
)


# --- enumerated error codes (ARCHITECTURE §Error handling) -------------------
class ErrorCode:
    WIFI_LOST = "wifi_lost"
    SERVER_UNREACHABLE = "server_unreachable"
    PROTO_UNSUPPORTED = "proto_unsupported"
    UNAUTHORIZED = "unauthorized"
    RATE_LIMITED = "rate_limited"
    ASR_FAILED = "asr_failed"
    LLM_TIMEOUT = "llm_timeout"
    LLM_FAILED = "llm_failed"
    TTS_FAILED = "tts_failed"
    INTERNAL = "internal"


ERROR_CODES = frozenset(
    {
        ErrorCode.WIFI_LOST, ErrorCode.SERVER_UNREACHABLE, ErrorCode.PROTO_UNSUPPORTED,
        ErrorCode.UNAUTHORIZED, ErrorCode.RATE_LIMITED, ErrorCode.ASR_FAILED,
        ErrorCode.LLM_TIMEOUT, ErrorCode.LLM_FAILED, ErrorCode.TTS_FAILED,
        ErrorCode.INTERNAL,
    }
)


# --- inbound text codec ------------------------------------------------------
class ProtocolError(ValueError):
    """Malformed inbound frame (not valid JSON, or missing/unknown ``type``)."""


@dataclass(frozen=True)
class Message:
    """A decoded inbound text frame."""

    type: str
    data: dict

    def get(self, key: str, default=None):
        return self.data.get(key, default)


def decode_text(raw: str) -> Message:
    """Parse an inbound text frame. Raises :class:`ProtocolError` if malformed."""
    try:
        obj = json.loads(raw)
    except (ValueError, TypeError) as exc:
        raise ProtocolError(f"not valid JSON: {exc}") from exc
    if not isinstance(obj, dict):
        raise ProtocolError("frame is not a JSON object")
    mtype = obj.get("type")
    if not isinstance(mtype, str) or mtype not in IN_TYPES:
        raise ProtocolError(f"missing/unknown type: {mtype!r}")
    return Message(type=mtype, data=obj)


# --- outbound text builders --------------------------------------------------
def encode(msg: dict) -> str:
    return json.dumps(msg, ensure_ascii=False)


def pong() -> dict:
    return {"type": Out.PONG}


def asr_partial(text: str) -> dict:
    return {"type": Out.ASR_PARTIAL, "text": text}


def asr(text: str) -> dict:
    return {"type": Out.ASR, "text": text}


def reply(text: str, *, delta: bool = False, done: bool = False) -> dict:
    """A reply message. ``delta=True`` marks a streamed token chunk; ``done``
    marks the final delta (ARCHITECTURE: ``reply{text}`` may stream as deltas)."""
    return {"type": Out.REPLY, "text": text, "delta": delta, "done": done}


def text_out(text: str) -> dict:
    return {"type": Out.TEXT_OUT, "text": text}


def tts_end() -> dict:
    return {"type": Out.TTS_END}


def error(code: str, msg: str = "") -> dict:
    if code not in ERROR_CODES:
        raise ValueError(f"error code not in the enumerated set: {code!r}")
    return {"type": Out.ERROR, "code": code, "msg": msg}


def config_updated() -> dict:
    return {"type": Out.CONFIG_UPDATED}


def restart() -> dict:
    return {"type": Out.RESTART}


# --- hello / proto negotiation ----------------------------------------------
def parse_proto_ver(value) -> tuple[int, int]:
    """Parse a ``proto_ver`` of form ``"major.minor"`` or ``[major, minor]``."""
    if isinstance(value, (list, tuple)) and len(value) == 2:
        return int(value[0]), int(value[1])
    if isinstance(value, str) and "." in value:
        major, _, minor = value.partition(".")
        return int(major), int(minor)
    raise ProtocolError(f"bad proto_ver: {value!r}")


def negotiate(client_proto, server_major: int) -> tuple[bool, tuple[int, int] | None]:
    """Return ``(ok, parsed_ver)``. The client's *major* must equal the server's;
    an unknown major is rejected (caller emits ``error{proto_unsupported}``)."""
    try:
        ver = parse_proto_ver(client_proto)
    except ProtocolError:
        return False, None
    return (ver[0] == server_major), ver
