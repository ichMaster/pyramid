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
