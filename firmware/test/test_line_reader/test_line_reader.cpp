// Host unit test for the pure v0.1 serial logic (line_reader.h +
// serial_protocol.h). No Arduino dependency, so it builds and runs on any
// C++17 host compiler:
//
//   c++ -std=c++17 -I../pyramid test_line_reader.cpp -o /tmp/test_line_reader \
//     && /tmp/test_line_reader
//
// In v1 this folds into PlatformIO's native test env (pio test -e native);
// for now it gives real host coverage of the line reader / text_in parser
// that PYR-001 leaves on-device behavior (Wi-Fi, LCD) to a manual DoD check.

#include <unity.h>
#include <cstdio>
#include <string>

#include "line_reader.h"
#include "serial_protocol.h"

using pyramid::LineReader;
using pyramid::TextIn;


// Feed an entire string; return the LAST completed line (and how many lines
// completed) so multi-line cases are checkable.
static int feedAll(LineReader& r, const std::string& s, std::string& last) {
  int n = 0;
  std::string out;
  for (char c : s) {
    if (r.feed(c, out)) {
      last = out;
      ++n;
    }
  }
  return n;
}

#define CHECK(cond, msg) TEST_ASSERT_TRUE_MESSAGE((cond), (msg))

void setUp(void) {}
void tearDown(void) {}

void test_all(void) {
  // 1. A newline-terminated line emits exactly that line.
  {
    LineReader r;
    std::string last;
    CHECK(feedAll(r, "hello\n", last) == 1, "single line: one emission");
    CHECK(last == "hello", "single line: content");
    CHECK(r.pending() == 0, "single line: buffer drained");
  }

  // 2. No newline yet -> no emission, bytes pending.
  {
    LineReader r;
    std::string last = "untouched";
    CHECK(feedAll(r, "partial", last) == 0, "no newline: no emission");
    CHECK(r.pending() == 7, "no newline: 7 bytes pending");
  }

  // 3. CRLF: the trailing carriage return is stripped.
  {
    LineReader r;
    std::string last;
    CHECK(feedAll(r, "windows\r\n", last) == 1, "crlf: one emission");
    CHECK(last == "windows", "crlf: CR stripped");
  }

  // 4. Two lines in one feed -> two emissions; empty line in between is empty.
  {
    LineReader r;
    std::string last;
    CHECK(feedAll(r, "a\nb\n", last) == 2, "two lines: two emissions");
    CHECK(last == "b", "two lines: last is b");
  }

  // 5. An empty line emits an empty string (reader is content-agnostic).
  {
    LineReader r;
    std::string last = "x";
    CHECK(feedAll(r, "\n", last) == 1, "empty line: one emission");
    CHECK(last.empty(), "empty line: empty content");
  }

  // 6. Bounded buffer: overflow drops excess but still emits on newline.
  {
    LineReader r(4);  // tiny cap
    std::string last;
    CHECK(feedAll(r, "abcdef\n", last) == 1, "overflow: still emits");
    CHECK(last == "abcd", "overflow: truncated to cap");
    CHECK(r.pending() == 0, "overflow: buffer drained");
  }

  // 7. reset() discards a partial line.
  {
    LineReader r;
    std::string out;
    r.feed('h', out);
    r.feed('i', out);
    CHECK(r.pending() == 2, "reset: bytes buffered before reset");
    r.reset();
    CHECK(r.pending() == 0, "reset: buffer cleared");
  }

  // 8. parseTextIn trims surrounding whitespace.
  {
    TextIn ev;
    CHECK(pyramid::parseTextIn("  hi there \t", ev), "parse: non-empty true");
    CHECK(ev.text == "hi there", "parse: trimmed content");
  }

  // 9. parseTextIn rejects empty / whitespace-only lines.
  {
    TextIn ev;
    CHECK(!pyramid::parseTextIn("", ev), "parse: empty -> false");
    CHECK(!pyramid::parseTextIn("   \t\r", ev), "parse: blank -> false");
  }

}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all);
  return UNITY_END();
}
