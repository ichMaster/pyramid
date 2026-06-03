// Host unit test for the pure streaming decode (sse.h): HTTP chunked decoding,
// SSE `data:` extraction, and event-JSON parsing.
//
//   c++ -std=c++17 -I../pyramid -I<ArduinoJson>/src \
//     test_sse.cpp -o /tmp/test_sse && /tmp/test_sse

#include <unity.h>
#include <cstdio>
#include <string>

#include <ArduinoJson.h>

#include "line_reader.h"
#include "sse.h"

using pyramid::Dechunker;
using pyramid::extractSseData;
using pyramid::parseStreamEvent;
using pyramid::StreamEvent;


static std::string dechunkAll(const std::string& in, std::size_t step) {
  Dechunker d;
  std::string out;
  for (std::size_t i = 0; i < in.size(); i += step) {
    const std::size_t n = (i + step <= in.size()) ? step : in.size() - i;
    d.feed(in.data() + i, n, out);
  }
  return out;
}

#define CHECK(cond, msg) TEST_ASSERT_TRUE_MESSAGE((cond), (msg))

void setUp(void) {}
void tearDown(void) {}

void test_all(void) {
  // --- Dechunker --------------------------------------------------------
  // 1. Two chunks then terminator decode to the concatenated payload.
  {
    const std::string in = "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n";
    CHECK(dechunkAll(in, in.size()) == "hello world", "dechunk: whole");
  }

  // 2. Same input fed one byte at a time (split mid-line) -> same result.
  {
    const std::string in = "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n";
    CHECK(dechunkAll(in, 1) == "hello world", "dechunk: byte-by-byte");
  }

  // 3. Hex size > 9 and a chunk extension are handled.
  {
    const std::string in = "a;foo=bar\r\n0123456789\r\n0\r\n\r\n";
    CHECK(dechunkAll(in, 3) == "0123456789", "dechunk: hex + extension");
  }

  // --- extractSseData ---------------------------------------------------
  {
    std::string out;
    CHECK(extractSseData("data: {\"a\":1}", out) && out == "{\"a\":1}",
          "sse-data: with space");
    CHECK(extractSseData("data:{\"a\":1}", out) && out == "{\"a\":1}",
          "sse-data: no space");
    CHECK(!extractSseData("event: message_stop", out), "sse-data: event line");
    CHECK(!extractSseData("", out), "sse-data: blank line");
    CHECK(!extractSseData(": comment", out), "sse-data: comment line");
  }

  // --- parseStreamEvent -------------------------------------------------
  // message_start carries input token usage.
  {
    StreamEvent ev;
    const std::string j =
        R"({"type":"message_start","message":{"usage":{"input_tokens":25,"output_tokens":1}}})";
    CHECK(parseStreamEvent(j, ev), "event: message_start parsed");
    CHECK(ev.kind == StreamEvent::kMessageStart, "event: kind message_start");
    CHECK(ev.inputTokens == 25, "event: input tokens");
  }

  // a text delta yields the token text.
  {
    StreamEvent ev;
    const std::string j =
        R"({"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Привіт"}})";
    CHECK(parseStreamEvent(j, ev), "event: text delta parsed");
    CHECK(ev.kind == StreamEvent::kTextDelta, "event: kind text delta");
    CHECK(ev.text == "Привіт", "event: delta text (UTF-8)");
  }

  // a non-text delta (e.g. input_json_delta) is ignored.
  {
    StreamEvent ev;
    const std::string j =
        R"({"type":"content_block_delta","delta":{"type":"input_json_delta","partial_json":"{"}})";
    CHECK(!parseStreamEvent(j, ev), "event: non-text delta ignored");
  }

  // message_delta carries the cumulative output token count.
  {
    StreamEvent ev;
    const std::string j =
        R"({"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":42}})";
    CHECK(parseStreamEvent(j, ev), "event: message_delta parsed");
    CHECK(ev.kind == StreamEvent::kMessageDelta, "event: kind message_delta");
    CHECK(ev.outputTokens == 42, "event: output tokens");
  }

  // message_stop and error.
  {
    StreamEvent ev;
    CHECK(parseStreamEvent(R"({"type":"message_stop"})", ev) &&
              ev.kind == StreamEvent::kMessageStop,
          "event: message_stop");
    StreamEvent ev2;
    CHECK(parseStreamEvent(
              R"({"type":"error","error":{"type":"overloaded_error","message":"overloaded"}})",
              ev2) &&
              ev2.kind == StreamEvent::kError &&
              ev2.error.find("overloaded") != std::string::npos,
          "event: error surfaces message");
  }

  // ping / unparseable are ignored.
  {
    StreamEvent ev;
    CHECK(!parseStreamEvent(R"({"type":"ping"})", ev), "event: ping ignored");
    CHECK(!parseStreamEvent("{bad json", ev), "event: bad json ignored");
  }

  // --- End-to-end: chunked SSE bytes -> decoded text + tokens -----------
  {
    // A realistic mini-stream, chunk-framed, with a data line split across a
    // chunk boundary ("Привіт" delta split into two chunks).
    const std::string sse =
        "event: message_start\n"
        R"(data: {"type":"message_start","message":{"usage":{"input_tokens":10,"output_tokens":1}}})"
        "\n\n"
        "event: content_block_delta\n"
        R"(data: {"type":"content_block_delta","delta":{"type":"text_delta","text":"Hi "}})"
        "\n\n"
        "event: content_block_delta\n"
        R"(data: {"type":"content_block_delta","delta":{"type":"text_delta","text":"there"}})"
        "\n\n"
        "event: message_delta\n"
        R"(data: {"type":"message_delta","delta":{},"usage":{"output_tokens":5}})"
        "\n\n"
        "event: message_stop\n"
        R"(data: {"type":"message_stop"})"
        "\n\n";

    // Wrap the whole SSE text in one chunk + terminator, fed 7 bytes at a time
    // so the dechunker and line assembler are exercised across boundaries.
    char sizebuf[16];
    std::snprintf(sizebuf, sizeof(sizebuf), "%x\r\n", (unsigned)sse.size());
    const std::string chunked = std::string(sizebuf) + sse + "\r\n0\r\n\r\n";

    Dechunker d;
    pyramid::LineReader lines;  // reuse the serial line reader for SSE lines
    std::string decoded, line, dataJson, reply;
    int inTok = 0, outTok = 0;
    bool stop = false;
    for (std::size_t i = 0; i < chunked.size() && !stop; i += 7) {
      const std::size_t n =
          (i + 7 <= chunked.size()) ? 7 : chunked.size() - i;
      decoded.clear();
      d.feed(chunked.data() + i, n, decoded);
      for (char ch : decoded) {
        if (!lines.feed(ch, line)) continue;
        if (!extractSseData(line, dataJson)) continue;
        StreamEvent ev;
        if (!parseStreamEvent(dataJson, ev)) continue;
        if (ev.kind == StreamEvent::kMessageStart) inTok = ev.inputTokens;
        else if (ev.kind == StreamEvent::kTextDelta) reply += ev.text;
        else if (ev.kind == StreamEvent::kMessageDelta) outTok = ev.outputTokens;
        else if (ev.kind == StreamEvent::kMessageStop) stop = true;
      }
    }
    CHECK(reply == "Hi there", "e2e: reassembled reply");
    CHECK(inTok == 10, "e2e: input tokens");
    CHECK(outTok == 5, "e2e: output tokens");
    CHECK(stop, "e2e: saw message_stop");
  }

}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all);
  return UNITY_END();
}
