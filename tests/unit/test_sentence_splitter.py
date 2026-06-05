"""Unit: the streaming clause/sentence splitter — sentence end, clause end,
abbreviation no-split, max-length flush, Ukrainian punctuation. (PYR-021.)"""

from __future__ import annotations

from pyramid_server.sentence import PhraseSplitter


def feed_all(text, *, chunk=3, max_chars=160):
    """Feed ``text`` in small chunks (simulating token deltas); return all phrases."""
    sp = PhraseSplitter(max_chars=max_chars)
    out = []
    for i in range(0, len(text), chunk):
        out.extend(sp.feed(text[i : i + chunk]))
    tail = sp.flush()
    if tail:
        out.append(tail)
    return out


def test_sentence_end_splits():
    assert feed_all("Привіт світ. Як справи. ") == ["Привіт світ.", "Як справи."]


def test_ukrainian_question_and_exclamation():
    assert feed_all("Як справи? Добре! ") == ["Як справи?", "Добре!"]


def test_ellipsis_and_run_of_enders():
    assert feed_all("Ну… Ого?! Кінець. ") == ["Ну…", "Ого?!", "Кінець."]


def test_clause_semicolon_splits():
    assert feed_all("Раз; два; три. ") == ["Раз;", "два;", "три."]


def test_abbreviation_does_not_split():
    # "напр." is an abbreviation — only the final period ends the sentence.
    assert feed_all("Візьмемо, напр. цей випадок. ") == ["Візьмемо, напр. цей випадок."]


def test_decimal_not_split():
    # "3.5" must not split (period not followed by whitespace).
    assert feed_all("Ціна 3.5 грн усього. ") == ["Ціна 3.5 грн усього."]


def test_comma_does_not_split():
    assert feed_all("Один, два, три і чотири. ") == ["Один, два, три і чотири."]


def test_max_length_force_flush():
    long_clause = "слово " * 40  # ~240 chars, no sentence punctuation
    phrases = feed_all(long_clause, max_chars=60)
    assert len(phrases) >= 3
    assert all(len(p) <= 60 for p in phrases)


def test_flush_returns_tail_without_punctuation():
    sp = PhraseSplitter()
    assert sp.feed("без крапки в кінці") == []
    assert sp.flush() == "без крапки в кінці"


def test_short_reply_single_phrase():
    # A short reply with no internal boundary → one phrase (the fallback path).
    assert feed_all("Ок. ") == ["Ок."]
