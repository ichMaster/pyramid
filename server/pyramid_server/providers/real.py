"""Real ASR/LLM/TTS adapters — Deepgram, Anthropic, ElevenLabs (the v1 providers,
now server-side with keys from ``.env``).

Not exercised in CI (mocks stand in); used at runtime / on the hardware path.
Each call builds its own ``httpx.AsyncClient`` so nothing networks at import.
Per-stage timeout budgets are enforced by the orchestrator (PYR-022).
"""

from __future__ import annotations

import json
from collections.abc import AsyncIterator

import httpx

from ..history import ChatMessage
from .base import ASRChunk, ProviderError


class DeepgramASR:
    """Batch transcription of a PCM16 16 kHz clip (one final chunk, no interims)."""

    URL = "https://api.deepgram.com/v1/listen"

    def __init__(self, api_key: str, *, language: str = "uk", model: str = "nova-2"):
        self.api_key = api_key
        self.language = language
        self.model = model

    async def transcribe(self, audio: bytes) -> AsyncIterator[ASRChunk]:
        params = {
            "encoding": "linear16",
            "sample_rate": "16000",
            "channels": "1",
            "model": self.model,
            "language": self.language,
            "punctuate": "true",
        }
        headers = {"Authorization": f"Token {self.api_key}", "Content-Type": "audio/raw"}
        try:
            async with httpx.AsyncClient(timeout=30.0) as client:
                resp = await client.post(self.URL, params=params, headers=headers, content=audio)
                resp.raise_for_status()
                body = resp.json()
            alt = body["results"]["channels"][0]["alternatives"][0]
            yield ASRChunk(alt.get("transcript", ""), is_final=True)
        except (httpx.HTTPError, KeyError, IndexError, ValueError) as exc:
            raise ProviderError(f"deepgram: {exc}") from exc


class AnthropicLLM:
    """Streaming reply via the Anthropic Messages API (SSE)."""

    URL = "https://api.anthropic.com/v1/messages"

    def __init__(self, api_key: str, *, model: str = "claude-3-5-haiku-latest", max_tokens: int = 512):
        self.api_key = api_key
        self.model = model
        self.max_tokens = max_tokens

    async def stream(self, system: str, messages: list[ChatMessage]) -> AsyncIterator[str]:
        payload = {
            "model": self.model,
            "max_tokens": self.max_tokens,
            "system": system,
            "stream": True,
            "messages": [{"role": m.role, "content": m.text} for m in messages],
        }
        headers = {
            "x-api-key": self.api_key,
            "anthropic-version": "2023-06-01",
            "content-type": "application/json",
        }
        try:
            async with httpx.AsyncClient(timeout=60.0) as client:
                async with client.stream("POST", self.URL, headers=headers, json=payload) as resp:
                    resp.raise_for_status()
                    async for line in resp.aiter_lines():
                        if not line.startswith("data:"):
                            continue
                        data = line[5:].strip()
                        if not data or data == "[DONE]":
                            continue
                        evt = json.loads(data)
                        if evt.get("type") == "content_block_delta":
                            delta = evt.get("delta", {})
                            if delta.get("type") == "text_delta":
                                yield delta.get("text", "")
        except (httpx.HTTPError, ValueError) as exc:
            raise ProviderError(f"anthropic: {exc}") from exc


class ElevenLabsTTS:
    """Streaming TTS as PCM16 16 kHz (``output_format=pcm_16000``)."""

    BASE = "https://api.elevenlabs.io/v1/text-to-speech"

    def __init__(self, api_key: str, voice_id: str, *, model_id: str = "eleven_turbo_v2_5"):
        self.api_key = api_key
        self.voice_id = voice_id
        self.model_id = model_id

    async def synthesize(self, text: str) -> AsyncIterator[bytes]:
        url = f"{self.BASE}/{self.voice_id}/stream"
        params = {"output_format": "pcm_16000"}
        headers = {"xi-api-key": self.api_key, "content-type": "application/json"}
        payload = {"text": text, "model_id": self.model_id}
        try:
            async with httpx.AsyncClient(timeout=60.0) as client:
                async with client.stream(
                    "POST", url, params=params, headers=headers, json=payload
                ) as resp:
                    resp.raise_for_status()
                    async for chunk in resp.aiter_bytes():
                        if chunk:
                            yield chunk
        except httpx.HTTPError as exc:
            raise ProviderError(f"elevenlabs: {exc}") from exc
