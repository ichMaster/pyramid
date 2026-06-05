"""The ASR→LLM→TTS turn orchestrator.

Runs the pipeline server-side and streams each stage over the WS contract via the
router's :class:`TurnContext`: ``asr_partial*``/``asr`` → ``reply`` deltas →
``tts_audio`` frames → ``tts_end``. Per-stage timeouts + error mapping are added
in PYR-022; sentence-streaming TTS in PYR-021. Provider-agnostic — real adapters
in production, mocks in tests.
"""

from __future__ import annotations

from .history import ChatMessage, window
from .prompt import assemble_system
from .providers.base import ASRProvider, LLMProvider, TTSProvider
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
    ):
        self.asr = asr
        self.llm = llm
        self.tts = tts
        self.persona = persona
        self.history_max_messages = history_max_messages
        self.history_max_chars = history_max_chars

    async def run_voice(self, ctx: TurnContext, audio: bytes) -> None:
        """ASR the buffered clip, then respond."""
        transcript = ""
        async for chunk in self.asr.transcribe(audio):
            if chunk.is_final:
                transcript = chunk.text
                await ctx.asr(chunk.text)
            else:
                await ctx.asr_partial(chunk.text)
        if not transcript.strip():
            # Nothing recognized — end the turn cleanly (device returns to idle).
            await ctx.tts_end()
            return
        await self._respond(ctx, transcript)

    async def run_text(self, ctx: TurnContext, text: str) -> None:
        """The text / serial-bridge path: skip ASR, respond directly."""
        if not text.strip():
            return
        await self._respond(ctx, text)

    async def _respond(self, ctx: TurnContext, user_text: str) -> None:
        session = ctx.session
        session.history.append(ChatMessage(role="user", text=user_text))
        system = assemble_system(self.persona)
        messages = window(
            session.history,
            max_messages=self.history_max_messages,
            max_chars=self.history_max_chars,
        )

        # Sentence-streaming TTS (rec #3): synthesize each phrase as it completes
        # while the LLM is still generating, so the first audio leaves early.
        reply_parts: list[str] = []
        splitter = PhraseSplitter()
        async for delta in self.llm.stream(system, messages):
            if not delta:
                continue
            reply_parts.append(delta)
            await ctx.reply_delta(delta)
            for phrase in splitter.feed(delta):
                await self._speak(ctx, phrase)
        await ctx.reply_delta("", done=True)

        tail = splitter.flush()  # the final phrase / whole short reply
        if tail:
            await self._speak(ctx, tail)

        reply_text = "".join(reply_parts)
        session.history.append(ChatMessage(role="assistant", text=reply_text))
        await ctx.tts_end()

    async def _speak(self, ctx: TurnContext, phrase: str) -> None:
        async for pcm in self.tts.synthesize(phrase):
            await ctx.tts_audio(pcm)


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
