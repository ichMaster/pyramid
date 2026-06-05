#pragma once

// Pyramid v2.1 — firmware side of the device↔server WS contract.
//
// The mirror of the server's protocol.py: build the device→server text frames
// (hello / listen_start / listen_stop / text_in / ping) and parse the
// server→device text frames (asr_partial / asr / reply / text_out / tts_end /
// error / config_updated / restart / pong). Binary frames carry raw PCM16 16 kHz
// mono with no JSON envelope — audio (device→server) and tts_audio
// (server→device) — so they are handled by the transport, not here.
//
// Pure, host-testable (ArduinoJson, no Arduino I/O) — see test/test_ws_codec.

#include <ArduinoJson.h>

#include <string>

namespace pyramid {

// Enumerated error.code set (ARCHITECTURE §Error handling), mirrored from the
// server so both ends share one vocabulary.
namespace wserr {
constexpr const char* kWifiLost = "wifi_lost";
constexpr const char* kServerUnreachable = "server_unreachable";
constexpr const char* kProtoUnsupported = "proto_unsupported";
constexpr const char* kUnauthorized = "unauthorized";
constexpr const char* kRateLimited = "rate_limited";
constexpr const char* kAsrFailed = "asr_failed";
constexpr const char* kLlmTimeout = "llm_timeout";
constexpr const char* kLlmFailed = "llm_failed";
constexpr const char* kTtsFailed = "tts_failed";
constexpr const char* kInternal = "internal";
}  // namespace wserr

// --- outbound (device → server) text frames --------------------------------
inline std::string buildHello(const std::string& deviceToken,
                              const std::string& protoVer,
                              const std::string& audioFmt) {
  JsonDocument d;
  d["type"] = "hello";
  d["device_token"] = deviceToken;
  d["proto_ver"] = protoVer;
  d["audio_fmt"] = audioFmt;
  std::string out;
  serializeJson(d, out);
  return out;
}

inline std::string buildListenStart() {
  JsonDocument d;
  d["type"] = "listen_start";
  std::string out;
  serializeJson(d, out);
  return out;
}

inline std::string buildListenStop() {
  JsonDocument d;
  d["type"] = "listen_stop";
  std::string out;
  serializeJson(d, out);
  return out;
}

inline std::string buildTextIn(const std::string& text) {
  JsonDocument d;
  d["type"] = "text_in";
  d["text"] = text;
  std::string out;
  serializeJson(d, out);
  return out;
}

inline std::string buildPing() {
  JsonDocument d;
  d["type"] = "ping";
  std::string out;
  serializeJson(d, out);
  return out;
}

// --- inbound (server → device) text frames ---------------------------------
enum class InType {
  AsrPartial,
  Asr,
  Reply,
  TextOut,
  TtsEnd,
  Error,
  ConfigUpdated,
  Restart,
  Pong,
  Unknown,
};

struct Inbound {
  InType type = InType::Unknown;
  std::string text;   // asr_partial / asr / reply / text_out
  bool delta = false;  // reply: streamed token chunk
  bool done = false;   // reply: final delta
  std::string code;   // error
  std::string msg;    // error
};

inline InType inTypeOf(const std::string& t) {
  if (t == "asr_partial") return InType::AsrPartial;
  if (t == "asr") return InType::Asr;
  if (t == "reply") return InType::Reply;
  if (t == "text_out") return InType::TextOut;
  if (t == "tts_end") return InType::TtsEnd;
  if (t == "error") return InType::Error;
  if (t == "config_updated") return InType::ConfigUpdated;
  if (t == "restart") return InType::Restart;
  if (t == "pong") return InType::Pong;
  return InType::Unknown;
}

// Parse a server→device text frame. Returns false on malformed JSON / no known
// type (so the caller can drop it).
inline bool parseInbound(const std::string& json, Inbound& out) {
  JsonDocument d;
  if (deserializeJson(d, json)) return false;
  if (!d["type"].is<const char*>()) return false;
  out = Inbound{};
  out.type = inTypeOf(std::string(d["type"].as<const char*>()));
  if (out.type == InType::Unknown) return false;
  if (d["text"].is<const char*>()) out.text = d["text"].as<const char*>();
  if (d["delta"].is<bool>()) out.delta = d["delta"].as<bool>();
  if (d["done"].is<bool>()) out.done = d["done"].as<bool>();
  if (d["code"].is<const char*>()) out.code = d["code"].as<const char*>();
  if (d["msg"].is<const char*>()) out.msg = d["msg"].as<const char*>();
  return true;
}

}  // namespace pyramid
