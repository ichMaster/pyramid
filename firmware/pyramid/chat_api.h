#pragma once

// Pyramid v0.2 — LLM chat API request/response shaping.
//
// Pure JSON build/parse for an OpenAI-compatible chat-completions API
// (DeepSeek / Qwen / OpenAI / etc. all speak this shape). Kept free of any
// Arduino/network dependency so it is host-testable against recorded mock
// responses (see ../test/test_chat_api.cpp); the .ino owns the actual TLS
// POST. Matches ARCHITECTURE §Contracts (LLM call): system = persona,
// messages[]; output = choices[0].message.content.
//
// v0.2 sends a single user turn; the rolling history window arrives in v0.3
// (PYR-003). Requires ArduinoJson v7 (header-only, compiles on host too).

#include <string>

#include <ArduinoJson.h>

namespace pyramid {

// Build the chat-completions request body:
//   {"model": <model>,
//    "messages": [{"role":"system","content":<persona>},
//                 {"role":"user","content":<userText>}]}
// ArduinoJson handles all escaping (quotes, newlines) and passes UTF-8
// (Ukrainian) through as raw bytes.
inline std::string buildChatRequest(const std::string& model,
                                    const std::string& persona,
                                    const std::string& userText) {
  JsonDocument doc;
  doc["model"] = model;
  JsonArray messages = doc["messages"].to<JsonArray>();

  JsonObject sys = messages.add<JsonObject>();
  sys["role"] = "system";
  sys["content"] = persona;

  JsonObject usr = messages.add<JsonObject>();
  usr["role"] = "user";
  usr["content"] = userText;

  std::string out;
  serializeJson(doc, out);
  return out;
}

// Parse a chat-completions response. On success sets `reply` to
// choices[0].message.content and returns true. On an API error object,
// malformed JSON, or missing/empty content, sets `err` to a readable line and
// returns false (so the caller surfaces it without crashing the loop).
inline bool parseChatReply(const std::string& body, std::string& reply,
                           std::string& err) {
  JsonDocument doc;
  DeserializationError jerr = deserializeJson(doc, body);
  if (jerr) {
    err = std::string("json parse error: ") + jerr.c_str();
    return false;
  }

  // API-level error object: {"error":{"message": "..."}}.
  if (doc["error"].is<JsonObject>()) {
    const char* m = doc["error"]["message"] | "unknown API error";
    err = std::string("api error: ") + m;
    return false;
  }

  JsonVariant content = doc["choices"][0]["message"]["content"];
  if (!content.is<const char*>()) {
    err = "no reply content in response";
    return false;
  }
  reply = content.as<const char*>();
  if (reply.empty()) {
    err = "empty reply content";
    return false;
  }
  return true;
}

}  // namespace pyramid
