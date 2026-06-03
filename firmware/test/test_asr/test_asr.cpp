// Host unit test for the pure v1.3 ASR response parsing (asr_api.h),
// against recorded Deepgram-shaped responses. Run via `pio test -e native`.

#include <unity.h>
#include <string>

#include <ArduinoJson.h>

#include "asr_api.h"

using pyramid::parseAsrTranscript;

#define CHECK(cond, msg) TEST_ASSERT_TRUE_MESSAGE((cond), (msg))

void setUp(void) {}
void tearDown(void) {}

void test_all(void) {
  // Success: transcript + confidence extracted (UTF-8 Ukrainian).
  {
    const std::string resp =
        R"({"results":{"channels":[{"alternatives":[)"
        R"({"transcript":"привіт світ","confidence":0.987}]}]}})";
    std::string t, err;
    float c = -1.0f;
    CHECK(parseAsrTranscript(resp, t, c, err), "asr: success true");
    CHECK(t == "привіт світ", "asr: transcript (UTF-8)");
    CHECK(c > 0.98f && c < 0.99f, "asr: confidence");
    CHECK(err.empty(), "asr: no error on success");
  }

  // Empty transcript (silence) -> false with a readable error.
  {
    const std::string resp =
        R"({"results":{"channels":[{"alternatives":[{"transcript":"","confidence":0.0}]}]}})";
    std::string t, err;
    float c = 0.0f;
    CHECK(!parseAsrTranscript(resp, t, c, err), "asr: empty -> false");
    CHECK(err.find("empty") != std::string::npos, "asr: empty error msg");
  }

  // Missing transcript field -> false.
  {
    const std::string resp = R"({"results":{"channels":[{"alternatives":[{}]}]}})";
    std::string t, err;
    float c = 0.0f;
    CHECK(!parseAsrTranscript(resp, t, c, err), "asr: missing transcript -> false");
  }

  // API error object -> false with the message.
  {
    const std::string resp =
        R"({"err_code":"INVALID_AUTH","err_msg":"Invalid credentials"})";
    std::string t, err;
    float c = 0.0f;
    CHECK(!parseAsrTranscript(resp, t, c, err), "asr: api error -> false");
    CHECK(err.find("Invalid credentials") != std::string::npos, "asr: surfaces api msg");
  }

  // Malformed JSON -> false with a parse error.
  {
    std::string t, err;
    float c = 0.0f;
    CHECK(!parseAsrTranscript("{not json", t, c, err), "asr: malformed -> false");
    CHECK(err.find("parse") != std::string::npos, "asr: reports parse error");
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all);
  return UNITY_END();
}
