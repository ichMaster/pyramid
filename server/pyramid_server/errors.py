"""Per-stage timeout budgets + provider-failure → ``error.code`` mapping.

ARCHITECTURE §Error handling: ASR ≤ 5 s, LLM first token ≤ 8 s, TTS first audio
≤ 3 s; on breach the turn aborts to an enumerated ``error{code}`` and the device
returns to idle. The session is *not* dropped — the next turn can proceed.
"""

from __future__ import annotations

from .protocol import ErrorCode

# Per-stage budgets (seconds).
ASR_TIMEOUT_S = 5.0
LLM_FIRST_TOKEN_S = 8.0
TTS_FIRST_AUDIO_S = 3.0

# A breach (timeout) per stage.
_TIMEOUT_CODE = {
    "asr": ErrorCode.ASR_FAILED,   # no dedicated asr_timeout in the enumerated set
    "llm": ErrorCode.LLM_TIMEOUT,
    "tts": ErrorCode.TTS_FAILED,
}
# A generic provider failure per stage (no specific code hint).
_FAIL_CODE = {
    "asr": ErrorCode.ASR_FAILED,
    "llm": ErrorCode.LLM_FAILED,
    "tts": ErrorCode.TTS_FAILED,
}


def provider_error_code(stage: str, exc: BaseException) -> str:
    """Map a stage failure to an enumerated ``error.code``.

    Order: a timeout → the stage timeout code; an explicit ``code`` hint on the
    exception (e.g. ``rate_limited`` / ``server_unreachable``) → that code;
    otherwise the stage's generic ``*_failed``.
    """
    if isinstance(exc, TimeoutError):  # asyncio.TimeoutError is TimeoutError (py3.11+)
        return _TIMEOUT_CODE[stage]
    code = getattr(exc, "code", None)
    if code in (
        ErrorCode.RATE_LIMITED,
        ErrorCode.SERVER_UNREACHABLE,
        ErrorCode.UNAUTHORIZED,
    ):
        return code
    return _FAIL_CODE[stage]
