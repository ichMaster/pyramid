"""Server configuration and secrets.

Loaded from environment / a local ``.env`` file (never committed — see
``.env.example``). In v0–v1 the keys lived in firmware config; from v2 they live
here (ARCHITECTURE §Security, auth, and secrets).
"""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path

# Protocol version the server speaks. Negotiated in `hello`; the server rejects
# an unknown *major* (ARCHITECTURE §WS device↔server / §Cross-cutting).
PROTO_MAJOR = 1
PROTO_MINOR = 0


def _load_dotenv(path: Path) -> None:
    """Minimal ``.env`` loader (no dependency on python-dotenv).

    ``KEY=VALUE`` lines; ``#`` comments and blank lines ignored; existing
    environment variables win (so real env / CI secrets override the file).
    """
    if not path.is_file():
        return
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        key = key.strip()
        value = value.strip().strip('"').strip("'")
        os.environ.setdefault(key, value)


@dataclass(frozen=True)
class Settings:
    """Resolved server settings."""

    host: str = "127.0.0.1"
    port: int = 8000
    env: str = "dev"  # dev | prod
    proto_major: int = PROTO_MAJOR
    proto_minor: int = PROTO_MINOR

    # Idle timeout for an inactive session, seconds (0 disables).
    session_idle_timeout_s: float = 600.0

    # Provider keys — present for v2.1's real adapters (PYR-020). Tests use mocks
    # and never read these, so they default empty and CI needs no secrets.
    llm_api_key: str = ""
    llm_model: str = "claude-3-5-haiku-latest"
    asr_api_key: str = ""
    tts_api_key: str = ""
    tts_voice_id: str = ""

    extra: dict = field(default_factory=dict)


def load_settings(env_file: str | os.PathLike | None = None) -> Settings:
    """Load :class:`Settings`, populating ``os.environ`` from ``.env`` first."""
    root = Path(__file__).resolve().parents[1]  # the /server directory
    _load_dotenv(Path(env_file) if env_file else root / ".env")

    def _f(name: str, default: float) -> float:
        try:
            return float(os.environ.get(name, default))
        except (TypeError, ValueError):
            return default

    return Settings(
        host=os.environ.get("PYRAMID_HOST", "127.0.0.1"),
        port=int(os.environ.get("PYRAMID_PORT", "8000")),
        env=os.environ.get("PYRAMID_ENV", "dev"),
        session_idle_timeout_s=_f("PYRAMID_SESSION_IDLE_TIMEOUT_S", 600.0),
        llm_api_key=os.environ.get("LLM_API_KEY", ""),
        llm_model=os.environ.get("LLM_MODEL", "claude-3-5-haiku-latest"),
        asr_api_key=os.environ.get("ASR_API_KEY", ""),
        tts_api_key=os.environ.get("TTS_API_KEY", ""),
        tts_voice_id=os.environ.get("TTS_VOICE_ID", ""),
    )
