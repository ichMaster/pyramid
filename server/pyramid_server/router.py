"""Per-connection message router and turn-state machine.

Dispatches inbound device→server frames (text + binary audio) and emits
server→device frames through a :class:`Transport`. The actual ASR→LLM→TTS turn
is delegated to a pluggable ``orchestrator`` (injected in PYR-020); without one,
``listen_stop`` / ``text_in`` simply transition state and do nothing — which is
all PYR-019's contract tests need.

Idempotency (ARCHITECTURE §Error handling): ``listen_start`` / ``listen_stop``
received outside the expected state are ignored.
"""

from __future__ import annotations

import enum
from typing import Protocol

from . import protocol
from .protocol import In, decode_text
from .session import Session


class Transport(Protocol):
    """Minimal duplex transport the router writes to (a WS, or a test fake)."""

    async def send_text(self, text: str) -> None: ...
    async def send_bytes(self, data: bytes) -> None: ...


class ConnState(enum.Enum):
    IDLE = "idle"
    LISTENING = "listening"
    PROCESSING = "processing"


class TurnContext:
    """Emit helpers handed to the orchestrator so it never touches the wire codec."""

    def __init__(self, session: Session, transport: Transport):
        self.session = session
        self.tx = transport

    async def _emit(self, msg: dict) -> None:
        await self.tx.send_text(protocol.encode(msg))

    async def asr_partial(self, text: str) -> None:
        await self._emit(protocol.asr_partial(text))

    async def asr(self, text: str) -> None:
        await self._emit(protocol.asr(text))

    async def reply_delta(self, text: str, *, done: bool = False) -> None:
        await self._emit(protocol.reply(text, delta=True, done=done))

    async def reply(self, text: str) -> None:
        await self._emit(protocol.reply(text))

    async def text_out(self, text: str) -> None:
        await self._emit(protocol.text_out(text))

    async def tts_audio(self, pcm: bytes) -> None:
        await self.tx.send_bytes(pcm)

    async def tts_end(self) -> None:
        await self._emit(protocol.tts_end())

    async def error(self, code: str, msg: str = "") -> None:
        await self._emit(protocol.error(code, msg))


class Orchestrator(Protocol):
    """Runs a turn, emitting via the :class:`TurnContext` (implemented in PYR-020)."""

    async def run_voice(self, ctx: TurnContext, audio: bytes) -> None: ...
    async def run_text(self, ctx: TurnContext, text: str) -> None: ...


class Router:
    def __init__(
        self,
        session: Session,
        transport: Transport,
        *,
        server_major: int,
        orchestrator: Orchestrator | None = None,
    ):
        self.session = session
        self.tx = transport
        self.server_major = server_major
        self.orch = orchestrator
        self.ctx = TurnContext(session, transport)
        self.state = ConnState.IDLE
        self.hello_done = False
        self._audio = bytearray()

    # --- inbound entry points; return False to close the connection ----------
    async def handle_text(self, raw: str) -> bool:
        try:
            msg = decode_text(raw)
        except protocol.ProtocolError:
            # Lenient: drop malformed frames rather than killing the session.
            return True

        if msg.type == In.PING:
            await self.ctx._emit(protocol.pong())
            return True
        if msg.type == In.HELLO:
            return await self._on_hello(msg)
        if msg.type == In.LISTEN_START:
            if self.state == ConnState.IDLE:
                self.state = ConnState.LISTENING
                self._audio.clear()
            return True
        if msg.type == In.LISTEN_STOP:
            if self.state == ConnState.LISTENING:
                await self._run_voice()
            return True
        if msg.type == In.TEXT_IN:
            await self._run_text(str(msg.get("text", "")))
            return True
        return True

    async def handle_binary(self, data: bytes) -> bool:
        if self.state == ConnState.LISTENING:
            self._audio.extend(data)
        # Stray audio outside a listen window is ignored.
        return True

    # --- handlers ------------------------------------------------------------
    async def _on_hello(self, msg) -> bool:
        ok, ver = protocol.negotiate(msg.get("proto_ver"), self.server_major)
        if not ok:
            await self.ctx.error(
                protocol.ErrorCode.PROTO_UNSUPPORTED,
                f"server speaks proto major {self.server_major}",
            )
            return False  # close the socket
        self.session.proto_ver = ver
        self.session.device_token = msg.get("device_token")
        self.hello_done = True
        # device_token validation (allowlist) is enforced from v2.6.
        return True

    async def _run_voice(self) -> None:
        audio = bytes(self._audio)
        self._audio.clear()
        self.state = ConnState.PROCESSING
        try:
            if self.orch is not None:
                await self.orch.run_voice(self.ctx, audio)
        finally:
            self.state = ConnState.IDLE

    async def _run_text(self, text: str) -> None:
        self.state = ConnState.PROCESSING
        try:
            if self.orch is not None:
                await self.orch.run_text(self.ctx, text)
        finally:
            self.state = ConnState.IDLE
