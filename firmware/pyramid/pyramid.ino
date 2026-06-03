// Pyramid — M5Stack Voice AI Chatbot.
// v0.2 (PYR-002): text chat loop.
//
// Boots the AtomS3R + Echo Base, brings up Wi-Fi from config.h, and turns the
// USB-CDC port into a line-oriented text channel: a typed line becomes a
// `text_in` event, is sent to a cloud LLM over direct HTTPS (persona from
// config), and the model's reply is printed back. All status logging goes
// through logf(), gated by DEBUG_SERIAL. The device stays thin — the persona
// lives in config, not in logic; no memory/decisions on-device.
//
// Build/flash: Arduino IDE (board "M5AtomS3R", M5Unified + ArduinoJson) or
//   arduino-cli compile --fqbn m5stack:esp32:m5stack_atoms3r firmware/pyramid
// Copy config.example.h -> config.h first and fill in Wi-Fi + LLM keys.

#include <HTTPClient.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include "chat_api.h"
#include "config.h"
#include "line_reader.h"
#include "serial_protocol.h"

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kWifiTimeoutMs = 20000;
constexpr uint32_t kHttpConnectMs = 8000;
constexpr uint32_t kHttpReadMs = 15000;

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

// One synchronous chat turn: POST the persona + user text to the LLM over
// HTTPS and read back the reply. Returns true and sets `reply` on success;
// returns false and sets `err` (a readable line) on any transport / HTTP /
// JSON failure, so the caller never hangs or crashes the loop. One turn at a
// time — streaming and history come later (v0.3 / v1).
bool llmTurn(const std::string& userText, std::string& reply,
             std::string& err) {
  WiFiClientSecure client;
  client.setInsecure();  // v0: no cert pinning under the private allowlist model

  HTTPClient http;
  http.setConnectTimeout(kHttpConnectMs);
  http.setTimeout(kHttpReadMs);
  if (!http.begin(client, LLM_ENDPOINT)) {
    err = "http begin failed";
    return false;
  }
  http.addHeader("content-type", "application/json");
  http.addHeader("x-api-key", LLM_API_KEY);
  http.addHeader("anthropic-version", LLM_ANTHROPIC_VERSION);

  const std::string body = pyramid::buildChatRequest(
      LLM_MODEL, LLM_PERSONA, userText, LLM_MAX_TOKENS);
  const int status = http.POST(String(body.c_str()));

  if (status <= 0) {
    err = std::string("transport error: ") +
          HTTPClient::errorToString(status).c_str();
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  if (status < 200 || status >= 300) {
    std::string discard, perr;
    // Prefer the API's own error message if the body carries one.
    pyramid::parseChatReply(payload.c_str(), discard, perr);
    err = "http " + std::to_string(status) +
          (perr.empty() ? "" : ": " + perr);
    return false;
  }

  return pyramid::parseChatReply(payload.c_str(), reply, err);
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setTextSize(2);

  Serial.begin(kSerialBaud);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) delay(10);  // let USB-CDC enumerate

  Serial.println("Pyramid v0.2 -- text chat loop");
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
        if (WiFi.status() != WL_CONNECTED) {
          // Wi-Fi auto-reconnect/backoff lands in v0.3 (PYR-003).
          Serial.println("error: offline (no wifi)");
        } else {
          logf("thinking...");
          showStatus("thinking");
          std::string reply, err;
          if (llmTurn(ev.text, reply, err)) {
            Serial.println(reply.c_str());
          } else {
            Serial.print("error: ");
            Serial.println(err.c_str());
          }
          showStatus("wifi OK");
        }
      }
    }
  }

  if (M5.BtnA.wasPressed()) {
    logf("button A pressed");
  }

  delay(5);
}
