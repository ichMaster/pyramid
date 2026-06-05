#include "turn.h"

#include <Arduino.h>
#include <WiFi.h>

#include "app_state.h"
#include "audio_io.h"   // ensureMicMode, streamCapture, beginSpeaker, playPcmChunk, endSpeaker
#include "log.h"
#include "ui.h"
#include "ws_client.h"  // wsConnected, wsSendText, wsSendBinary
#include "ws_protocol.h"

namespace app {

using namespace pyramid;

// Are we currently streaming tts_audio to the speaker? (set on the first chunk,
// cleared on tts_end / abort) — gates the one-time mic→speaker switch.
static bool s_speaking = false;

void failTurn(const char* what) {
  Serial.print("error: ");
  Serial.println(what);
  ensureMicMode();
  s_speaking = false;
  if (WiFi.status() != WL_CONNECTED || !wsConnected()) {
    g_offline = true;
    applyEvent(TurnEvent::WifiLost);  // -> Offline; serviceWiFi / ws reconnect
  } else {
    applyEvent(TurnEvent::Fail);  // -> Error -> (dwell) -> Idle
  }
}

void onWsConnected() {
  logf("ws: connected to server");
  wsSendText(buildHello(DEVICE_TOKEN, PROTO_VER, AUDIO_FMT));
  g_offline = false;
  applyEvent(TurnEvent::WifiUp);  // Offline -> Idle
}

void onWsDisconnected() {
  logf("ws: disconnected from server");
  s_speaking = false;
  ensureMicMode();
  g_offline = true;
  applyEvent(TurnEvent::WifiLost);  // -> Offline (input paused)
}

static void recordAnswer() {
  if (g_answerStartMs == 0) return;
  g_lastAnswerMs = g_stamps.speakMs - g_answerStartMs;  // request-ready -> first audio
  g_answerCount++;
  g_answerSumMs += g_lastAnswerMs;
  const uint32_t avg = static_cast<uint32_t>(g_answerSumMs / g_answerCount);
  Serial.printf("[answer] last %u.%us  avg %u.%us  (n=%u)\n", g_lastAnswerMs / 1000,
                (g_lastAnswerMs % 1000) / 100, avg / 1000, (avg % 1000) / 100,
                static_cast<unsigned>(g_answerCount));
}

void onTtsAudio(const uint8_t* data, size_t len) {
  if (!s_speaking) {
    s_speaking = true;
    beginSpeaker();                  // mic -> speaker, once, on the first chunk
    applyEvent(TurnEvent::Reply);    // -> Replying
    g_stamps.speakMs = millis();     // the moment the user first hears audio
    if (g_voiceActive) {
      const uint32_t total = g_stamps.speakMs - g_stamps.pressMs;
      const uint32_t speech =
          (g_stamps.recEndMs > g_stamps.pressMs) ? g_stamps.recEndMs - g_stamps.pressMs : 0;
      Serial.printf("[latency] press->speak=%lu ms (speech=%lu ms; server=%lu ms)\n",
                    static_cast<unsigned long>(total), static_cast<unsigned long>(speech),
                    static_cast<unsigned long>(total - speech));
      g_voiceActive = false;
    }
    recordAnswer();
  }
  playPcmChunk(data, len);           // early/streaming playback
}

void onWsText(const std::string& json) {
  Inbound in;
  if (!parseInbound(json, in)) return;
  switch (in.type) {
    case InType::AsrPartial:
      g_userText = in.text;  // interim transcript
      break;
    case InType::Asr:
      g_userText = in.text;  // final transcript
      g_replyText.clear();
#if SHOW_TRANSCRIPT
      renderState();
#endif
      break;
    case InType::Reply:
      if (in.delta) g_replyText += in.text;  // streamed token chunk
#if SHOW_TRANSCRIPT
      renderState();
#endif
      break;
    case InType::TextOut:
      g_replyText = in.text;
#if SHOW_TRANSCRIPT
      renderState();
#endif
      break;
    case InType::TtsEnd:
      if (s_speaking) {
        endSpeaker();  // drain + speaker -> mic
        s_speaking = false;
      }
      applyEvent(TurnEvent::Done);  // -> Idle
      break;
    case InType::Error:
      Serial.printf("[server error] %s: %s\n", in.code.c_str(), in.msg.c_str());
      failTurn(in.code.empty() ? "server error" : in.code.c_str());
      break;
    case InType::Restart:
      logf("ws: restart requested");
      delay(100);
      ESP.restart();  // restart -> boot (ARCHITECTURE §Error handling)
      break;
    case InType::ConfigUpdated:
      logf("ws: config_updated (Role reload arrives in v2.2)");
      break;
    case InType::Pong:
    case InType::Unknown:
    default:
      break;
  }
}

void streamVoice() {
  if (!wsConnected()) {
    Serial.println("offline: server not connected");
    return;
  }
  g_stamps = VoiceStamps{};
  g_voiceActive = true;
  g_stamps.pressMs = millis();
  applyEvent(TurnEvent::Listen);  // -> Listening
  wsSendText(buildListenStart());
  streamCapture();  // captures + streams audio chunks; VAD/release ends it
  g_stamps.recEndMs = millis();
  wsSendText(buildListenStop());
  g_answerStartMs = g_stamps.recEndMs;  // answer clock: release -> first audio
  applyEvent(TurnEvent::Think);  // -> Thinking (asr/reply/audio arrive async)
}

void handleTextIn(const std::string& text) {
  if (!wsConnected()) {
    Serial.println("offline: server not connected");
    return;
  }
  logf("text_in: \"%s\"", text.c_str());
  g_voiceActive = false;
  g_userText = text;
  g_replyText.clear();
  g_answerStartMs = millis();
  applyEvent(TurnEvent::Think);  // -> Thinking
  wsSendText(buildTextIn(text));
}

}  // namespace app
