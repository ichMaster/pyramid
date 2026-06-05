"""FastAPI application: the WSS endpoint and the session lifecycle.

Accepts a single duplex WS channel and drives it through :class:`Router` — the
device↔server codec + turn-state machine (PYR-019). The ASR→LLM→TTS orchestrator
(PYR-020) is injected via ``create_app(orchestrator=...)`` and plugged into each
router; without one, the router still honors hello/ping/idempotency.
"""

from __future__ import annotations

from fastapi import FastAPI, WebSocket, WebSocketDisconnect

from .config import Settings, load_settings
from .logging_conf import bind, configure_logging, get_logger
from .router import Orchestrator, Router
from .session import Session


class _WSTransport:
    """Adapt a FastAPI ``WebSocket`` to the router's :class:`Transport`."""

    def __init__(self, ws: WebSocket):
        self._ws = ws

    async def send_text(self, text: str) -> None:
        await self._ws.send_text(text)

    async def send_bytes(self, data: bytes) -> None:
        await self._ws.send_bytes(data)


def create_app(
    settings: Settings | None = None,
    orchestrator: Orchestrator | None = None,
) -> FastAPI:
    """Build the FastAPI app. A factory so tests get a fresh instance."""
    settings = settings or load_settings()
    configure_logging()
    log = get_logger("pyramid.server")

    app = FastAPI(title="Pyramid server", version="2.1.0-dev")
    app.state.settings = settings
    app.state.orchestrator = orchestrator
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
        router = Router(
            session,
            _WSTransport(ws),
            server_major=settings.proto_major,
            orchestrator=app.state.orchestrator,
        )
        try:
            while True:
                frame = await ws.receive()
                if frame["type"] == "websocket.disconnect":
                    break
                session.touch()
                keep = True
                text = frame.get("text")
                data = frame.get("bytes")
                if text is not None:
                    keep = await router.handle_text(text)
                elif data is not None:
                    keep = await router.handle_binary(data)
                if not keep:
                    await ws.close()
                    break
        except WebSocketDisconnect:
            pass
        finally:
            app.state.sessions.pop(session.id, None)
            slog.info("session.close")

    return app


# Module-level app for ``uvicorn pyramid_server.main:app``.
app = create_app()
