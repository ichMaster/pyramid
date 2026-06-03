#pragma once

// Pyramid v0.3+ — Anthropic streaming (SSE) decode.
//
// Pure, Arduino-free logic for reading a streamed Messages API response so the
// device can show a genuine time-to-first-token and print tokens as they
// arrive. Three layers, each host-testable (see ../test/test_sse.cpp):
//   1. Dechunker     — decode HTTP `Transfer-Encoding: chunked` body bytes.
//   2. extractSseData — pull the JSON payload out of a `data:` SSE line.
//   3. parseStreamEvent — classify one event JSON + extract text / usage.
// The .ino feeds raw socket bytes through Dechunker -> a LineReader -> these,
// owning only the TLS read loop (which needs the board to verify).

#include <cstddef>
#include <string>

#include <ArduinoJson.h>

namespace pyramid {

// Incremental HTTP chunked-transfer decoder. feed() appends the decoded body
// bytes to `out`; state persists across calls, so input may be split anywhere
// (including mid-line, as real TCP reads are).
class Dechunker {
 public:
  void feed(const char* data, std::size_t n, std::string& out) {
    for (std::size_t i = 0; i < n; ++i) {
      const char c = data[i];
      switch (state_) {
        case Size:
          if (c == '\n') {
            remaining_ = parseHex(sizeLine_);
            sizeLine_.clear();
            state_ = (remaining_ == 0) ? Done : Data;
          } else if (c != '\r') {
            sizeLine_.push_back(c);
          }
          break;
        case Data:
          out.push_back(c);
          if (--remaining_ == 0) state_ = AfterData;
          break;
        case AfterData:          // consume the CRLF that follows chunk data
          if (c == '\n') state_ = Size;
          break;
        case Done:
          break;                 // trailing chunk / trailers: ignore
      }
    }
  }

 private:
  enum State { Size, Data, AfterData, Done };

  static std::size_t parseHex(const std::string& s) {
    std::size_t v = 0;
    for (char c : s) {
      int d;
      if (c >= '0' && c <= '9') d = c - '0';
      else if (c >= 'a' && c <= 'f') d = 10 + (c - 'a');
      else if (c >= 'A' && c <= 'F') d = 10 + (c - 'A');
      else break;                // ';' chunk-extension or stray char
      v = v * 16 + static_cast<std::size_t>(d);
    }
    return v;
  }

  State state_ = Size;
  std::size_t remaining_ = 0;
  std::string sizeLine_;
};

// If `line` is an SSE `data:` line, copy its JSON payload into `out` and return
// true. Handles "data: {...}" and "data:{...}"; ignores event:/comment/blank.
inline bool extractSseData(const std::string& line, std::string& out) {
  static const char kPrefix[] = "data:";
  const std::size_t plen = sizeof(kPrefix) - 1;
  if (line.compare(0, plen, kPrefix) != 0) return false;
  std::size_t i = plen;
  if (i < line.size() && line[i] == ' ') ++i;  // optional single space
  out = line.substr(i);
  return !out.empty();
}

// One decoded SSE event, as far as the turn loop cares.
struct StreamEvent {
  enum Kind { kOther, kMessageStart, kTextDelta, kMessageDelta, kMessageStop, kError };
  Kind kind = kOther;
  std::string text;      // kTextDelta
  int inputTokens = 0;   // kMessageStart
  int outputTokens = 0;  // kMessageStart / kMessageDelta (cumulative)
  std::string error;     // kError
};

// Parse one SSE event JSON (the payload from a `data:` line). Returns true for
// events the loop acts on (message_start / text delta / message_delta /
// message_stop / error); false for ping, content_block_start/stop, non-text
// deltas, and anything unparseable.
inline bool parseStreamEvent(const std::string& json, StreamEvent& ev) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;

  const std::string type(doc["type"] | "");
  if (type == "message_start") {
    ev.kind = StreamEvent::kMessageStart;
    ev.inputTokens = doc["message"]["usage"]["input_tokens"] | 0;
    ev.outputTokens = doc["message"]["usage"]["output_tokens"] | 0;
    return true;
  }
  if (type == "content_block_delta") {
    const std::string dt(doc["delta"]["type"] | "");
    if (dt != "text_delta") return false;
    ev.kind = StreamEvent::kTextDelta;
    ev.text = doc["delta"]["text"] | "";
    return true;
  }
  if (type == "message_delta") {
    ev.kind = StreamEvent::kMessageDelta;
    ev.outputTokens = doc["usage"]["output_tokens"] | 0;
    return true;
  }
  if (type == "message_stop") {
    ev.kind = StreamEvent::kMessageStop;
    return true;
  }
  if (type == "error") {
    ev.kind = StreamEvent::kError;
    ev.error = std::string(doc["error"]["message"] | "stream error");
    return true;
  }
  return false;
}

}  // namespace pyramid
