#include "net.h"

#include <WiFi.h>

#include "app_state.h"
#include "backoff.h"
#include "log.h"
#include "ui.h"

namespace app {

bool connectWiFi() {
  logf("wifi: connecting to \"%s\"", WIFI_SSID);
  setState(pyramid::TurnState::Offline);  // not connected yet — input paused
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > kWifiTimeoutMs) {
      logf("wifi: connect timeout (will retry in loop)");
      setState(pyramid::TurnState::Offline);
      return false;
    }
    delay(250);
  }
  logf("wifi: connected, ip=%s", WiFi.localIP().toString().c_str());
  applyEvent(pyramid::TurnEvent::WifiUp);  // Offline -> Idle
  return true;
}

// Non-blocking Wi-Fi supervisor: tracks offline state and reconnects with
// exponential backoff. Called every loop tick; input is paused while offline.
void serviceWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (g_offline) {
      g_offline = false;
      g_wifiAttempt = 0;
      logf("wifi: reconnected, ip=%s", WiFi.localIP().toString().c_str());
      applyEvent(pyramid::TurnEvent::WifiUp);  // Offline -> Idle
    }
    return;
  }

  if (!g_offline) {
    g_offline = true;
    logf("wifi: connection lost — pausing input");
    applyEvent(pyramid::TurnEvent::WifiLost);  // -> Offline
  }

  const uint32_t now = millis();
  if (now >= g_nextWifiTryMs) {
    const uint32_t wait =
        pyramid::backoffDelayMs(g_wifiAttempt, kWifiBackoffBaseMs, kWifiBackoffCapMs);
    logf("wifi: reconnect attempt %d (next backoff %u ms)", g_wifiAttempt + 1,
         static_cast<unsigned>(wait));
    WiFi.reconnect();
    g_nextWifiTryMs = now + wait;
    ++g_wifiAttempt;
  }
}

}  // namespace app
