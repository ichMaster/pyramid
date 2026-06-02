// Pyramid — M5Stack Voice AI Chatbot.
// v0.1 (PYR-001): device skeleton + serial.
//
// Boots the AtomS3R + Echo Base, brings up Wi-Fi from config.h, and turns the
// USB-CDC port into a line-oriented text channel: a typed line becomes a
// `text_in` event and is echoed back. All status logging goes through logf(),
// gated by DEBUG_SERIAL. The device stays thin — no persona/LLM logic here
// (that arrives in v0.2 / PYR-002).
//
// Build/flash: Arduino IDE (board "M5AtomS3R", M5Unified) or
//   arduino-cli compile --fqbn m5stack:esp32:m5stack_atoms3r firmware/pyramid
// Copy config.example.h -> config.h first.

#include <M5Unified.h>
#include <WiFi.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include "config.h"
#include "line_reader.h"
#include "serial_protocol.h"

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kWifiTimeoutMs = 20000;

pyramid::LineReader g_reader;

// All status logging routes through here and is gated by DEBUG_SERIAL, so the
// device can be quietened later without touching call sites.
void logf(const char* fmt, ...) {
  if (!DEBUG_SERIAL) return;
  char line[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  Serial.print("[log] ");
  Serial.println(line);
}

// Coarse status line on the LCD (full state machine lands in v1.4).
void showStatus(const char* s) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.print(s);
}

bool connectWiFi() {
  logf("wifi: connecting to \"%s\"", WIFI_SSID);
  showStatus("wifi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > kWifiTimeoutMs) {
      logf("wifi: connect timeout");
      showStatus("wifi FAIL");
      return false;  // auto-reconnect/backoff arrives in v0.3 (PYR-003)
    }
    delay(250);
  }
  logf("wifi: connected, ip=%s", WiFi.localIP().toString().c_str());
  showStatus("wifi OK");
  return true;
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setTextSize(2);

  Serial.begin(kSerialBaud);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) delay(10);  // let USB-CDC enumerate

  Serial.println("Pyramid v0.1 -- device skeleton + serial");
  showStatus("boot");

  connectWiFi();

  Serial.println("ready. type a line:");
}

void loop() {
  M5.update();

  // Non-blocking: drain whatever bytes arrived this tick through the reader.
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    std::string line;
    if (g_reader.feed(c, line)) {
      pyramid::TextIn ev;
      if (pyramid::parseTextIn(line, ev)) {
        logf("text_in: \"%s\"", ev.text.c_str());
        Serial.println(pyramid::formatReply(ev).c_str());  // v0.1: echo
      }
    }
  }

  if (M5.BtnA.wasPressed()) {
    logf("button A pressed");
  }

  delay(5);
}
