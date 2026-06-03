// Host unit test for the pure v0.2 LLM chat API logic (chat_api.h):
// request building and response parsing, against recorded mock responses.
// No network — only ArduinoJson, which compiles on the host:
//
//   c++ -std=c++17 -I../pyramid -I<ArduinoJson>/src \
//     test_chat_api.cpp -o /tmp/test_chat_api && /tmp/test_chat_api
//
// (The firmware/README.md has the exact command with the resolved path.)

#include <cstdio>
#include <string>

#include <ArduinoJson.h>

#include "chat_api.h"

using pyramid::buildChatRequest;
using pyramid::parseChatReply;

static int g_failures = 0;

#define CHECK(cond, msg)                  \
  do {                                    \
    if (!(cond)) {                        \
      std::printf("FAIL: %s\n", (msg));   \
      ++g_failures;                       \
    }                                     \
  } while (0)

int main() {
  // 1. buildChatRequest emits valid JSON: model + max_tokens + top-level
  //    system(persona) + a single user message (Anthropic Messages shape).
  {
    std::string body =
        buildChatRequest("claude-haiku-4-5-20251001", "Be kind.", "hello", 1024);
    JsonDocument doc;
    CHECK(!deserializeJson(doc, body), "build: valid JSON");
    CHECK(doc["model"] == "claude-haiku-4-5-20251001", "build: model field");
    CHECK(doc["max_tokens"] == 1024, "build: max_tokens field");
    CHECK(doc["system"] == "Be kind.", "build: top-level system persona");
    CHECK(doc["messages"].size() == 1, "build: one message");
    CHECK(doc["messages"][0]["role"] == "user", "build: msg0 is user");
    CHECK(doc["messages"][0]["content"] == "hello", "build: user content");
    CHECK(!doc["messages"][0]["role"].is<const char*>() ||
              doc["messages"][0]["role"] != "system",
          "build: no system role in messages");
  }

  // 2. Escaping + UTF-8: quotes survive and Ukrainian round-trips.
  {
    std::string body = buildChatRequest("m", "p", "він сказав \"привіт\"", 256);
    JsonDocument doc;
    CHECK(!deserializeJson(doc, body), "build: escaped JSON still valid");
    CHECK(doc["messages"][0]["content"] == "він сказав \"привіт\"",
          "build: quotes + UTF-8 preserved");
  }

  // 3. parseChatReply: success extracts the first text block's text.
  {
    std::string resp =
        R"({"type":"message","role":"assistant",)"
        R"("content":[{"type":"text","text":"Привіт!"}]})";
    std::string reply, err;
    CHECK(parseChatReply(resp, reply, err), "parse: success returns true");
    CHECK(reply == "Привіт!", "parse: extracts text block");
    CHECK(err.empty(), "parse: no error on success");
  }

  // 3b. Skips a leading non-text block (e.g. a future tool_use) to find text.
  {
    std::string resp =
        R"({"content":[{"type":"tool_use","name":"x"},)"
        R"({"type":"text","text":"Готово"}]})";
    std::string reply, err;
    CHECK(parseChatReply(resp, reply, err), "parse: finds text after non-text");
    CHECK(reply == "Готово", "parse: picks the text block");
  }

  // 4. API-level error object -> false with the API message.
  {
    std::string resp =
        R"({"type":"error","error":{"type":"authentication_error",)"
        R"("message":"invalid x-api-key"}})";
    std::string reply, err;
    CHECK(!parseChatReply(resp, reply, err), "parse: api error -> false");
    CHECK(err.find("invalid x-api-key") != std::string::npos,
          "parse: surfaces api message");
  }

  // 5. Malformed JSON -> false with a parse error.
  {
    std::string reply, err;
    CHECK(!parseChatReply("{not json", reply, err), "parse: malformed -> false");
    CHECK(err.find("parse") != std::string::npos, "parse: reports parse error");
  }

  // 6. Missing content array -> false.
  {
    std::string resp = R"({"type":"message","role":"assistant"})";
    std::string reply, err;
    CHECK(!parseChatReply(resp, reply, err), "parse: missing content -> false");
  }

  // 7. Empty text -> false.
  {
    std::string resp = R"({"content":[{"type":"text","text":""}]})";
    std::string reply, err;
    CHECK(!parseChatReply(resp, reply, err), "parse: empty content -> false");
  }

  // 8. Content array with no text block -> false.
  {
    std::string resp = R"({"content":[{"type":"tool_use","name":"x"}]})";
    std::string reply, err;
    CHECK(!parseChatReply(resp, reply, err), "parse: no text block -> false");
  }

  if (g_failures == 0) {
    std::printf("ok - all tests passed\n");
    return 0;
  }
  std::printf("FAILED - %d check(s) failed\n", g_failures);
  return 1;
}
