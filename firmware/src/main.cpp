// Pyramid — M5Stack Voice AI Chatbot (AtomS3R + Atomic Echo Base).
//
// v2.1: the device is a thin **WSS client** of our own server. It boots, brings
// up Wi-Fi, opens a duplex WebSocket to the server, and runs the I/O loop:
// push-to-talk speech is streamed up as binary `audio` frames; typed lines over
// USB-CDC serial map to `text_in`; the server runs ASR→LLM→TTS and streams the
// reply back, and each `tts_audio` chunk is played as it arrives (early
// playback). The device stays thin — no ASR/LLM/TTS or history on-device.
//
// Layout: pure logic in *_api.h / states.h / vad.h / audio.h / timing.h /
// ws_protocol.h; glue split into modules —
//   app_state  shared globals + tuning constants
//   log        status logging (app::logf)
//   ui         turn-state machine + LCD (and the optional transcript)
//   net        Wi-Fi bring-up + reconnect supervisor
//   audio_io   Echo Base mic streaming + early speaker playback
//   ws_client  the WSS transport
//   turn       WS event handlers + the input paths
// This file is just setup()/loop().
//
// Build/flash (PlatformIO): from firmware/ run
//   pio run                 # compile
//   pio run -t upload       # flash the AtomS3R
//   pio device monitor      # serial @115200
// Copy src/config.example.h -> src/config.h first and fill in Wi-Fi + server.

#include <Arduino.h>
#include <M5Unified.h>

#include <string>

#include "app_state.h"
#include "net.h"
#include "serial_protocol.h"
#include "states.h"
#include "turn.h"
#include "ui.h"
#include "ws_client.h"

using namespace app;

void setup() {
  auto cfg = M5.config();
  // Atomic Echo Base: ES8311 codec drives both the mic and the speaker.
  cfg.external_speaker.atomic_echo = true;
  M5.begin(cfg);
  M5.Display.setTextSize(2);
  // Start in mic mode on the shared ES8311 bus (audio_io flips to speaker for
  // playback and back).
  M5.Speaker.end();
  M5.Mic.begin();

  Serial.begin(kSerialBaud);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) delay(10);  // let USB-CDC enumerate

  Serial.println("Pyramid v2.1 -- server proxy (WSS client)");
  setState(pyramid::TurnState::Offline);  // boot: not connected yet

  connectWiFi();
  wsBegin();  // start connecting to our server (hello sent on connect)

  Serial.println("ready. hold BtnA to talk, or type a line:");
}

void loop() {
  M5.update();
  serviceWiFi();
  wsLoop();  // service the WSS socket; inbound frames dispatch via turn.cpp

  // Resilience: never get stuck on Error — auto-return to Idle once the failure
  // has been visible for kErrorDwellMs.
  if (g_state == pyramid::TurnState::Error &&
      millis() - g_errorSinceMs > kErrorDwellMs) {
    applyEvent(pyramid::TurnEvent::Done);  // Error -> Idle
  }

  // Serial bridge: a typed line becomes text_in for the server (local debug client).
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    std::string line;
    if (g_reader.feed(c, line)) {
      pyramid::TextIn ev;
      if (pyramid::parseTextIn(line, ev)) {
        handleTextIn(ev.text);
      }
    }
  }

  // Push-to-talk: hold BtnA to stream speech; release OR a trailing pause (VAD)
  // ends the listen window. The reply + audio arrive asynchronously over the WS.
  if (M5.BtnA.isPressed()) {
    streamVoice();
    // VAD may have ended capture while the button is still held; drain the hold
    // while keeping the socket serviced (so playback can start).
    while (M5.BtnA.isPressed()) {
      M5.update();
      wsLoop();
      delay(5);
    }
  }

  delay(5);
}
