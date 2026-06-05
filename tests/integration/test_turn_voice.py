"""Integration: the voice path — ``listen_start → audio → listen_stop`` then
``asr_partial* → asr → reply (deltas) → tts_audio → tts_end``, asserting stage
order and per-stage streaming. (PYR-020.)"""

from __future__ import annotations

from pyramid_server.main import create_app
from pyramid_server.orchestrator import Orchestrator
from pyramid_server.providers.mock import MockASR, MockLLM, MockTTS
from tests.fakes.fake_device import connect


def test_voice_turn_full_pipeline_in_order():
    transcript = "яка сьогодні погода"
    reply = "Сьогодні сонячно."
    orch = Orchestrator(
        MockASR(transcript=transcript, partials=["яка", "яка сьогодні"]),
        MockLLM(reply=reply),
        MockTTS(),
    )
    app = create_app(orchestrator=orch)

    with connect(app) as dev:
        dev.hello()
        dev.listen_start()
        dev.send_audio(b"\x00\x01" * 100)
        dev.send_audio(b"\x02\x03" * 100)
        dev.listen_stop()
        frames = dev.drain_until("tts_end")

    text_types = [p["type"] for k, p in frames if k == "text"]
    # Stage order: partials → final asr → reply(s) → tts_end.
    assert text_types.count("asr_partial") == 2
    assert "asr" in text_types
    first_reply = text_types.index("reply")
    assert text_types.index("asr") < first_reply
    assert text_types[-1] == "tts_end"

    # Final ASR transcript and reconstructed reply.
    asr_final = [p["text"] for k, p in frames if k == "text" and p["type"] == "asr"][0]
    assert asr_final == transcript
    deltas = [p["text"] for k, p in frames if k == "text" and p["type"] == "reply"]
    assert "".join(deltas) == reply

    # Audio streamed before tts_end.
    assert any(k == "bytes" for k, _ in frames)


def test_silence_ends_turn_without_reply():
    # Empty transcript → no LLM/TTS, just a clean tts_end.
    orch = Orchestrator(MockASR(transcript="   "), MockLLM(), MockTTS())
    app = create_app(orchestrator=orch)
    with connect(app) as dev:
        dev.hello()
        dev.listen_start()
        dev.send_audio(b"\x00\x00" * 50)
        dev.listen_stop()
        frames = dev.drain_until("tts_end")
    text_types = [p["type"] for k, p in frames if k == "text"]
    assert "reply" not in text_types
    assert not any(k == "bytes" for k, _ in frames)
    assert text_types[-1] == "tts_end"
