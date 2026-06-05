// Host unit test for the firmware WS codec (ws_protocol.h) + the serial →
// text_in bridge (serial_protocol.h). Pure C++17 + ArduinoJson, no board:
//
//   pio test -e native
//
// Covers the device side of the v2.1 device↔server contract — it must stay in
// lockstep with the server's protocol.py (and its contract tests).

#include <unity.h>

#include <string>

#include "serial_protocol.h"
#include "ws_protocol.h"

using namespace pyramid;

void setUp(void) {}
void tearDown(void) {}

// --- outbound: build then re-parse the JSON to check the shape --------------
static JsonDocument reparse(const std::string& json) {
  JsonDocument d;
  deserializeJson(d, json);
  return d;
}

void test_build_hello() {
  auto d = reparse(buildHello("dev-tok", "1.0", "pcm16"));
  TEST_ASSERT_EQUAL_STRING("hello", d["type"]);
  TEST_ASSERT_EQUAL_STRING("dev-tok", d["device_token"]);
  TEST_ASSERT_EQUAL_STRING("1.0", d["proto_ver"]);
  TEST_ASSERT_EQUAL_STRING("pcm16", d["audio_fmt"]);
}

void test_build_listen_and_ping() {
  TEST_ASSERT_EQUAL_STRING("listen_start", reparse(buildListenStart())["type"]);
  TEST_ASSERT_EQUAL_STRING("listen_stop", reparse(buildListenStop())["type"]);
  TEST_ASSERT_EQUAL_STRING("ping", reparse(buildPing())["type"]);
}

void test_build_text_in_roundtrips_unicode() {
  auto d = reparse(buildTextIn("привіт, світ"));
  TEST_ASSERT_EQUAL_STRING("text_in", d["type"]);
  TEST_ASSERT_EQUAL_STRING("привіт, світ", d["text"]);
}

// --- inbound parsing --------------------------------------------------------
void test_parse_asr_partial_and_final() {
  Inbound in;
  TEST_ASSERT_TRUE(parseInbound("{\"type\":\"asr_partial\",\"text\":\"як\"}", in));
  TEST_ASSERT_TRUE(in.type == InType::AsrPartial);
  TEST_ASSERT_EQUAL_STRING("як", in.text.c_str());

  TEST_ASSERT_TRUE(parseInbound("{\"type\":\"asr\",\"text\":\"як справи\"}", in));
  TEST_ASSERT_TRUE(in.type == InType::Asr);
  TEST_ASSERT_EQUAL_STRING("як справи", in.text.c_str());
}

void test_parse_reply_delta_and_done() {
  Inbound in;
  TEST_ASSERT_TRUE(parseInbound(
      "{\"type\":\"reply\",\"text\":\"Доб\",\"delta\":true,\"done\":false}", in));
  TEST_ASSERT_TRUE(in.type == InType::Reply);
  TEST_ASSERT_TRUE(in.delta);
  TEST_ASSERT_FALSE(in.done);
  TEST_ASSERT_EQUAL_STRING("Доб", in.text.c_str());

  TEST_ASSERT_TRUE(parseInbound(
      "{\"type\":\"reply\",\"text\":\"\",\"delta\":true,\"done\":true}", in));
  TEST_ASSERT_TRUE(in.done);
}

void test_parse_text_out_and_tts_end() {
  Inbound in;
  TEST_ASSERT_TRUE(parseInbound("{\"type\":\"text_out\",\"text\":\"ок\"}", in));
  TEST_ASSERT_TRUE(in.type == InType::TextOut);
  TEST_ASSERT_EQUAL_STRING("ок", in.text.c_str());

  TEST_ASSERT_TRUE(parseInbound("{\"type\":\"tts_end\"}", in));
  TEST_ASSERT_TRUE(in.type == InType::TtsEnd);
}

void test_parse_error_with_code() {
  Inbound in;
  TEST_ASSERT_TRUE(parseInbound(
      "{\"type\":\"error\",\"code\":\"llm_timeout\",\"msg\":\"boom\"}", in));
  TEST_ASSERT_TRUE(in.type == InType::Error);
  TEST_ASSERT_EQUAL_STRING(wserr::kLlmTimeout, in.code.c_str());
  TEST_ASSERT_EQUAL_STRING("boom", in.msg.c_str());
}

void test_parse_control_frames() {
  Inbound in;
  TEST_ASSERT_TRUE(parseInbound("{\"type\":\"config_updated\"}", in));
  TEST_ASSERT_TRUE(in.type == InType::ConfigUpdated);
  TEST_ASSERT_TRUE(parseInbound("{\"type\":\"restart\"}", in));
  TEST_ASSERT_TRUE(in.type == InType::Restart);
  TEST_ASSERT_TRUE(parseInbound("{\"type\":\"pong\"}", in));
  TEST_ASSERT_TRUE(in.type == InType::Pong);
}

void test_parse_rejects_malformed() {
  Inbound in;
  TEST_ASSERT_FALSE(parseInbound("not json", in));
  TEST_ASSERT_FALSE(parseInbound("{\"no_type\":1}", in));
  TEST_ASSERT_FALSE(parseInbound("{\"type\":\"bogus\"}", in));
}

// --- serial → text_in bridge (the local debug client) ----------------------
void test_serial_line_maps_to_text_in() {
  TextIn ev;
  TEST_ASSERT_TRUE(parseTextIn("  привіт  ", ev));
  TEST_ASSERT_EQUAL_STRING("привіт", ev.text.c_str());
  // The bridged text becomes a text_in frame for the server.
  auto d = reparse(buildTextIn(ev.text));
  TEST_ASSERT_EQUAL_STRING("text_in", d["type"]);
  TEST_ASSERT_EQUAL_STRING("привіт", d["text"]);

  TEST_ASSERT_FALSE(parseTextIn("   \t\r\n", ev));  // blank line: nothing to send
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_build_hello);
  RUN_TEST(test_build_listen_and_ping);
  RUN_TEST(test_build_text_in_roundtrips_unicode);
  RUN_TEST(test_parse_asr_partial_and_final);
  RUN_TEST(test_parse_reply_delta_and_done);
  RUN_TEST(test_parse_text_out_and_tts_end);
  RUN_TEST(test_parse_error_with_code);
  RUN_TEST(test_parse_control_frames);
  RUN_TEST(test_parse_rejects_malformed);
  RUN_TEST(test_serial_line_maps_to_text_in);
  return UNITY_END();
}
