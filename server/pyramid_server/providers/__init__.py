"""ASR / LLM / TTS provider adapters.

One interface per stage (``base``), with real implementations (Deepgram /
Anthropic / ElevenLabs) and deterministic ``mock`` implementations so the turn
pipeline runs offline in CI — external AI is never called in tests.
"""
