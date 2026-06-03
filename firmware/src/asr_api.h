#pragma once

// Pyramid v1.3 — ASR response parsing (Deepgram prerecorded).
//
// Pure, Arduino-free parse of a Deepgram `/v1/listen` JSON response. The device
// POSTs raw 16 kHz mono PCM16 to
//   https://api.deepgram.com/v1/listen?model=nova-2&language=uk&encoding=linear16&sample_rate=16000
// with `Authorization: Token <key>` (see asrTranscribe() in main.cpp); the
// transcript is at results.channels[0].alternatives[0].transcript (+ confidence).
// Host-tested (test/test_asr/). Requires ArduinoJson v7.

#include <string>

#include <ArduinoJson.h>

namespace pyramid {

// Parse a Deepgram response. On a non-empty transcript, sets `transcript` +
// `confidence` (0..1) and returns true. On malformed JSON, an API-error object,
// a missing transcript, or an empty transcript, sets `err` and returns false
// (the caller re-prompts on empty/low-confidence — PYR-013).
inline bool parseAsrTranscript(const std::string& body, std::string& transcript,
                               float& confidence, std::string& err) {
  JsonDocument doc;
  DeserializationError jerr = deserializeJson(doc, body);
  if (jerr) {
    err = std::string("json parse error: ") + jerr.c_str();
    return false;
  }
  // Deepgram errors come back as {"err_code":..,"err_msg":".."}.
  if (doc["err_msg"].is<const char*>()) {
    err = std::string("api error: ") + (doc["err_msg"] | "unknown");
    return false;
  }

  JsonVariantConst alt = doc["results"]["channels"][0]["alternatives"][0];
  if (!alt["transcript"].is<const char*>()) {
    err = "no transcript in response";
    return false;
  }
  transcript = alt["transcript"].as<const char*>();
  confidence = alt["confidence"] | 0.0f;
  if (transcript.empty()) {
    err = "empty transcript";
    return false;
  }
  return true;
}

}  // namespace pyramid
