"""PYR-018 smoke test: the server boots, accepts a WS connection, answers
``ping`` with ``pong``, and creates / tears down a ``Session`` per connection.

This is the first green pytest run CI enforces; it stands up the /tests tree
(``unit``/``contract``/``integration``/``fakes``) the rest of v2 fills in.
Uses FastAPI's TestClient (no real network / uvicorn process needed).
"""

from __future__ import annotations

from fastapi.testclient import TestClient

from pyramid_server.main import create_app


def test_healthz_ok():
    app = create_app()
    with TestClient(app) as client:
        resp = client.get("/healthz")
    assert resp.status_code == 200
    assert resp.json()["status"] == "ok"


def test_ws_connect_and_ping_pong():
    app = create_app()
    with TestClient(app) as client:
        with client.websocket_connect("/ws") as ws:
            ws.send_json({"type": "ping"})
            reply = ws.receive_json()
    assert reply == {"type": "pong"}


def test_session_created_and_torn_down():
    app = create_app()
    assert app.state.sessions == {}
    with TestClient(app) as client:
        with client.websocket_connect("/ws") as ws:
            ws.send_json({"type": "ping"})
            ws.receive_json()
            # Exactly one live session while connected.
            assert len(app.state.sessions) == 1
    # Torn down on disconnect.
    assert app.state.sessions == {}
