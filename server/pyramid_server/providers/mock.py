"""Deterministic mock ASR/LLM/TTS — keep the pipeline offline in CI.

No network, no randomness: given the same input they always produce the same
output, so integration tests assert exact stage order and content. Optional
delays/failures let tests exercise streaming overlap and the error paths.
"""

from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator

from .base import ASRChunk, ProviderError


class MockASR:
    """Yields the configured interim partials then a final transcript."""

    def __init__(self, transcript: str = "привіт", partials: list[str] | None = None):
        self.transcript = transcript
        self.partials = partials or []

    async def transcribe(self, audio: bytes) -> AsyncIterator[ASRChunk]:
        for p in self.partials:
            yield ASRChunk(p, is_final=False)
        yield ASRChunk(self.transcript, is_final=True)


class MockLLM:
    """Streams a canned reply word by word (so streaming is observable)."""

    def __init__(self, reply: str = "Привіт. Чим можу допомогти?", delay_s: float = 0.0):
        self.reply = reply
        self.delay_s = delay_s
        self.last_system: str | None = None
        self.last_messages = None

    async def stream(self, system: str, messages) -> AsyncIterator[str]:
        self.last_system = system
        self.last_messages = list(messages)
        words = self.reply.split(" ")
        for i, w in enumerate(words):
            if self.delay_s:
                await asyncio.sleep(self.delay_s)
            yield w if i == 0 else " " + w


class MockTTS:
    """Yields deterministic PCM16 chunks — one per call-segment, two per text."""

    def __init__(self, delay_s: float = 0.0):
        self.delay_s = delay_s
        self.synth_calls: list[str] = []

    async def synthesize(self, text: str) -> AsyncIterator[bytes]:
        self.synth_calls.append(text)
        # Two deterministic, non-empty PCM16 chunks derived from the text length.
        n = max(2, (len(text) % 8) * 2)
        chunk = (b"\x10\x00" * n)
        if self.delay_s:
            await asyncio.sleep(self.delay_s)
        yield chunk[: len(chunk) // 2] or b"\x00\x00"
        yield chunk[len(chunk) // 2:] or b"\x00\x00"


class StallingMock:
    """An async generator that sleeps forever after an optional first item —
    used to exercise per-stage timeouts (PYR-022)."""

    def __init__(self, stage: str = "llm"):
        self.stage = stage

    async def transcribe(self, audio: bytes) -> AsyncIterator[ASRChunk]:
        await asyncio.sleep(3600)
        yield ASRChunk("", True)  # pragma: no cover

    async def stream(self, system: str, messages) -> AsyncIterator[str]:
        await asyncio.sleep(3600)
        yield ""  # pragma: no cover

    async def synthesize(self, text: str) -> AsyncIterator[bytes]:
        await asyncio.sleep(3600)
        yield b""  # pragma: no cover


class FailingMock:
    """Raises ``ProviderError`` immediately — exercises error mapping (PYR-022)."""

    async def transcribe(self, audio: bytes) -> AsyncIterator[ASRChunk]:
        raise ProviderError("asr boom")
        yield  # pragma: no cover  (makes this an async generator)

    async def stream(self, system: str, messages) -> AsyncIterator[str]:
        raise ProviderError("llm boom")
        yield  # pragma: no cover

    async def synthesize(self, text: str) -> AsyncIterator[bytes]:
        raise ProviderError("tts boom")
        yield  # pragma: no cover
