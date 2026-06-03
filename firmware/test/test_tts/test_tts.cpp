// Host unit test for the pure v1.2 TTS request builder (tts_api.h).
// Run via `pio test -e native`.

#include <unity.h>
#include <string>

#include <ArduinoJson.h>

#include "tts_api.h"

using pyramid::buildTtsRequest;

#define CHECK(cond, msg) TEST_ASSERT_TRUE_MESSAGE((cond), (msg))

void setUp(void) {}
void tearDown(void) {}

void test_all(void) {
  // Valid JSON with model_id + text.
  {
    std::string body = buildTtsRequest("eleven_multilingual_v2", "Привіт, світе");
    JsonDocument doc;
    CHECK(!deserializeJson(doc, body), "build: valid JSON");
    CHECK(doc["model_id"] == "eleven_multilingual_v2", "build: model_id");
    CHECK(doc["text"] == "Привіт, світе", "build: UTF-8 text preserved");
  }

  // Quotes / newlines are escaped (still valid JSON, content preserved).
  {
    std::string body = buildTtsRequest("m", "він сказав \"так\"\nі пішов");
    JsonDocument doc;
    CHECK(!deserializeJson(doc, body), "build: escaped JSON valid");
    CHECK(doc["text"] == "він сказав \"так\"\nі пішов", "build: escaping");
  }

  // Empty text is still valid JSON (caller decides whether to send).
  {
    std::string body = buildTtsRequest("m", "");
    JsonDocument doc;
    CHECK(!deserializeJson(doc, body), "build: empty text valid JSON");
    CHECK(doc["text"] == "", "build: empty text field");
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all);
  return UNITY_END();
}
