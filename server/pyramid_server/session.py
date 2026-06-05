"""Per-connection session state.

A :class:`Session` is the live connection plus its current turn/audio — created
on connect, torn down on disconnect / ``restart`` / idle timeout (ARCHITECTURE
§Sessions and history). Durable, per-account state (Role, memory, …) is *not*
held here; that arrives in later phases. Serving many sessions concurrently with
shared resources is v4.3 — v2.1 terminates a single channel.
"""

from __future__ import annotations

import time
import uuid
from dataclasses import dataclass, field


@dataclass
class Session:
    """The live connection and its turn counter."""

    id: str = field(default_factory=lambda: uuid.uuid4().hex[:12])
    device_token: str | None = None
    proto_ver: tuple[int, int] | None = None
    started_at: float = field(default_factory=time.time)
    last_seen: float = field(default_factory=time.time)
    _turn_seq: int = 0

    def touch(self) -> None:
        """Mark activity (resets the idle clock)."""
        self.last_seen = time.time()

    def next_turn_id(self) -> str:
        """Mint a turn id unique within this session."""
        self._turn_seq += 1
        return f"{self.id}-t{self._turn_seq}"

    def idle_for(self) -> float:
        """Seconds since the last activity."""
        return time.time() - self.last_seen
