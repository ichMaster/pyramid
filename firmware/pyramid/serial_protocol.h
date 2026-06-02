#pragma once

// Pyramid v0.1 — serial text protocol.
//
// In v0 the USB-CDC port is a plain-text channel (ARCHITECTURE §Protocols):
// one inbound line maps to the device->server `text_in{text}` event, and the
// device writes a reply line back. v0.1 has no AI yet, so the "reply" is an
// echo; v0.2 (PYR-002) replaces formatReply() with the LLM reply while the
// text_in mapping below stays put.
//
// Pure, Arduino-free logic so it is host-testable alongside line_reader.h.

#include <string>

namespace pyramid {

// An inbound serial line mapped to the v0 `text_in` event.
struct TextIn {
  std::string text;
};

// Map a completed serial line to a text_in event. Surrounding whitespace is
// trimmed; an empty / whitespace-only line yields false (nothing to dispatch).
inline bool parseTextIn(const std::string& line, TextIn& out) {
  const char* ws = " \t\r\n";
  std::size_t b = line.find_first_not_of(ws);
  if (b == std::string::npos) return false;
  std::size_t e = line.find_last_not_of(ws);
  out.text = line.substr(b, e - b + 1);
  return !out.text.empty();
}

// v0.1 reply: echo the inbound text so the serial round-trip is visible.
// The "echo: " prefix keeps it distinct from logf() status lines. Replaced by
// the LLM reply in v0.2.
inline std::string formatReply(const TextIn& in) {
  return "echo: " + in.text;
}

}  // namespace pyramid
