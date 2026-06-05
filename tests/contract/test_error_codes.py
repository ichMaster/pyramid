"""Contract test: the enumerated ``error.code`` set equals the ARCHITECTURE set
exactly — a guard against silent drift. (ARCHITECTURE §Error handling.)"""

from __future__ import annotations

import pytest

from pyramid_server import protocol as p

# The canonical set, copied verbatim from ARCHITECTURE §Error handling.
ARCHITECTURE_ERROR_CODES = {
    "wifi_lost",
    "server_unreachable",
    "proto_unsupported",
    "unauthorized",
    "rate_limited",
    "asr_failed",
    "llm_timeout",
    "llm_failed",
    "tts_failed",
    "internal",
}


def test_error_code_set_matches_architecture_exactly():
    assert set(p.ERROR_CODES) == ARCHITECTURE_ERROR_CODES


def test_error_builder_rejects_unenumerated_code():
    with pytest.raises(ValueError):
        p.error("made_up_code", "nope")


def test_error_builder_accepts_each_enumerated_code():
    for code in ARCHITECTURE_ERROR_CODES:
        assert p.error(code)["code"] == code
