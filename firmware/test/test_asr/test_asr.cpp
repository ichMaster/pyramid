// Host unit test for the pure v1.3 ASR response parsing (asr_api.h),
// against recorded Deepgram-shaped responses. Run via `pio test -e native`.

#include <unity.h>
#include <string>

#include <ArduinoJson.h>

#include "asr_api.h"

using pyramid::parseAsrTranscript;
using pyramid::parseHost;

#define CHECK(cond, msg) TEST_ASSERT_TRUE_MESSAGE((cond), (msg))

void setUp(void) {}
void tearDown(void) {}

// Host extraction for the v1.4 ASR pre-warm: bare host, no scheme/port/path.
void test_parse_host(void) {
  CHECK(parseHost("https://api.deepgram.com/v1/listen?model=nova-2") ==
            "api.deepgram.com",
        "host from full deepgram url");
  CHECK(parseHost("https://api.deepgram.com") == "api.deepgram.com",
        "host with no path");
  CHECK(parseHost("https://example.com:8443/x") == "example.com",
        "explicit port stripped");
  CHECK(parseHost("api.deepgram.com/v1/listen") == "api.deepgram.com",
        "host without scheme");
}

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
  RUN_TEST(test_parse_host);
  return UNITY_END();
}
