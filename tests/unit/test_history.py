"""Unit: rolling-history windowing, and the guarantee that audio is never
persisted — only user/assistant text. (PYR-020; ARCHITECTURE §Sessions and history.)"""

from __future__ import annotations

from pyramid_server.history import ChatMessage, window
from pyramid_server.orchestrator import Orchestrator
from pyramid_server.providers.mock import MockASR, MockLLM, MockTTS
from pyramid_server.router import TurnContext
from pyramid_server.session import Session
from tests.fakes.fake_device import RecordingTransport


def _msgs(n):
    return [ChatMessage(role="user" if i % 2 == 0 else "assistant", text=f"m{i}") for i in range(n)]


def test_window_keeps_last_n_messages():
    kept = window(_msgs(30), max_messages=10, max_chars=100000)
    assert len(kept) == 10
    assert [m.text for m in kept] == [f"m{i}" for i in range(20, 30)]


def test_window_trims_to_char_budget():
    msgs = [ChatMessage("user", "x" * 100) for _ in range(10)]
    kept = window(msgs, max_messages=100, max_chars=250)
    # 250 / 100 → keeps 2 (front trimmed until under budget).
    assert len(kept) == 2


def test_window_keeps_at_least_one_even_if_oversized():
    kept = window([ChatMessage("user", "z" * 9999)], max_messages=10, max_chars=10)
    assert len(kept) == 1


async def test_turn_persists_text_only_no_audio():
    orch = Orchestrator(MockASR(transcript="привіт"), MockLLM(reply="Вітаю!"), MockTTS())
    session = Session()
    ctx = TurnContext(session, RecordingTransport())

    await orch.run_voice(ctx, b"\x01\x02\x03\x04raw-audio-bytes")

    roles = [m.role for m in session.history]
    texts = [m.text for m in session.history]
    assert roles == ["user", "assistant"]
    assert texts == ["привіт", "Вітаю!"]
    # The raw audio is nowhere in the persisted history.
    assert all(isinstance(m.text, str) for m in session.history)
    assert not any("raw-audio-bytes" in m.text for m in session.history)


async def test_second_turn_appends_pair():
    orch = Orchestrator(MockASR(transcript="ще раз"), MockLLM(reply="ок"), MockTTS())
    session = Session()
    ctx = TurnContext(session, RecordingTransport())
    await orch.run_text(ctx, "перше")
    await orch.run_text(ctx, "друге")
    assert [m.role for m in session.history] == ["user", "assistant", "user", "assistant"]
    assert [m.text for m in session.history][::2] == ["перше", "друге"]
