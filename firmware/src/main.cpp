// Pyramid — M5Stack Voice AI Chatbot.
// v0.3 (PYR-003): quality and UX.
//
// Boots the AtomS3R + Echo Base, brings up Wi-Fi from config.h, and turns the
// USB-CDC port into a line-oriented text channel: a typed line becomes a
// `text_in` event, is sent (with a short rolling history) to a cloud LLM over
// direct HTTPS, and the model's reply is streamed back token by token (SSE).
// The loop is hardened:
// bounded retry on transient LLM failures, non-blocking Wi-Fi reconnect with
// exponential backoff (input paused while offline), and coarse LCD states. All
// status logging goes through logf(), gated by DEBUG_SERIAL. The device stays
// thin — persona + history shape the request, but no decisions live on-device.
//
// Build/flash (PlatformIO, from v1.1): from firmware/ run
//   pio run                 # compile
//   pio run -t upload       # flash the AtomS3R
//   pio device monitor      # serial @115200
// Copy src/config.example.h -> src/config.h first and fill in Wi-Fi + LLM keys.
// (v0 was built in the Arduino IDE; the sketch pyramid.ino moved to this file.)

#include <Arduino.h>
#include <HTTPClient.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "backoff.h"
#include "chat_api.h"
#include "config.h"
#include "history.h"
#include "line_reader.h"
#include "serial_protocol.h"
#include "sse.h"

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kWifiTimeoutMs = 20000;
constexpr uint32_t kHttpConnectMs = 8000;
constexpr uint32_t kHttpReadMs = 15000;

// Bounded retry for transient LLM failures (transport / 429 / 5xx).
constexpr int kMaxRetries = 2;
constexpr uint32_t kRetryBaseMs = 500;
constexpr uint32_t kRetryCapMs = 4000;

// Wi-Fi reconnect backoff (ARCHITECTURE §Error handling: ~0.5 -> 8 s).
constexpr uint32_t kWifiBackoffBaseMs = 500;
constexpr uint32_t kWifiBackoffCapMs = 8000;

pyramid::LineReader g_reader;
pyramid::History g_history(HISTORY_MAX_TURNS);

// Wi-Fi reconnect state (non-blocking).
bool g_offline = false;
int g_wifiAttempt = 0;
uint32_t g_nextWifiTryMs = 0;

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

