"""Contract test: idempotency + framing on the router state machine.

``listen_start``/``listen_stop`` outside the expected state are ignored; audio
is buffered only during a listen window; ``restart`` exists as a server→device
message (the device's restart→boot is firmware behavior). Driven directly
against the ``Router`` via an in-memory transport. (ARCHITECTURE §Error handling.)
"""

from __future__ import annotations

import json

from pyramid_server import protocol as p
from pyramid_server.router import ConnState, Router
from pyramid_server.session import Session
from tests.fakes.fake_device import RecordingTransport


def make_router():
    tx = RecordingTransport()
    return Router(Session(), tx, server_major=1), tx


async def test_ping_pongs():
    r, tx = make_router()
    await r.handle_text(json.dumps({"type": "ping"}))
    assert tx.types() == [p.Out.PONG]


async def test_listen_start_idempotent():
    r, _ = make_router()
    await r.handle_text(json.dumps({"type": "listen_start"}))
    assert r.state == ConnState.LISTENING
    # A second listen_start while already listening is ignored (no error, no reset).
    await r.handle_text(json.dumps({"type": "listen_start"}))
    assert r.state == ConnState.LISTENING


async def test_listen_stop_when_idle_is_ignored():
    r, tx = make_router()
    assert r.state == ConnState.IDLE
    await r.handle_text(json.dumps({"type": "listen_stop"}))
    assert r.state == ConnState.IDLE
    assert tx.texts == []  # no turn, no output


async def test_audio_buffered_only_while_listening():
    r, _ = make_router()
    # Stray audio outside a listen window is dropped.
    await r.handle_binary(b"\x01\x02")
    assert len(r._audio) == 0
    # During a listen window it accumulates.
    await r.handle_text(json.dumps({"type": "listen_start"}))
    await r.handle_binary(b"\x01\x02\x03\x04")
    assert len(r._audio) == 4
    # listen_stop ends the window; with no orchestrator the turn is a no-op and
    # the buffer is cleared, state returns to IDLE.
    await r.handle_text(json.dumps({"type": "listen_stop"}))
    assert r.state == ConnState.IDLE
    assert len(r._audio) == 0


async def test_malformed_frame_is_dropped_not_fatal():
    r, tx = make_router()
    keep = await r.handle_text("definitely not json")
    assert keep is True
    assert tx.texts == []


def test_restart_message_exists():
    # restart→boot is firmware behavior; the contract just guarantees the message.
    assert p.restart() == {"type": p.Out.RESTART}
