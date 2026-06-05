"""Contract test: the ``hello`` handshake + ``proto_ver`` negotiation. An unknown
*major* is rejected with ``error{proto_unsupported}`` and the socket closes.
(ARCHITECTURE §WS device↔server / §Cross-cutting concerns.)"""

from __future__ import annotations

from pyramid_server import protocol as p
from pyramid_server.main import create_app
from tests.fakes.fake_device import connect


# --- unit: negotiate / parse_proto_ver --------------------------------------
def test_negotiate_same_major_ok():
    ok, ver = p.negotiate("1.0", server_major=1)
    assert ok and ver == (1, 0)


def test_negotiate_unknown_major_rejected():
    ok, ver = p.negotiate("2.3", server_major=1)
    assert not ok and ver == (2, 3)


def test_parse_proto_ver_forms():
    assert p.parse_proto_ver("1.5") == (1, 5)
    assert p.parse_proto_ver([1, 7]) == (1, 7)


# --- over the wire -----------------------------------------------------------
def test_hello_accepted_keeps_socket_open():
    app = create_app()
    with connect(app) as dev:
        dev.hello(proto_ver="1.0")
        dev.ping()  # only works if the socket is still open after hello
        assert dev.recv_json() == {"type": "pong"}


def test_hello_unknown_major_errors_and_closes():
    app = create_app()
    with connect(app) as dev:
        dev.hello(proto_ver="2.0")
        msg = dev.recv_json()
        assert msg["type"] == "error"
        assert msg["code"] == p.ErrorCode.PROTO_UNSUPPORTED
        assert dev.expect_closed()


def test_hello_malformed_proto_ver_errors_and_closes():
    app = create_app()
    with connect(app) as dev:
        dev.send_raw('{"type": "hello", "proto_ver": "garbage"}')
        msg = dev.recv_json()
        assert msg["type"] == "error" and msg["code"] == p.ErrorCode.PROTO_UNSUPPORTED
        assert dev.expect_closed()
