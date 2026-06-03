#pragma once

// Pyramid v1.2 — TTS request shaping (ElevenLabs Messages-style JSON).
//
// Pure, Arduino-free JSON build for the ElevenLabs text-to-speech API. The
// device POSTs this to
//   https://api.elevenlabs.io/v1/text-to-speech/{voice_id}?output_format=pcm_16000
// with an `xi-api-key` header, and reads back raw 16 kHz mono PCM16 (no decode;
// see ttsFetch() in main.cpp). Kept free of Arduino/network deps so the request
// building is host-testable (test/test_tts/). UTF-8 (Ukrainian) passes through
// as raw bytes; ArduinoJson handles escaping.

#include <string>

#include <ArduinoJson.h>

namespace pyramid {

// Truncate `text` to at most `maxBytes` bytes without splitting a UTF-8
// multibyte sequence, so the result is always valid UTF-8 (Ukrainian is 2-byte
// Cyrillic). Continuation bytes are 10xxxxxx (0x80–0xBF); if the cut lands on
// one, back off to the preceding character boundary. Used to bound the reply
// sent to TTS (TTS_MAX_CHARS) so it fits the playback buffer and stays cheap.
inline std::string clampUtf8(const std::string& text, std::size_t maxBytes) {
  if (text.size() <= maxBytes) return text;
  std::size_t end = maxBytes;
  while (end > 0 &&
         (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
    --end;  // inside a multibyte char — back off to its lead byte
  }
  return text.substr(0, end);
}

// Build the ElevenLabs TTS request body:
//   {"model_id": <model>, "text": <text>}
inline std::string buildTtsRequest(const std::string& model,
                                   const std::string& text) {
  JsonDocument doc;
  doc["model_id"] = model;
  doc["text"] = text;
  std::string out;
  serializeJson(doc, out);
  return out;
}

}  // namespace pyramid
