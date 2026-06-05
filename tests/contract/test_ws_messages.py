"""Contract test: the device↔server message codec — every message shape, and
text-JSON vs binary-audio framing. Changing the contract changes this test.
(ARCHITECTURE §WS device↔server.)"""

from __future__ import annotations

import pytest

from pyramid_server import protocol as p
from pyramid_server.protocol import In, Out, ProtocolError, decode_text


# --- inbound (device → server) decode ---------------------------------------
@pytest.mark.parametrize(
    "frame",
    [
        {"type": In.HELLO, "proto_ver": "1.0", "device_token": "d", "audio_fmt": "pcm16"},
        {"type": In.LISTEN_START},
        {"type": In.LISTEN_STOP},
        {"type": In.TEXT_IN, "text": "привіт"},
        {"type": In.PING},
    ],
)
def test_decode_accepts_every_inbound_type(frame):
    import json

    msg = decode_text(json.dumps(frame))
    assert msg.type == frame["type"]
    assert msg.data == frame


@pytest.mark.parametrize(
    "raw",
    [
        "not json",
        "[1, 2, 3]",                 # not an object
        '{"no_type": true}',         # missing type
        '{"type": "tts_audio"}',     # binary frame has no text type
        '{"type": "bogus"}',         # unknown type
    ],
)
def test_decode_rejects_malformed(raw):
    with pytest.raises(ProtocolError):
        decode_text(raw)


# --- outbound (server → device) builders ------------------------------------
def test_outbound_builders_shapes():
    assert p.pong() == {"type": Out.PONG}
    assert p.asr_partial("h") == {"type": Out.ASR_PARTIAL, "text": "h"}
    assert p.asr("hi") == {"type": Out.ASR, "text": "hi"}
    assert p.reply("x") == {"type": Out.REPLY, "text": "x", "delta": False, "done": False}
    assert p.reply("x", delta=True, done=True) == {
        "type": Out.REPLY, "text": "x", "delta": True, "done": True,
    }
    assert p.text_out("o") == {"type": Out.TEXT_OUT, "text": "o"}
    assert p.tts_end() == {"type": Out.TTS_END}
    assert p.config_updated() == {"type": Out.CONFIG_UPDATED}
    assert p.restart() == {"type": Out.RESTART}
    assert p.error(p.ErrorCode.ASR_FAILED, "boom") == {
        "type": Out.ERROR, "code": "asr_failed", "msg": "boom",
    }


def test_encode_is_json_roundtrip():
    import json

    raw = p.encode(p.reply("café", delta=True))
    assert json.loads(raw) == {"type": "reply", "text": "café", "delta": True, "done": False}


# --- binary framing ----------------------------------------------------------
def test_audio_format_constants():
    # The single v2.1 audio format: PCM16 16 kHz mono (OPUS deferred).
    assert (p.AUDIO_SAMPLE_RATE, p.AUDIO_CHANNELS, p.AUDIO_FORMAT) == (16000, 1, "pcm16")
