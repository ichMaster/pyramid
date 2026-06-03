#pragma once

// Pyramid v0.2 — LLM chat API request/response shaping (Anthropic Messages).
//
// Pure JSON build/parse for the Anthropic Messages API
// (https://docs.anthropic.com/en/api/messages). Kept free of any Arduino/
// network dependency so it is host-testable against recorded mock responses
// (see ../test/test_chat_api.cpp); the .ino owns the actual TLS POST and the
// x-api-key / anthropic-version headers. Matches ARCHITECTURE §Contracts (LLM
// call): system = persona, messages[]; output = reply text.
//
// v0.2 sends a single user turn; the rolling history window arrives in v0.3
// (PYR-003). Requires ArduinoJson v7 (header-only, compiles on host too).

#include <string>
#include <vector>

#include <ArduinoJson.h>

#include "history.h"

namespace pyramid {

// Build the Messages API request body:
//   {"model": <model>, "max_tokens": <maxTokens>, "stream": true,
//    "system": <persona>,
//    "messages": [{"role": <turn.role>, "content": <turn.content>}, ...]}
// The persona is a top-level `system` field (not a message); `turns` carries
// the windowed conversation history (v0.3) and must start with a user turn and
// alternate. `stream:true` requests an SSE response (see sse.h) so the device
// can show time-to-first-token. ArduinoJson handles all escaping and passes
// UTF-8 (Ukrainian) through as raw bytes.
inline std::string buildChatRequest(const std::string& model,
                                    const std::string& persona,
                                    const std::vector<Turn>& turns,
                                    int maxTokens) {
  JsonDocument doc;
  doc["model"] = model;
  doc["max_tokens"] = maxTokens;
  doc["stream"] = true;
  doc["system"] = persona;
  JsonArray messages = doc["messages"].to<JsonArray>();

  for (const Turn& t : turns) {
    JsonObject m = messages.add<JsonObject>();
    m["role"] = t.role;
    m["content"] = t.content;
  }

  std::string out;
  serializeJson(doc, out);
  return out;
}

// Whether an HTTP status from the LLM call is worth a bounded retry: 429
// (rate-limited) and 5xx (server) are transient; 4xx (client/auth) are not.
inline bool isRetryableHttpStatus(int status) {
  return status == 429 || (status >= 500 && status < 600);
}

// Token accounting reported by the Messages API `usage` object.
struct Usage {
  int inputTokens = 0;
  int outputTokens = 0;
  int total() const { return inputTokens + outputTokens; }
};

// Parse a Messages API response. On success sets `reply` to the text of the
// first `text` content block and returns true. On an API error object,
// malformed JSON, or missing/empty content, sets `err` to a readable line and
// returns false (so the caller surfaces it without crashing the loop). When
// `usage` is non-null, it is filled from the response `usage` on success.
inline bool parseChatReply(const std::string& body, std::string& reply,
                           std::string& err, Usage* usage = nullptr) {
  JsonDocument doc;
  DeserializationError jerr = deserializeJson(doc, body);
  if (jerr) {
    err = std::string("json parse error: ") + jerr.c_str();
    return false;
  }

  // API-level error object: {"type":"error","error":{"message": "..."}}.
  if (doc["error"].is<JsonObject>()) {
    const char* m = doc["error"]["message"] | "unknown API error";
    err = std::string("api error: ") + m;
    return false;
  }

  // Success: content is an array of blocks; take the first {"type":"text"}.
  if (!doc["content"].is<JsonArrayConst>()) {
    err = "no content array in response";
    return false;
  }
  for (JsonObjectConst block : doc["content"].as<JsonArrayConst>()) {
    if (block["type"] == "text") {
      reply = block["text"] | "";
      if (reply.empty()) {
        err = "empty reply content";
        return false;
      }
      if (usage) {
        usage->inputTokens = doc["usage"]["input_tokens"] | 0;
        usage->outputTokens = doc["usage"]["output_tokens"] | 0;
      }
      return true;
    }
  }
  err = "no text block in response";
  return false;
}

}  // namespace pyramid
