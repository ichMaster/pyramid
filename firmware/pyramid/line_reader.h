#pragma once

// Pyramid v0.1 — non-blocking serial line reader.
//
// Pure, Arduino-free logic: feed it bytes as they arrive from the USB-CDC
// port and it buffers until a newline, then emits exactly one complete line
// (with any trailing CR stripped, so CRLF terminals behave). The buffer is
// bounded — once full, extra bytes are dropped until the next newline so a
// runaway sender can never exhaust RAM.
//
// Kept free of any Arduino/M5 dependency so it is host-testable today
// (see ../test/test_line_reader.cpp) and folds cleanly into PlatformIO's
// native test env in v1.

#include <cstddef>
#include <string>

namespace pyramid {

class LineReader {
 public:
  explicit LineReader(std::size_t max_len = 1024) : max_len_(max_len) {}

  // Feed a single byte. Returns true when a complete line is ready, placing
  // it (without the trailing newline / carriage return) into `out`.
  bool feed(char c, std::string& out) {
    if (c == '\n') {
      if (!buf_.empty() && buf_.back() == '\r') buf_.pop_back();
      out.assign(buf_);
      buf_.clear();
      return true;
    }
    if (buf_.size() < max_len_) {
      buf_.push_back(c);
    }
    // else: buffer full — drop bytes silently until the next newline.
    return false;
  }

  // Discard any partially-buffered line (e.g. on reconnect / restart).
  void reset() { buf_.clear(); }

  // Bytes buffered for the line currently being assembled.
  std::size_t pending() const { return buf_.size(); }

 private:
  std::string buf_;
  std::size_t max_len_;
};

}  // namespace pyramid
