"""The ASR→LLM→TTS turn orchestrator.

Runs the pipeline server-side and streams each stage over the WS contract via the
router's :class:`TurnContext`: ``asr_partial*``/``asr`` → ``reply`` deltas →
``tts_audio`` frames → ``tts_end``.

Bounded + legible (PYR-022): each stage has a timeout budget (ASR ≤ 5 s, LLM
first token ≤ 8 s, TTS first audio ≤ 3 s); on breach or provider failure the turn
aborts to an enumerated ``error{code}`` and the device returns to idle. An
aborted turn is abandoned (the user message is rolled back so history stays
consistent) but the session stays connected for the next turn.
"""

from __future__ import annotations

import asyncio

from .errors import (
    ASR_TIMEOUT_S,
    LLM_FIRST_TOKEN_S,
    TTS_FIRST_AUDIO_S,
    provider_error_code,
)
from .history import ChatMessage, window
from .prompt import assemble_system
from .protocol import ErrorCode
from .providers.base import ASRProvider, LLMProvider, ProviderError, TTSProvider
from .router import TurnContext
from .sentence import PhraseSplitter


class Orchestrator:
    def __init__(
        self,
        asr: ASRProvider,
        llm: LLMProvider,
        tts: TTSProvider,
        *,
        persona: str | None = None,
        history_max_messages: int = 20,
        history_max_chars: int = 4000,
        asr_timeout_s: float = ASR_TIMEOUT_S,
        llm_first_token_s: float = LLM_FIRST_TOKEN_S,
        tts_first_audio_s: float = TTS_FIRST_AUDIO_S,
    ):
        self.asr = asr
        self.llm = llm
        self.tts = tts
        self.persona = persona
        self.history_max_messages = history_max_messages
        self.history_max_chars = history_max_chars
        self.asr_timeout_s = asr_timeout_s
        self.llm_first_token_s = llm_first_token_s
        self.tts_first_audio_s = tts_first_audio_s

    # --- entry points --------------------------------------------------------
    async def run_voice(self, ctx: TurnContext, audio: bytes) -> None:
        try:
            transcript = await asyncio.wait_for(self._do_asr(ctx, audio), self.asr_timeout_s)
        except (TimeoutError, ProviderError) as exc:
            await ctx.error(provider_error_code("asr", exc))
            return
        except Exception:
            await ctx.error(ErrorCode.INTERNAL)
            return
        if not transcript.strip():
            await ctx.tts_end()  # silence → clean end, not an error
            return
        await self._respond(ctx, transcript)

    async def run_text(self, ctx: TurnContext, text: str) -> None:
        if not text.strip():
            return
        await self._respond(ctx, text)

    # --- stages --------------------------------------------------------------
    async def _do_asr(self, ctx: TurnContext, audio: bytes) -> str:
        transcript = ""
        async for chunk in self.asr.transcribe(audio):
            if chunk.is_final:
                transcript = chunk.text
                await ctx.asr(chunk.text)
            else:
                await ctx.asr_partial(chunk.text)
        return transcript

    async def _respond(self, ctx: TurnContext, user_text: str) -> None:
        session = ctx.session
        session.history.append(ChatMessage(role="user", text=user_text))
        reply_text = await self._generate_and_speak(ctx)
        if reply_text is None:
            # Aborted (an error was already emitted) — roll back so history stays
            # consistent (alternating user/assistant) for the next turn.
            session.history.pop()
            return
        session.history.append(ChatMessage(role="assistant", text=reply_text))
        await ctx.tts_end()

    async def _generate_and_speak(self, ctx: TurnContext) -> str | None:
        """Stream the LLM reply and sentence-stream TTS. Returns the reply text,
        or ``None`` after emitting an ``error`` if any stage failed."""
        system = assemble_system(self.persona)
        messages = window(
            ctx.session.history,
            max_messages=self.history_max_messages,
            max_chars=self.history_max_chars,
        )
        agen = self.llm.stream(system, messages)
        reply_parts: list[str] = []
        splitter = PhraseSplitter()

        # First token within budget.
        try:
            first = await asyncio.wait_for(agen.__anext__(), self.llm_first_token_s)
            have_first = True
        except StopAsyncIteration:
            have_first, first = False, None
        except (TimeoutError, ProviderError) as exc:
            await ctx.error(provider_error_code("llm", exc))
            return None
        except Exception:
            await ctx.error(ErrorCode.INTERNAL)
            return None

        async def _process(delta: str) -> bool:
            if not delta:
                return True
            reply_parts.append(delta)
            await ctx.reply_delta(delta)
            for phrase in splitter.feed(delta):
                if not await self._speak(ctx, phrase):
                    return False
            return True

        try:
            if have_first and not await _process(first):
                return None
            async for delta in agen:
                if not await _process(delta):
                    return None
        except ProviderError as exc:
            await ctx.error(provider_error_code("llm", exc))
            return None
        except Exception:
            await ctx.error(ErrorCode.INTERNAL)
            return None

        await ctx.reply_delta("", done=True)
        tail = splitter.flush()
        if tail and not await self._speak(ctx, tail):
            return None
        return "".join(reply_parts)

    async def _speak(self, ctx: TurnContext, phrase: str) -> bool:
        """Synthesize one phrase, first audio within budget. Returns False after
        emitting a ``tts_failed``/mapped error if synthesis failed."""
        agen = self.tts.synthesize(phrase)
        try:
            first = await asyncio.wait_for(agen.__anext__(), self.tts_first_audio_s)
        except StopAsyncIteration:
            return True  # empty audio for this phrase
        except (TimeoutError, ProviderError) as exc:
            await ctx.error(provider_error_code("tts", exc))
            return False
        except Exception:
            await ctx.error(ErrorCode.INTERNAL)
            return False
        await ctx.tts_audio(first)
        try:
            async for pcm in agen:
                await ctx.tts_audio(pcm)
        except (TimeoutError, ProviderError) as exc:
            await ctx.error(provider_error_code("tts", exc))
            return False
        except Exception:
            await ctx.error(ErrorCode.INTERNAL)
            return False
        return True


def build_default_orchestrator(settings) -> Orchestrator | None:
    """Build an orchestrator from real providers + settings, for runtime use.

    Returns ``None`` if no LLM key is configured (so a keyless dev server still
    boots and the contract paths work; turns simply no-op until keys are set).
    """
    if not settings.llm_api_key:
        return None
    from .providers.real import AnthropicLLM, DeepgramASR, ElevenLabsTTS

    return Orchestrator(
        asr=DeepgramASR(settings.asr_api_key),
        llm=AnthropicLLM(settings.llm_api_key, model=settings.llm_model),
        tts=ElevenLabsTTS(settings.tts_api_key, settings.tts_voice_id),
    )
