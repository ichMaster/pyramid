"""Short, per-session conversation history.

Held in RAM on the live connection and trimmed to a rolling window before each
LLM call (ARCHITECTURE §Sessions and history). Audio frames are never stored —
only the user/assistant text. Cross-session persistence (`memory_type=recent`)
is v2.4; semantic long-term memory is v3.
"""

from __future__ import annotations

import time
from dataclasses import dataclass, field


@dataclass
class ChatMessage:
    role: str  # "user" | "assistant"
    text: str
    ts: float = field(default_factory=time.time)


def window(
    messages: list[ChatMessage],
    *,
    max_messages: int = 20,
    max_chars: int = 4000,
) -> list[ChatMessage]:
    """Return the most recent slice within both a message count and a char budget.

    Keeps at least the last message even if it alone exceeds ``max_chars``.
    """
    kept = list(messages[-max_messages:])
    while len(kept) > 1 and sum(len(m.text) for m in kept) > max_chars:
        kept = kept[1:]
    return kept
