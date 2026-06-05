"""Integration: the text / serial-bridge path — ``text_in → reply (deltas) →
tts_audio → tts_end`` over the fake device + mock providers. (PYR-020.)"""

from __future__ import annotations

from pyramid_server.main import create_app
from pyramid_server.orchestrator import Orchestrator
from pyramid_server.providers.mock import MockASR, MockLLM, MockTTS
from tests.fakes.fake_device import connect


def build_app(reply="Привіт. Чим можу допомогти?"):
    orch = Orchestrator(MockASR(), MockLLM(reply=reply), MockTTS())
    return create_app(orchestrator=orch)


def test_text_in_yields_reply_then_audio_then_end():
    reply = "Привіт. Чим можу допомогти?"
    app = build_app(reply)
    with connect(app) as dev:
        dev.hello()
        dev.text_in("привіт")
        frames = dev.drain_until("tts_end")

    kinds = [k for k, _ in frames]
    texts = [p for k, p in frames if k == "text"]
    binaries = [p for k, p in frames if k == "bytes"]

    # Reply streamed as deltas; concatenated they reconstruct the full reply.
    deltas = [t["text"] for t in texts if t["type"] == "reply"]
    assert "".join(deltas) == reply
    # At least one streamed delta, a done marker, audio, and a terminal tts_end.
    assert any(t["type"] == "reply" and t.get("done") for t in texts)
    assert len(binaries) >= 1
    assert texts[-1]["type"] == "tts_end"
    # No ASR on the text path.
    assert not any(t["type"] in ("asr", "asr_partial") for t in texts)
    assert "bytes" in kinds


def test_no_orchestrator_text_in_is_noop():
    # Router without an orchestrator: text_in does not crash and emits nothing.
    app = create_app(orchestrator=None)
    with connect(app) as dev:
        dev.hello()
        dev.text_in("привіт")
        dev.ping()
        assert dev.recv_json() == {"type": "pong"}
