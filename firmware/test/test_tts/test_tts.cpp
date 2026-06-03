// Host unit test for the pure v1.2 TTS request builder (tts_api.h).
// Run via `pio test -e native`.

#include <unity.h>
#include <string>

#include <ArduinoJson.h>

#include "tts_api.h"

using pyramid::buildTtsRequest;
using pyramid::clampUtf8;

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

  // clampUtf8: no truncation when within budget.
  CHECK(clampUtf8("hello", 10) == "hello", "clamp: under budget unchanged");
  CHECK(clampUtf8("", 5) == "", "clamp: empty");

  // ASCII truncation is exact.
  CHECK(clampUtf8("hello", 3) == "hel", "clamp: ascii cut");

  // Ukrainian (2-byte Cyrillic): "Привіт" = 12 bytes (6 chars × 2).
  {
    const std::string s = "Привіт";
    CHECK(s.size() == 12, "clamp: Привіт is 12 bytes");
    // cut at 5 lands mid-char (byte 5 = 2nd byte of 'и') → back off to 4 = "Пр".
    CHECK(clampUtf8(s, 5) == "Пр", "clamp: mid-char backs off to boundary");
    // cut at 4 is already a boundary → "Пр".
    CHECK(clampUtf8(s, 4) == "Пр", "clamp: on-boundary cut");
    // result is whole chars only (even byte length for 2-byte Cyrillic).
    CHECK(clampUtf8(s, 7).size() % 2 == 0, "clamp: never splits a char");
    CHECK(clampUtf8(s, 100) == s, "clamp: over budget unchanged");
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all);
  return UNITY_END();
}
