#include "ws_client.h"

#include <WebSocketsClient.h>

#include "config.h"
#include "log.h"
#include "turn.h"  // onWsConnected / onWsDisconnected / onWsText / onTtsAudio

namespace app {

static WebSocketsClient s_ws;
static bool s_connected = false;

static void onEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      s_connected = true;
      onWsConnected();
      break;
    case WStype_DISCONNECTED:
      s_connected = false;
      onWsDisconnected();
      break;
    case WStype_TEXT:
      onWsText(std::string(reinterpret_cast<const char*>(payload), length));
      break;
    case WStype_BIN:
      onTtsAudio(payload, length);  // tts_audio — play immediately (early playback)
      break;
    default:
      break;
  }
}

void wsBegin() {
#if SERVER_USE_TLS
  s_ws.beginSSL(SERVER_HOST, SERVER_PORT, SERVER_PATH);  // wss (self-signed in dev)
#else
  s_ws.begin(SERVER_HOST, SERVER_PORT, SERVER_PATH);     // ws (LAN dev)
#endif
  s_ws.onEvent(onEvent);
  s_ws.setReconnectInterval(2000);            // backoff handled by the lib
  s_ws.enableHeartbeat(15000, 3000, 2);       // WS-level ping/pong keepalive
  logf("ws: connecting to %s:%d%s", SERVER_HOST, (int)SERVER_PORT, SERVER_PATH);
}

void wsLoop() { s_ws.loop(); }

bool wsConnected() { return s_connected; }

void wsSendText(const std::string& payload) {
  // Non-const overload is present across lib versions; copy-safe.
  s_ws.sendTXT(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(payload.data())),
               payload.size());
}

void wsSendBinary(const uint8_t* data, size_t len) {
  s_ws.sendBIN(const_cast<uint8_t*>(data), len);
}

}  // namespace app
