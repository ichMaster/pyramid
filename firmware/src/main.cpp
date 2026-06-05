// Pyramid — M5Stack Voice AI Chatbot (AtomS3R + Atomic Echo Base).
//
// Boots the board, brings up Wi-Fi from config.h, and runs the voice/text turn
// loop: push-to-talk (or pause-detected) speech -> Deepgram ASR -> Anthropic LLM
// (streamed) -> ElevenLabs TTS -> Echo Base speaker, in Ukrainian. Typed lines
// over USB-CDC serial are an equivalent text path / debug channel. The device
// stays thin — persona + history shape the request; no decisions live on-device.
//
// Layout (v1.4 refactor): the pure logic is in *_api.h / states.h / vad.h /
// audio.h / timing.h; the M5/WiFi/HTTP glue is split into modules —
//   app_state  shared globals + tuning constants
//   log        status logging (app::logf)
//   ui         turn-state machine + LCD (and the optional transcript)
//   net        Wi-Fi bring-up + reconnect supervisor
//   audio_io   Echo Base mic capture + speaker playback
//   cloud      LLM / ASR / TTS HTTPS clients
//   turn       the ASR->LLM->TTS orchestration + failure recovery
// This file is just setup()/loop().
//
// Build/flash (PlatformIO): from firmware/ run
//   pio run                 # compile
//   pio run -t upload       # flash the AtomS3R
//   pio device monitor      # serial @115200
// Copy src/config.example.h -> src/config.h first and fill in Wi-Fi + API keys.

#include <Arduino.h>
#include <M5Unified.h>

#include <string>

#include "app_state.h"
#include "audio_io.h"
#include "net.h"
#include "serial_protocol.h"
#include "states.h"
#include "timing.h"
#include "turn.h"
#include "ui.h"

using namespace app;

void setup() {
  auto cfg = M5.config();
  // Atomic Echo Base: ES8311 codec drives both the mic and the speaker.
  // M5Unified initializes it (I2C + I2S) only when this flag is set.
  cfg.external_speaker.atomic_echo = true;
  M5.begin(cfg);
  M5.Display.setTextSize(2);
  // M5.begin enables the speaker; start in mic mode on the shared ES8311 bus
  // (the record/playback switch is handled in recordWhileHeld/playbackCaptured).
  M5.Speaker.end();
  M5.Mic.begin();

  Serial.begin(kSerialBaud);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) delay(10);  // let USB-CDC enumerate

  Serial.println("Pyramid v1.4 -- states and UX");
  setState(pyramid::TurnState::Offline);  // boot: not connected yet

  connectWiFi();

  Serial.println("ready. type a line:");
}

void loop() {
  M5.update();
  serviceWiFi();

  // Resilience (v1.4): never get stuck on Error — auto-return to Idle once the
  // failure has been visible for kErrorDwellMs.
  if (g_state == pyramid::TurnState::Error &&
      millis() - g_errorSinceMs > kErrorDwellMs) {
    applyEvent(pyramid::TurnEvent::Done);  // Error -> Idle
  }

  // Non-blocking: drain whatever bytes arrived this tick through the reader.
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    std::string line;
    if (g_reader.feed(c, line)) {
      pyramid::TextIn ev;
      if (pyramid::parseTextIn(line, ev)) {
        handleTurn(ev.text);  // serial text_in → LLM → TTS → speaker
      }
    }
  }

  // Push-to-talk: hold BtnA to record; release OR a trailing pause (v1.4 VAD)
  // ends capture and speaks the answer.
  if (M5.BtnA.isPressed()) {
    g_stamps = pyramid::VoiceStamps{};  // re-arm latency tracking for this turn
    g_voiceActive = true;
    g_stamps.pressMs = millis();  // t0: button pressed
    recordWhileHeld();            // captures into g_pcm; returns on release or pause
    g_stamps.recEndMs = millis();  // end of speech (release or detected pause)
    voiceTurn();                   // ASR(g_pcm) → LLM → TTS → speaker
    // VAD may have ended capture while the button is still held; drain the hold
    // so we don't immediately start another recording.
    while (M5.BtnA.isPressed()) {
      M5.update();
      delay(5);
    }
  }

  delay(5);
}
