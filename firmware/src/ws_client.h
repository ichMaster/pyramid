#pragma once

// Pyramid v2.1 — WSS client to our server (app namespace).
//
// Wraps the duplex WebSocket connection: connect to SERVER_HOST/PORT/PATH,
// auto-reconnect, send device→server text/binary frames, and dispatch inbound
// frames to the turn handlers (ws_client.cpp → turn.h). The wire codec is in
// ws_protocol.h; the transport glue is here.

#include <cstddef>
#include <cstdint>
#include <string>

namespace app {

void wsBegin();                                    // configure + start connecting
void wsLoop();                                     // service the socket (every tick)
bool wsConnected();                                // connected to the server?
void wsSendText(const std::string& payload);       // a JSON text frame
void wsSendBinary(const uint8_t* data, size_t len);  // a binary audio frame

}  // namespace app
