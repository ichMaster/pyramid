#pragma once

// Pyramid v0.3 — short in-session conversation history.
//
// Holds the last N committed turns (alternating user / assistant) so each LLM
// request carries context. Pure, Arduino-free logic so it is host-testable
// (see ../test/test_history.cpp). History is RAM-only and per-session; nothing
// is persisted on-device (intelligence stays off-device — long-term memory is
// a v3 server concern, ARCHITECTURE §Sessions and history).
//
// Windowing keeps at most `maxTurns` turns and guarantees the kept window
// starts with a `user` turn, which the Messages API requires (the first
// message must be from the user, and roles alternate).

#include <cstddef>
#include <string>
#include <vector>

namespace pyramid {

// One chat message. `role` is "user" or "assistant"; `content` is its text.
struct Turn {
  std::string role;
  std::string content;
};

class History {
 public:
  explicit History(std::size_t maxTurns = 8) : maxTurns_(maxTurns) {}

  void addUser(const std::string& text) { add("user", text); }
  void addAssistant(const std::string& text) { add("assistant", text); }

  // The committed turns, already windowed (starts with a user turn).
  const std::vector<Turn>& turns() const { return turns_; }

  void clear() { turns_.clear(); }
  std::size_t size() const { return turns_.size(); }

 private:
  void add(const char* role, const std::string& content) {
    turns_.push_back(Turn{role, content});
    trim();
  }

  // Keep at most maxTurns_, then drop any leading non-user turn so the window
  // always begins with a user message.
  void trim() {
    while (turns_.size() > maxTurns_) turns_.erase(turns_.begin());
    while (!turns_.empty() && turns_.front().role != "user") {
      turns_.erase(turns_.begin());
    }
  }

  std::vector<Turn> turns_;
  std::size_t maxTurns_;
};

}  // namespace pyramid