// Coarse turn state on the LCD: idle / thinking / error / offline (the full
// state machine lands in v1.4).
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
      logf("wifi: connect timeout (will retry in loop)");
      showStatus("offline");
      return false;
    }
    delay(250);
  }
  logf("wifi: connected, ip=%s", WiFi.localIP().toString().c_str());
  showStatus("idle");
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
      showStatus("idle");
    }
    return;
  }

  if (!g_offline) {
    g_offline = true;
    logf("wifi: connection lost — pausing input");
    showStatus("offline");
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

// Outcome of a single HTTP attempt: ok (reply set), or failed (err set) with a
// hint on whether retrying is worthwhile. Also carries per-attempt timing and
// the token usage reported by the API.
struct Attempt {
  bool ok = false;
  bool retryable = false;
  std::string reply;
  std::string err;
  uint32_t firstMs = 0;  // request sent -> server response received
  uint32_t totalMs = 0;  // request sent -> body fully read
  pyramid::Usage usage;
};

// One synchronous HTTPS attempt: POST persona + history to the LLM and read
// back the reply. Never hangs — connect/read timeouts are bounded.
Attempt llmAttempt(const std::vector<pyramid::Turn>& turns) {
  Attempt r;
  WiFiClientSecure client;
  client.setInsecure();  // v0: no cert pinning under the private allowlist model

  HTTPClient http;
  http.setConnectTimeout(kHttpConnectMs);
  http.setTimeout(kHttpReadMs);
  if (!http.begin(client, LLM_ENDPOINT)) {
    r.err = "http begin failed";
    return r;
  }
  http.addHeader("content-type", "application/json");
  http.addHeader("x-api-key", LLM_API_KEY);
  http.addHeader("anthropic-version", LLM_ANTHROPIC_VERSION);

  http.addHeader("accept", "text/event-stream");

  const std::string body =
      pyramid::buildChatRequest(LLM_MODEL, LLM_PERSONA, turns, LLM_MAX_TOKENS);

  const uint32_t tStart = millis();
  const int status = http.POST(String(body.c_str()));

  if (status <= 0) {
    r.err = std::string("transport error: ") +
            HTTPClient::errorToString(status).c_str();
    r.retryable = true;  // transport-level, nothing streamed yet: worth a retry
    r.totalMs = millis() - tStart;
    http.end();
    return r;
  }

  if (status < 200 || status >= 300) {
    // Errors come back as a normal (non-streamed) JSON body.
    const String payload = http.getString();
    std::string discard, perr;
    pyramid::parseChatReply(payload.c_str(), discard, perr);
    r.err = "http " + std::to_string(status) + (perr.empty() ? "" : ": " + perr);
    r.retryable = pyramid::isRetryableHttpStatus(status);
    r.totalMs = millis() - tStart;
    http.end();
    return r;
  }

  // 2xx: read the SSE stream, printing tokens as they arrive and capturing the
  // time-to-first-token + usage. Raw socket bytes -> dechunk -> SSE lines ->
  // events (all of that decode path is host-tested in sse.h).
  WiFiClient* stream = http.getStreamPtr();
  pyramid::Dechunker dechunk;
  pyramid::LineReader sseLines;
  std::string decoded, line, dataJson;
  bool gotFirst = false, sawStop = false, streamErr = false;
  uint8_t buf[256];
  uint32_t lastRx = millis();

  while (!sawStop && !streamErr) {
    const int avail = stream ? stream->available() : 0;
    if (avail <= 0) {
      if (!http.connected() && (!stream || stream->available() == 0)) break;
      if (millis() - lastRx > kHttpReadMs) {
        r.err = "stream timeout";
        break;
      }
      delay(2);
      continue;
    }
    const int want = avail < static_cast<int>(sizeof(buf)) ? avail
                                                           : static_cast<int>(sizeof(buf));
    const int n = stream->readBytes(buf, want);
    if (n <= 0) {
      delay(2);
      continue;
    }
    lastRx = millis();

    decoded.clear();
    dechunk.feed(reinterpret_cast<const char*>(buf), n, decoded);
    for (char ch : decoded) {
      if (!sseLines.feed(ch, line)) continue;
      if (!pyramid::extractSseData(line, dataJson)) continue;
      pyramid::StreamEvent ev;
      if (!pyramid::parseStreamEvent(dataJson, ev)) continue;
      switch (ev.kind) {
        case pyramid::StreamEvent::kMessageStart:
          r.usage.inputTokens = ev.inputTokens;
          r.usage.outputTokens = ev.outputTokens;
          break;
        case pyramid::StreamEvent::kTextDelta:
          if (!gotFirst) {
            gotFirst = true;
            r.firstMs = millis() - tStart;  // genuine time-to-first-token
          }
          r.reply += ev.text;
          Serial.print(ev.text.c_str());  // stream to serial as it arrives
          break;
        case pyramid::StreamEvent::kMessageDelta:
          r.usage.outputTokens = ev.outputTokens;  // cumulative final count
          break;
        case pyramid::StreamEvent::kError:
          streamErr = true;
          r.err = ev.error;
          break;
        case pyramid::StreamEvent::kMessageStop:
          sawStop = true;
          break;
        default:
          break;
      }
      if (sawStop || streamErr) break;
    }
  }

  http.end();
  r.totalMs = millis() - tStart;
  if (gotFirst) {
    Serial.println();  // terminate the streamed reply line
    r.ok = true;       // a mid-stream hiccup after text keeps the partial reply
  } else if (r.err.empty()) {
    r.err = "empty stream";
  }
  return r;  // streamed turns are committed once they print; not retried
}

// A full chat turn with bounded retry + backoff. Returns the final Attempt:
// .ok with .reply / .usage / .firstMs on success, or !ok with .err once
// retries are spent.
Attempt llmTurn(const std::vector<pyramid::Turn>& turns) {
  Attempt a;
  for (int i = 0; i <= kMaxRetries; ++i) {
    if (i > 0) {
      const uint32_t wait =
          pyramid::backoffDelayMs(i - 1, kRetryBaseMs, kRetryCapMs);
      logf("llm: retry %d/%d in %u ms (%s)", i, kMaxRetries,
           static_cast<unsigned>(wait), a.err.c_str());
      delay(wait);
    }
    a = llmAttempt(turns);
    if (a.ok) return a;
    if (!a.retryable) break;
  }
  return a;
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setTextSize(2);

  Serial.begin(kSerialBaud);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) delay(10);  // let USB-CDC enumerate

  Serial.println("Pyramid v0.3 -- quality and UX");
  showStatus("boot");

  connectWiFi();

  Serial.println("ready. type a line:");
}

void loop() {
  M5.update();
  serviceWiFi();

  // Non-blocking: drain whatever bytes arrived this tick through the reader.
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    std::string line;
    if (g_reader.feed(c, line)) {
      pyramid::TextIn ev;
      if (pyramid::parseTextIn(line, ev)) {
        logf("text_in: \"%s\"", ev.text.c_str());
        if (g_offline) {
          Serial.println("offline: input paused (reconnecting)");
          continue;
        }
        logf("thinking...");
        showStatus("thinking");

        // Build the request from history + the pending user turn; commit to
        // history only on success so a failed call can't poison the context.
        std::vector<pyramid::Turn> req = g_history.turns();
        req.push_back(pyramid::Turn{"user", ev.text});

        const uint32_t turnStart = millis();
        const Attempt a = llmTurn(req);  // reply streams to serial inside
        const uint32_t allMs = millis() - turnStart;

        if (a.ok) {
          // Per-turn stats: first_token = time until the first streamed token;
          // total = whole turn incl. any retries; tokens from the API usage.
          Serial.printf(
              "[stats] first_token=%lu ms  total=%lu ms  tokens: in=%d out=%d total=%d\n",
              static_cast<unsigned long>(a.firstMs),
              static_cast<unsigned long>(allMs), a.usage.inputTokens,
              a.usage.outputTokens, a.usage.total());
          g_history.addUser(ev.text);
          g_history.addAssistant(a.reply);
          showStatus("idle");
        } else {
          Serial.print("error: ");
          Serial.println(a.err.c_str());
          Serial.printf("[stats] failed after %lu ms\n",
                        static_cast<unsigned long>(allMs));
          showStatus("error");
        }
      }
    }
  }

  if (M5.BtnA.wasPressed()) {
    logf("button A pressed");
  }

  delay(5);
}
