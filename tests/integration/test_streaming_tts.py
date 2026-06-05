"""Integration: sentence-streaming TTS (PYR-021, rec #3).

The first ``tts_audio`` frame must be emitted *before* the LLM finishes streaming
(phrase-by-phrase), and the full ``audio → tts_end`` turn must still pass.
"""

from __future__ import annotations

from pyramid_server.orchestrator import Orchestrator
from pyramid_server.providers.mock import MockASR, MockLLM, MockTTS
from pyramid_server.router import TurnContext
from pyramid_server.session import Session
from tests.fakes.fake_device import RecordingTransport

MULTI = "Перше речення. Друге речення. Третє речення."


async def test_first_tts_audio_before_llm_done():
    orch = Orchestrator(MockASR(), MockLLM(reply=MULTI), MockTTS())
    tx = RecordingTransport()
    ctx = TurnContext(Session(), tx)

    await orch.run_text(ctx, "привіт")

    events = tx.events
    first_audio = next(i for i, (k, _) in enumerate(events) if k == "bytes")
    done_idx = next(
        i for i, (k, p) in enumerate(events)
        if k == "text" and isinstance(p, dict) and p.get("type") == "reply" and p.get("done")
    )
    # Streaming: audio starts before the final (done) reply delta. With the old
    # buffered approach every tts_audio came *after* done — so this distinguishes them.
    assert first_audio < done_idx


async def test_each_sentence_synthesized_separately():
    tts = MockTTS()
    orch = Orchestrator(MockASR(), MockLLM(reply=MULTI), tts)
    ctx = TurnContext(Session(), RecordingTransport())
    await orch.run_text(ctx, "привіт")
    # Three sentences → three TTS synth calls (phrase-by-phrase).
    assert tts.synth_calls == ["Перше речення.", "Друге речення.", "Третє речення."]


async def test_full_turn_still_completes_no_regression():
    tx = RecordingTransport()
    orch = Orchestrator(MockASR(), MockLLM(reply=MULTI), MockTTS())
    ctx = TurnContext(Session(), tx)
    await orch.run_text(ctx, "привіт")

    types = [p.get("type") for k, p in tx.events if k == "text"]
    assert types[-1] == "tts_end"
    deltas = [p["text"] for k, p in tx.events if k == "text" and p.get("type") == "reply"]
    assert "".join(deltas) == MULTI
    assert any(k == "bytes" for k, _ in tx.events)


async def test_short_reply_synthesized_once():
    tts = MockTTS()
    orch = Orchestrator(MockASR(), MockLLM(reply="Ок"), tts)
    ctx = TurnContext(Session(), RecordingTransport())
    await orch.run_text(ctx, "привіт")
    # No internal boundary → one synth call for the whole short reply (fallback).
    assert tts.synth_calls == ["Ок"]
