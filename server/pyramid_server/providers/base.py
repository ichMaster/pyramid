"""Provider interfaces — one per pipeline stage.

Each stage streams: ASR yields interim + final chunks, the LLM yields token
deltas, TTS yields PCM16 audio chunks. The orchestrator wires them to the WS
contract (asr_partial/asr, reply deltas, tts_audio/tts_end).
"""

from __future__ import annotations

from collections.abc import AsyncIterator
from dataclasses import dataclass
from typing import Protocol, runtime_checkable

from ..history import ChatMessage


@dataclass(frozen=True)
class ASRChunk:
    text: str
    is_final: bool


@runtime_checkable
class ASRProvider(Protocol):
    def transcribe(self, audio: bytes) -> AsyncIterator[ASRChunk]:
        """Yield zero or more interim chunks (``is_final=False``) then exactly one
        final chunk (``is_final=True``). Batch providers yield just the final."""
        ...


@runtime_checkable
class LLMProvider(Protocol):
    def stream(self, system: str, messages: list[ChatMessage]) -> AsyncIterator[str]:
        """Yield reply token deltas as they are generated."""
        ...


@runtime_checkable
class TTSProvider(Protocol):
    def synthesize(self, text: str) -> AsyncIterator[bytes]:
        """Yield PCM16 16 kHz mono audio chunks for ``text``."""
        ...


class ProviderError(RuntimeError):
    """A provider call failed (network / API / decode)."""
