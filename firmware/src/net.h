#pragma once

// Pyramid — Wi-Fi bring-up and non-blocking reconnect supervisor (app namespace).

namespace app {

bool connectWiFi();   // initial blocking connect (bounded); returns success
void serviceWiFi();   // call every loop tick: tracks offline + reconnects w/ backoff

}  // namespace app
