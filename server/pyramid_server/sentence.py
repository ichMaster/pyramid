"""Streaming clause/sentence splitter (PYR-021, rec #3).

Consumes a stream of LLM token deltas and yields complete phrases at sentence /
clause boundaries, so each phrase can be synthesized and streamed as ``tts_audio``
while the model is still generating — the first audio leaves before the reply is
finished. Pure and unit-testable (no I/O).

Rules:
- Sentence enders ``. ! ? …`` and the clause ender ``;`` are boundaries, but only
  when followed by whitespace (so ``3.5``, times, and a punctuation mark sitting
  at the very end of the buffer don't split prematurely).
- Common Ukrainian abbreviations (``напр.``, ``т.д.`` …) don't trigger a split.
- A ``max_chars`` guard force-flushes a runaway clause so one long phrase can't
  stall the stream.
- ``flush()`` returns whatever remains (the short-reply / final-tail path).
"""

from __future__ import annotations

SENTENCE_ENDS = frozenset(".!?…")
CLAUSE_ENDS = frozenset(";")
_BOUNDARY = SENTENCE_ENDS | CLAUSE_ENDS

DEFAULT_ABBREVIATIONS = frozenset(
    {
        "напр.", "т.д.", "т.п.", "т.зв.", "та ін.", "ін.", "див.", "рис.", "табл.",
        "стор.", "с.", "м.", "вул.", "просп.", "р.", "рр.", "ст.", "ст.ст.",
        "грн.", "коп.", "тис.", "млн.", "млрд.", "проф.", "акад.",
    }
)


class PhraseSplitter:
    def __init__(self, *, max_chars: int = 160, abbreviations=DEFAULT_ABBREVIATIONS):
        self.max_chars = max_chars
        self.abbreviations = abbreviations
        self.buf = ""

    def feed(self, delta: str) -> list[str]:
        """Append a delta and return any phrases that are now complete."""
        self.buf += delta
        out: list[str] = []
        while True:
            cut = self._find_boundary(self.buf)
            if cut is None:
                break
            phrase = self.buf[:cut].strip()
            self.buf = self.buf[cut:].lstrip()
            if phrase:
                out.append(phrase)
        # Force-flush a runaway clause (no boundary but too long).
        while len(self.buf) >= self.max_chars:
            cut = self._force_cut(self.buf)
            phrase = self.buf[:cut].strip()
            self.buf = self.buf[cut:].lstrip()
            if not phrase:
                break
            out.append(phrase)
        return out

    def flush(self) -> str:
        """Return and clear the remaining buffer (the final / short-reply tail)."""
        tail = self.buf.strip()
        self.buf = ""
        return tail

    # --- internals -----------------------------------------------------------
    def _find_boundary(self, s: str) -> int | None:
        n = len(s)
        for i, ch in enumerate(s):
            if ch not in _BOUNDARY:
                continue
            nxt = s[i + 1] if i + 1 < n else ""
            if nxt and not nxt.isspace():
                continue  # e.g. "3.5", a colon in "5;x", or mid-token punctuation
            # Consume a run of sentence enders ("?!", "...").
            j = i
            while j + 1 < n and s[j + 1] in SENTENCE_ENDS:
                j += 1
            end = j + 1
            if ch == "." and self._is_abbrev(s[:end]):
                continue
            if nxt == "":
                # Ender sits at the very end of the buffer — wait for the next
                # delta to confirm it's a real boundary (avoids cutting "3." in "3.5").
                continue
            return end
        return None

    def _is_abbrev(self, s: str) -> bool:
        low = s.lower()
        return any(low.endswith(abbr) for abbr in self.abbreviations)

    def _force_cut(self, s: str) -> int:
        window = s[: self.max_chars]
        space = window.rfind(" ")
        return space + 1 if space > 0 else self.max_chars
