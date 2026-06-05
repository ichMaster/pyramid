"""System-prompt assembly — kept behind a seam.

In v2.1 the persona is a fixed string (the firmware-era default). v2.2 replaces
this with the configurable ``Role`` (Name + authored Canon); v3 folds in the
daily temperament. Keeping assembly here means those phases swap one function.
"""

from __future__ import annotations

DEFAULT_PERSONA = (
    "Ти — Піраміда, лаконічний і доброзичливий україномовний голосовий помічник. "
    "Відповідай коротко й по суті, природною розмовною українською."
)


def assemble_system(persona: str | None = None) -> str:
    """Build the system prompt. v2.2: canon + persona; v3: + temperament."""
    return persona or DEFAULT_PERSONA
