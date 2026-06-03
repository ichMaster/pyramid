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
#include <vector>

#include <ArduinoJson.h>

#include "chat_api.h"

using pyramid::buildChatRequest;
using pyramid::isRetryableHttpStatus;
using pyramid::parseChatReply;
using pyramid::Turn;

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
  //    system(persona) + the history turns (Anthropic Messages shape).
  {
    std::vector<Turn> turns = {{"user", "hello"}};
    std::string body =
        buildChatRequest("claude-haiku-4-5-20251001", "Be kind.", turns, 1024);
    JsonDocument doc;
    CHECK(!deserializeJson(doc, body), "build: valid JSON");
    CHECK(doc["model"] == "claude-haiku-4-5-20251001", "build: model field");
    CHECK(doc["max_tokens"] == 1024, "build: max_tokens field");
    CHECK(doc["system"] == "Be kind.", "build: top-level system persona");
    CHECK(doc["messages"].size() == 1, "build: one message");
    CHECK(doc["messages"][0]["role"] == "user", "build: msg0 is user");
    CHECK(doc["messages"][0]["content"] == "hello", "build: user content");
  }

  // 1b. Multi-turn history maps in order, roles preserved.
  {
    std::vector<Turn> turns = {
        {"user", "u1"}, {"assistant", "a1"}, {"user", "u2"}};
    std::string body = buildChatRequest("m", "p", turns, 512);
    JsonDocument doc;
    CHECK(!deserializeJson(doc, body), "build(history): valid JSON");
    CHECK(doc["messages"].size() == 3, "build(history): three messages");
    CHECK(doc["messages"][1]["role"] == "assistant",
          "build(history): assistant in the middle");
    CHECK(doc["messages"][2]["content"] == "u2", "build(history): last user");
  }

  // 2. Escaping + UTF-8: quotes survive and Ukrainian round-trips.
  {
    std::vector<Turn> turns = {{"user", "він сказав \"привіт\""}};
    std::string body = buildChatRequest("m", "p", turns, 256);
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

  // 9. isRetryableHttpStatus: 429 + 5xx are transient; 4xx + 2xx are not.
  {
    CHECK(isRetryableHttpStatus(429), "retry: 429 -> yes");
    CHECK(isRetryableHttpStatus(500), "retry: 500 -> yes");
    CHECK(isRetryableHttpStatus(503), "retry: 503 -> yes");
    CHECK(!isRetryableHttpStatus(400), "retry: 400 -> no");
    CHECK(!isRetryableHttpStatus(401), "retry: 401 -> no");
    CHECK(!isRetryableHttpStatus(404), "retry: 404 -> no");
    CHECK(!isRetryableHttpStatus(200), "retry: 200 -> no");
  }

  if (g_failures == 0) {
    std::printf("ok - all tests passed\n");
    return 0;
  }
  std::printf("FAILED - %d check(s) failed\n", g_failures);
  return 1;
}
