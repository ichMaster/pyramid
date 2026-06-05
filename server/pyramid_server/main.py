"""FastAPI application: the WSS endpoint and the session lifecycle.

v2.1 (PYR-018) skeleton: accept a single duplex WS channel, answer ``ping`` with
``pong``, and create/tear down a :class:`Session` per connection. The full
device↔server message codec/router and the ASR→LLM→TTS orchestrator arrive in
PYR-019 / PYR-020 and plug into this same endpoint.
"""

from __future__ import annotations

import json

from fastapi import FastAPI, WebSocket, WebSocketDisconnect

from .config import Settings, load_settings
from .logging_conf import bind, configure_logging, get_logger
from .session import Session


def create_app(settings: Settings | None = None) -> FastAPI:
    """Build the FastAPI app. A factory so tests get a fresh instance."""
    settings = settings or load_settings()
    configure_logging()
    log = get_logger("pyramid.server")

    app = FastAPI(title="Pyramid server", version="2.1.0-dev")
    app.state.settings = settings
    # Live sessions by id — a registry the multi-session hub (v4.3) will build on.
    app.state.sessions = {}

    @app.get("/healthz")
    async def healthz() -> dict:
        return {"status": "ok", "proto": f"{settings.proto_major}.{settings.proto_minor}"}

    @app.websocket("/ws")
    async def ws_endpoint(ws: WebSocket) -> None:
        await ws.accept()
        session = Session()
        app.state.sessions[session.id] = session
        slog = bind(log, session_id=session.id)
        slog.info("session.open")
        try:
            while True:
                frame = await ws.receive()
                if frame["type"] == "websocket.disconnect":
                    break
                session.touch()
                text = frame.get("text")
                if text is not None:
                    await _handle_text(ws, session, slog, text)
                # Binary frames (audio) are handled from PYR-019/PYR-020 onward.
        except WebSocketDisconnect:
            pass
        finally:
            app.state.sessions.pop(session.id, None)
            slog.info("session.close")

    return app


async def _handle_text(ws: WebSocket, session: Session, slog, text: str) -> None:
    """Minimal text handling for the skeleton: ``ping`` → ``pong``.

    Replaced by the codec/router in PYR-019; kept tiny here on purpose.
    """
    try:
        msg = json.loads(text)
    except (ValueError, TypeError):
        return
    if msg.get("type") == "ping":
        await ws.send_text(json.dumps({"type": "pong"}))


# Module-level app for ``uvicorn pyramid_server.main:app``.
app = create_app()
