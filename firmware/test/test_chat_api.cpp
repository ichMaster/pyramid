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
  // 1. buildChatRequest emits valid JSON with model + system(persona) + user.
  {
    std::string body = buildChatRequest("gpt-4o-mini", "Be kind.", "hello");
    JsonDocument doc;
    CHECK(!deserializeJson(doc, body), "build: valid JSON");
    CHECK(doc["model"] == "gpt-4o-mini", "build: model field");
    CHECK(doc["messages"].size() == 2, "build: two messages");
    CHECK(doc["messages"][0]["role"] == "system", "build: msg0 is system");
    CHECK(doc["messages"][0]["content"] == "Be kind.", "build: persona content");
    CHECK(doc["messages"][1]["role"] == "user", "build: msg1 is user");
    CHECK(doc["messages"][1]["content"] == "hello", "build: user content");
  }

  // 2. Escaping + UTF-8: quotes survive and Ukrainian round-trips.
  {
    std::string body = buildChatRequest("m", "p", "він сказав \"привіт\"");
    JsonDocument doc;
    CHECK(!deserializeJson(doc, body), "build: escaped JSON still valid");
    CHECK(doc["messages"][1]["content"] == "він сказав \"привіт\"",
          "build: quotes + UTF-8 preserved");
  }

  // 3. parseChatReply: success extracts choices[0].message.content.
  {
    std::string resp =
        R"({"choices":[{"message":{"role":"assistant","content":"Привіт!"}}]})";
    std::string reply, err;
    CHECK(parseChatReply(resp, reply, err), "parse: success returns true");
    CHECK(reply == "Привіт!", "parse: extracts content");
    CHECK(err.empty(), "parse: no error on success");
  }

  // 4. API-level error object -> false with the API message.
  {
    std::string resp =
        R"({"error":{"message":"Invalid API key","type":"auth"}})";
    std::string reply, err;
    CHECK(!parseChatReply(resp, reply, err), "parse: api error -> false");
    CHECK(err.find("Invalid API key") != std::string::npos,
          "parse: surfaces api message");
  }

  // 5. Malformed JSON -> false with a parse error.
  {
    std::string reply, err;
    CHECK(!parseChatReply("{not json", reply, err), "parse: malformed -> false");
    CHECK(err.find("parse") != std::string::npos, "parse: reports parse error");
  }

  // 6. Missing content -> false.
  {
    std::string resp = R"({"choices":[{"message":{"role":"assistant"}}]})";
    std::string reply, err;
    CHECK(!parseChatReply(resp, reply, err), "parse: missing content -> false");
  }

  // 7. Empty content -> false.
  {
    std::string resp = R"({"choices":[{"message":{"content":""}}]})";
    std::string reply, err;
    CHECK(!parseChatReply(resp, reply, err), "parse: empty content -> false");
  }

  if (g_failures == 0) {
    std::printf("ok - all tests passed\n");
    return 0;
  }
  std::printf("FAILED - %d check(s) failed\n", g_failures);
  return 1;
}
