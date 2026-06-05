"""Pyramid server — FastAPI + websockets orchestrator (v2+).

The device is a thin WSS client; all intelligence (ASR → LLM → TTS, roles,
sessions, memory) lives here. See specification/ARCHITECTURE.md.
"""

__version__ = "2.1.0-dev"
