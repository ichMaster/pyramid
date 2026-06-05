"""A fake device that speaks the WS protocol — the contract + integration
harness (ARCHITECTURE §Testing; the firmware serial bridge mirrors it).

Two flavors:
- :class:`FakeDevice` drives a real WS connection via FastAPI's ``TestClient``.
- :class:`RecordingTransport` is an in-memory ``Transport`` for driving a
  ``Router`` directly (no WS), recording everything the server sends.
"""

from __future__ import annotations

import json
from contextlib import contextmanager

from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect


class FakeDevice:
    """Wraps a ``TestClient`` websocket session with protocol helpers."""

    def __init__(self, ws):
        self._ws = ws

    # device → server
    def hello(self, proto_ver="1.0", device_token="dev-test", audio_fmt="pcm16"):
        self._ws.send_json(
            {
                "type": "hello",
                "proto_ver": proto_ver,
                "device_token": device_token,
                "audio_fmt": audio_fmt,
            }
        )

    def ping(self):
        self._ws.send_json({"type": "ping"})

    def listen_start(self):
        self._ws.send_json({"type": "listen_start"})

    def listen_stop(self):
        self._ws.send_json({"type": "listen_stop"})

    def text_in(self, text: str):
        self._ws.send_json({"type": "text_in", "text": text})

    def send_audio(self, pcm: bytes):
        self._ws.send_bytes(pcm)

    def send_raw(self, text: str):
        self._ws.send_text(text)

    # server → device
    def recv_json(self) -> dict:
        return self._ws.receive_json()

    def recv_bytes(self) -> bytes:
        return self._ws.receive_bytes()

    def expect_closed(self) -> bool:
        """Return True if the server has closed the socket."""
        try:
            self._ws.receive_json()
        except WebSocketDisconnect:
            return True
        return False


@contextmanager
def connect(app):
    """Open a fake-device WS connection to ``app``; yields a :class:`FakeDevice`.

    Tolerant of a server-initiated close (e.g. after ``error{proto_unsupported}``)
    so tests that expect the socket to close don't error on context exit.
    """
    client = TestClient(app)
    with client:
        ws_cm = client.websocket_connect("/ws")
        ws = ws_cm.__enter__()
        try:
            yield FakeDevice(ws)
        finally:
            try:
                ws_cm.__exit__(None, None, None)
            except Exception:
                pass


class RecordingTransport:
    """In-memory ``Transport``: records text/binary frames the server sends."""

    def __init__(self):
        self.texts: list[str] = []
        self.binaries: list[bytes] = []

    async def send_text(self, text: str) -> None:
        self.texts.append(text)

    async def send_bytes(self, data: bytes) -> None:
        self.binaries.append(data)

    # convenience views
    @property
    def messages(self) -> list[dict]:
        return [json.loads(t) for t in self.texts]

    def types(self) -> list[str]:
        return [m.get("type") for m in self.messages]
