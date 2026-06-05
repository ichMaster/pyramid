"""Unit: provider/connection failures → enumerated error.code values. (PYR-022.)"""

from __future__ import annotations

import pytest

from pyramid_server.errors import provider_error_code
from pyramid_server.protocol import ERROR_CODES, ErrorCode
from pyramid_server.providers.base import ProviderError


@pytest.mark.parametrize(
    "stage,expected",
    [("asr", ErrorCode.ASR_FAILED), ("llm", ErrorCode.LLM_TIMEOUT), ("tts", ErrorCode.TTS_FAILED)],
)
def test_timeout_maps_per_stage(stage, expected):
    assert provider_error_code(stage, TimeoutError()) == expected


@pytest.mark.parametrize(
    "stage,expected",
    [("asr", ErrorCode.ASR_FAILED), ("llm", ErrorCode.LLM_FAILED), ("tts", ErrorCode.TTS_FAILED)],
)
def test_generic_provider_failure_maps_per_stage(stage, expected):
    assert provider_error_code(stage, ProviderError("boom")) == expected


def test_explicit_code_hint_wins():
    assert provider_error_code("llm", ProviderError("x", code=ErrorCode.RATE_LIMITED)) == ErrorCode.RATE_LIMITED
    assert provider_error_code("tts", ProviderError("x", code=ErrorCode.SERVER_UNREACHABLE)) == ErrorCode.SERVER_UNREACHABLE
    assert provider_error_code("asr", ProviderError("x", code=ErrorCode.UNAUTHORIZED)) == ErrorCode.UNAUTHORIZED


def test_all_mapped_codes_are_enumerated():
    for stage in ("asr", "llm", "tts"):
        assert provider_error_code(stage, TimeoutError()) in ERROR_CODES
        assert provider_error_code(stage, ProviderError("x")) in ERROR_CODES
