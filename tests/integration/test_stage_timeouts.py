"""Integration: per-stage timeouts + error mapping; aborted turn keeps the
session usable. Driven with stalling/failing mocks and tiny budgets. (PYR-022.)"""

from __future__ import annotations

from pyramid_server.main import create_app
from pyramid_server.orchestrator import Orchestrator
from pyramid_server.protocol import ErrorCode
from pyramid_server.providers.mock import FailingMock, MockASR, MockLLM, MockTTS, StallingMock
from pyramid_server.router import TurnContext
from pyramid_server.session import Session
from tests.fakes.fake_device import RecordingTransport, connect

TINY = dict(asr_timeout_s=0.05, llm_first_token_s=0.05, tts_first_audio_s=0.05)


def last_error(tx: RecordingTransport):
    errs = [m for m in tx.messages if m.get("type") == "error"]
    return errs[-1] if errs else None


# --- timeouts ----------------------------------------------------------------
async def test_asr_timeout_emits_asr_failed():
    orch = Orchestrator(StallingMock(), MockLLM(), MockTTS(), **TINY)
    tx = RecordingTransport()
    await orch.run_voice(TurnContext(Session(), tx), b"audio")
    assert last_error(tx)["code"] == ErrorCode.ASR_FAILED


async def test_llm_first_token_timeout_emits_llm_timeout():
    orch = Orchestrator(MockASR(), StallingMock(), MockTTS(), **TINY)
    tx = RecordingTransport()
    await orch.run_text(TurnContext(Session(), tx), "привіт")
    assert last_error(tx)["code"] == ErrorCode.LLM_TIMEOUT


async def test_tts_first_audio_timeout_emits_tts_failed():
    orch = Orchestrator(MockASR(), MockLLM(reply="Привіт."), StallingMock(), **TINY)
    tx = RecordingTransport()
    await orch.run_text(TurnContext(Session(), tx), "привіт")
    assert last_error(tx)["code"] == ErrorCode.TTS_FAILED


# --- provider failures -------------------------------------------------------
async def test_asr_failure_emits_asr_failed():
    orch = Orchestrator(FailingMock(), MockLLM(), MockTTS())
    tx = RecordingTransport()
    await orch.run_voice(TurnContext(Session(), tx), b"audio")
    assert last_error(tx)["code"] == ErrorCode.ASR_FAILED


async def test_llm_failure_emits_llm_failed():
    orch = Orchestrator(MockASR(), FailingMock(), MockTTS())
    tx = RecordingTransport()
    await orch.run_text(TurnContext(Session(), tx), "привіт")
    assert last_error(tx)["code"] == ErrorCode.LLM_FAILED


async def test_tts_failure_emits_tts_failed():
    orch = Orchestrator(MockASR(), MockLLM(reply="Привіт."), FailingMock())
    tx = RecordingTransport()
    await orch.run_text(TurnContext(Session(), tx), "привіт")
    assert last_error(tx)["code"] == ErrorCode.TTS_FAILED


# --- abort hygiene -----------------------------------------------------------
async def test_aborted_turn_rolls_back_user_message():
    orch = Orchestrator(MockASR(), FailingMock(), MockTTS())
    session = Session()
    await orch.run_text(TurnContext(session, RecordingTransport()), "привіт")
    # The user turn is rolled back so history stays consistent.
    assert session.history == []


async def test_no_tts_end_on_abort():
    orch = Orchestrator(MockASR(), FailingMock(), MockTTS())
    tx = RecordingTransport()
    await orch.run_text(TurnContext(Session(), tx), "привіт")
    assert not any(m.get("type") == "tts_end" for m in tx.messages)


def test_session_survives_error_over_ws():
    # After an LLM failure the device gets error{llm_failed}; the socket stays
    # open and the next ping still pongs (session usable for the next turn).
    orch = Orchestrator(MockASR(), FailingMock(), MockTTS())
    app = create_app(orchestrator=orch)
    with connect(app) as dev:
        dev.hello()
        dev.text_in("привіт")
        frames = dev.drain_until("error")
        err = frames[-1][1]
        assert err["code"] == ErrorCode.LLM_FAILED
        dev.ping()
        assert dev.recv_json() == {"type": "pong"}
