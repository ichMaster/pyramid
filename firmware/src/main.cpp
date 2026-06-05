// Pyramid — M5Stack Voice AI Chatbot.
// v0.3 (PYR-003): quality and UX.
//
// Boots the AtomS3R + Echo Base, brings up Wi-Fi from config.h, and turns the
// USB-CDC port into a line-oriented text channel: a typed line becomes a
// `text_in` event, is sent (with a short rolling history) to a cloud LLM over
// direct HTTPS, and the model's reply is streamed back token by token (SSE).
// The loop is hardened:
// bounded retry on transient LLM failures, non-blocking Wi-Fi reconnect with
// exponential backoff (input paused while offline), and coarse LCD states. All
// status logging goes through logf(), gated by DEBUG_SERIAL. The device stays
// thin — persona + history shape the request, but no decisions live on-device.
//
// Build/flash (PlatformIO, from v1.1): from firmware/ run
//   pio run                 # compile
//   pio run -t upload       # flash the AtomS3R
//   pio device monitor      # serial @115200
// Copy src/config.example.h -> src/config.h first and fill in Wi-Fi + LLM keys.
// (v0 was built in the Arduino IDE; the sketch pyramid.ino moved to this file.)

#include <Arduino.h>
#include <HTTPClient.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "asr_api.h"
#include "audio.h"
#include "backoff.h"
#include "chat_api.h"
#include "config.h"
#include "history.h"
#include "line_reader.h"
#include "serial_protocol.h"
#include "sse.h"
#include "states.h"
#include "timing.h"
#include "tts_api.h"
#include "ulaw.h"
#include "vad.h"

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kWifiTimeoutMs = 20000;
constexpr uint32_t kHttpConnectMs = 8000;
constexpr uint32_t kHttpReadMs = 15000;
constexpr uint32_t kTtsReadMs = 15000;  // TTS audio read budget (v1.2)
constexpr uint32_t kAsrReadMs = 15000;  // ASR read budget (v1.3)

// Bounded retry for transient LLM failures (transport / 429 / 5xx).
constexpr int kMaxRetries = 2;
constexpr int kAsrMaxRetries = 2;  // transient ASR (408 SLOW_UPLOAD / 5xx) retries
constexpr uint32_t kRetryBaseMs = 500;
constexpr uint32_t kRetryCapMs = 4000;

// Wi-Fi reconnect backoff (ARCHITECTURE §Error handling: ~0.5 -> 8 s).
constexpr uint32_t kWifiBackoffBaseMs = 500;
constexpr uint32_t kWifiBackoffCapMs = 8000;

// How long the Error state lingers on the LCD before auto-returning to Idle
// (v1.4 resilience). The loop never gets stuck on Error.
constexpr uint32_t kErrorDwellMs = 2000;

pyramid::LineReader g_reader;
pyramid::History g_history(HISTORY_MAX_TURNS);

// Wi-Fi reconnect state (non-blocking).
bool g_offline = false;
int g_wifiAttempt = 0;
uint32_t g_nextWifiTryMs = 0;

// Audio capture (v1.1): fixed PCM16 mono record buffer, bounded by REC_MAX_MS.
constexpr size_t kMaxSamples =
    pyramid::samplesForMs(REC_MAX_MS, AUDIO_SAMPLE_RATE);
int16_t g_pcm[kMaxSamples];
size_t g_pcmLen = 0;  // valid samples captured in the last push-to-talk

// Push-to-talk latency tracking (button press -> first spoken audio). Stamped
// across the voice path (loop -> voiceTurn -> handleTurn -> playbackCaptured)
// and reported when playback starts. g_voiceActive scopes it to the button
// path so the serial text path doesn't emit (or pollute) a latency line.
pyramid::VoiceStamps g_stamps;
bool g_voiceActive = false;

// All status logging routes through here and is gated by DEBUG_SERIAL, so the
// device can be quietened later without touching call sites.
void logf(const char* fmt, ...) {
  if (!DEBUG_SERIAL) return;
  char line[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  Serial.print("[log] ");
  Serial.println(line);
}

// Turn-state machine (v1.4): one source of truth for what the device is doing.
// The transition logic is pure + host-tested (states.h); here we hold the
// current state, render it to the LCD (label + a glance-readable color), and
// expose applyEvent() so every path updates state the same way. Starts Offline
// (boot, before the first Wi-Fi connect).
pyramid::TurnState g_state = pyramid::TurnState::Offline;
uint32_t g_errorSinceMs = 0;  // when we entered Error (for the auto-return dwell)

// Latest exchange, for the optional on-screen transcript (SHOW_TRANSCRIPT).
std::string g_userText;   // what the user said / typed
std::string g_replyText;  // the assistant's reply

// Answer-time stats: "request ready -> reply spoken" for the last turn, plus a
// running session average. For voice the request is ready at button release; for
// typed input, at handleTurn entry.
uint32_t g_answerStartMs = 0;  // request-ready timestamp for the in-flight turn
uint32_t g_lastAnswerMs = 0;   // last answer's total time (ms)
uint32_t g_answerCount = 0;    // answers counted this session
uint64_t g_answerSumMs = 0;    // sum of answer times (for the average)

// State -> LCD background color (firmware-only; the label lives in states.h).
uint16_t stateColor(pyramid::TurnState s) {
  switch (s) {
    case pyramid::TurnState::Idle:
      return TFT_BLACK;
    case pyramid::TurnState::Listening:
      return TFT_NAVY;     // capturing speech
    case pyramid::TurnState::Thinking:
      return TFT_OLIVE;    // processing (ASR/LLM/TTS)
    case pyramid::TurnState::Replying:
      return TFT_DARKGREEN;  // talking back
    case pyramid::TurnState::Error:
      return TFT_MAROON;   // a turn failed
    case pyramid::TurnState::Offline:
      return TFT_DARKGREY;  // Wi-Fi down / input paused
  }
  return TFT_BLACK;
}

#if SHOW_TRANSCRIPT
// Bright per-state color for the status word in transcript mode (the dark
// stateColor backgrounds would be unreadable as text on black).
uint16_t statusTextColor(pyramid::TurnState s) {
  switch (s) {
    case pyramid::TurnState::Idle:      return TFT_WHITE;
    case pyramid::TurnState::Listening: return TFT_CYAN;
    case pyramid::TurnState::Thinking:  return TFT_YELLOW;
    case pyramid::TurnState::Replying:  return TFT_GREEN;
    case pyramid::TurnState::Error:     return TFT_RED;
    case pyramid::TurnState::Offline:   return TFT_ORANGE;
  }
  return TFT_WHITE;
}

// Transcript mode (SHOW_TRANSCRIPT): show the conversation text on the LCD in a
// small Unicode font. efontCN_12 is a U8g2 Unicode font whose base covers
// Cyrillic (incl. Ukrainian і/ї/є/ґ); the default GFX font is ASCII-only and
// would render Cyrillic as boxes.
void renderTranscript() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setFont(&fonts::efontCN_12);  // ~12 px, Cyrillic-capable
  M5.Display.setTextSize(1);
  M5.Display.setTextWrap(true);
  M5.Display.setCursor(0, 0);

  M5.Display.setTextColor(statusTextColor(g_state));
  M5.Display.printf("[%s]\n", pyramid::label(g_state));
  if (g_answerCount > 0) {  // last + average answer time (request-ready -> spoken)
    const uint32_t avg = static_cast<uint32_t>(g_answerSumMs / g_answerCount);
    M5.Display.setTextColor(TFT_YELLOW);
    M5.Display.printf("last %u.%us avg %u.%us\n", g_lastAnswerMs / 1000,
                      (g_lastAnswerMs % 1000) / 100, avg / 1000, (avg % 1000) / 100);
  }
  if (!g_userText.empty()) {
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.printf("> %s\n", g_userText.c_str());
  }
  if (!g_replyText.empty()) {
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.print(g_replyText.c_str());
  }
}
#endif

void renderState() {
#if SHOW_TRANSCRIPT
  renderTranscript();
#else
  M5.Display.fillScreen(stateColor(g_state));
  M5.Display.setCursor(0, 0);
  M5.Display.print(pyramid::label(g_state));
#endif
}

void setState(pyramid::TurnState s) {
  g_state = s;
  if (s == pyramid::TurnState::Error) g_errorSinceMs = millis();
  renderState();
}

// Drive a transition through the pure state machine, then render it.
void applyEvent(pyramid::TurnEvent e) { setState(pyramid::nextState(g_state, e)); }

// Leave the shared ES8311 / I2S bus in mic mode no matter how a turn ended
// (v1.4): a mid-turn abort during the mic<->speaker switch must never leave the
// speaker begun or the mic stopped. Safe to call from any state.
void ensureMicMode() {
  if (M5.Speaker.isEnabled()) M5.Speaker.end();
  if (!M5.Mic.isEnabled()) M5.Mic.begin();
}

// Centralized mid-turn failure (v1.4): log it, restore the audio bus, and pick
// the right recovery state — Offline (input paused) if Wi-Fi dropped, else Error
// (which auto-returns to Idle after kErrorDwellMs). The bounded per-stage
// timeouts guarantee we reach here instead of hanging.
void failTurn(const char* what) {
  Serial.print("error: ");
  Serial.println(what);
  ensureMicMode();
  if (WiFi.status() != WL_CONNECTED) {
    g_offline = true;
    applyEvent(pyramid::TurnEvent::WifiLost);  // -> Offline; serviceWiFi recovers
  } else {
    applyEvent(pyramid::TurnEvent::Fail);  // -> Error -> (dwell) -> Idle
  }
}

bool connectWiFi() {
  logf("wifi: connecting to \"%s\"", WIFI_SSID);
  setState(pyramid::TurnState::Offline);  // not connected yet — input paused
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > kWifiTimeoutMs) {
      logf("wifi: connect timeout (will retry in loop)");
      setState(pyramid::TurnState::Offline);
      return false;
    }
    delay(250);
  }
  logf("wifi: connected, ip=%s", WiFi.localIP().toString().c_str());
  applyEvent(pyramid::TurnEvent::WifiUp);  // Offline -> Idle
  return true;
}

// Non-blocking Wi-Fi supervisor: tracks offline state and reconnects with
// exponential backoff. Called every loop tick; input is paused while offline.
void serviceWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (g_offline) {
      g_offline = false;
      g_wifiAttempt = 0;
      logf("wifi: reconnected, ip=%s", WiFi.localIP().toString().c_str());
      applyEvent(pyramid::TurnEvent::WifiUp);  // Offline -> Idle
    }
    return;
  }

  if (!g_offline) {
    g_offline = true;
    logf("wifi: connection lost — pausing input");
    applyEvent(pyramid::TurnEvent::WifiLost);  // -> Offline
  }

  const uint32_t now = millis();
  if (now >= g_nextWifiTryMs) {
    const uint32_t wait =
        pyramid::backoffDelayMs(g_wifiAttempt, kWifiBackoffBaseMs, kWifiBackoffCapMs);
    logf("wifi: reconnect attempt %d (next backoff %u ms)", g_wifiAttempt + 1,
         static_cast<unsigned>(wait));
    WiFi.reconnect();
    g_nextWifiTryMs = now + wait;
    ++g_wifiAttempt;
  }
}

// Push-to-talk capture (v1.1): record 16 kHz mono PCM16 from the Echo Base mic
// while BtnA is held, bounded by REC_MAX_MS, then log length + level. Mic and
// speaker share the I2S bus, so the speaker is released first. Mic bring-up
// needs the board (manual DoD check); the sizing/level math is host-tested.
void recordWhileHeld() {
  applyEvent(pyramid::TurnEvent::Listen);  // -> Listening
  logf("rec: start");
  // Sit in mic mode on the shared ES8311 / I2S (as in v1.1/v1.3). record()
  // also auto-begins if needed.
  if (!M5.Mic.isEnabled()) M5.Mic.begin();

  // Pause-based end-of-utterance (v1.4). CRUCIAL: keep queueing chunks
  // continuously and drain the mic queue only ONCE at the end (like v1.3) —
  // draining per chunk made M5's mic_task stop+restart the I2S driver and crash
  // (i2s_stop on an uninstalled port). So we never wait mid-capture; instead the
  // endpointer is fed chunks the real-time DMA has surely filled by now, tracked
  // by elapsed time (lagging the queued `total`).
  pyramid::Endpointer ep{VAD_SILENCE_PEAK, VAD_HANGOVER_MS, RECOG_PATIENCE_MS};
  size_t total = 0;       // samples queued/captured
  size_t analyzed = 0;    // samples already fed to the endpointer
  constexpr size_t kChunk = 512;
  constexpr uint32_t kChunkMs = kChunk * 1000u / AUDIO_SAMPLE_RATE;  // 32 ms
  bool endedByPause = false;
  const uint32_t recStart = millis();

  while (M5.BtnA.isPressed() && total < kMaxSamples) {
    const size_t want = pyramid::capSamples(kChunk, kMaxSamples - total);
    if (M5.Mic.record(&g_pcm[total], want, AUDIO_SAMPLE_RATE)) {
      total += want;
    }
    // Feed the endpointer with chunks the DMA has filled by now (real time),
    // never reading ahead of capture and never draining the queue.
    size_t filled =
        static_cast<size_t>((millis() - recStart) * 1ull * AUDIO_SAMPLE_RATE / 1000);
    if (filled > total) filled = total;
    while (analyzed + kChunk <= filled) {
      const pyramid::PcmStats cs = pyramid::analyzePcm(&g_pcm[analyzed], kChunk, 32700);
      analyzed += kChunk;
      if (ep.feed(cs.peak, kChunkMs)) {  // natural pause or recog_patience cap
        endedByPause = true;
        break;
      }
    }
    if (endedByPause) break;
    M5.update();
  }
  while (M5.Mic.isRecording()) delay(1);  // single clean drain (as in v1.3)
  g_pcmLen = total;

  const pyramid::PcmStats st = pyramid::analyzePcm(g_pcm, g_pcmLen, 32700);
  const uint32_t ms =
      static_cast<uint32_t>(g_pcmLen * 1000ull / AUDIO_SAMPLE_RATE);
  logf("rec: %u samples (%u ms) peak=%d clipped=%u end=%s",
       static_cast<unsigned>(g_pcmLen), static_cast<unsigned>(ms),
       static_cast<int>(st.peak), static_cast<unsigned>(st.clipped),
       endedByPause ? "pause" : "release");
  // No state change here: voiceTurn() fires Think (transcribing) or Done (gated
  // out) next, so we don't flash Idle between capture and processing.
}

// Playback (v1.1): play the just-captured PCM16 buffer through the Echo Base
// speaker, then return to mic mode. Mic and speaker share the ES8311 / I2S, so
// (per M5Unified's Microphone example) we drain the mic DMA, end the mic, begin
// the speaker, play, end the speaker, and re-begin the mic — ending the active
// side before claiming the bus avoids tearing down I2S under a live task. This
// proves the push-to-talk record -> playback loop (v1.1 DoD); v1.2 renders
// cloud TTS through the same path.
void playbackCaptured() {
  if (g_pcmLen == 0) return;
  applyEvent(pyramid::TurnEvent::Reply);  // -> Replying
  logf("play: %u samples", static_cast<unsigned>(g_pcmLen));

  while (M5.Mic.isRecording()) delay(1);  // let capture DMA finish first
  M5.Mic.end();                           // release the shared bus
  if (!M5.Speaker.begin()) {
    failTurn("speaker begin failed");  // restores mic mode + Error/Offline
    return;
  }
  M5.Speaker.setVolume(SPK_VOLUME);
  g_stamps.speakMs = millis();  // audio starts here — the moment the user hears a reply
  M5.Speaker.playRaw(g_pcm, g_pcmLen, AUDIO_SAMPLE_RATE);  // mono PCM16
  if (g_voiceActive) {
    const pyramid::LatencyBreakdown b = pyramid::computeLatency(g_stamps);
    Serial.printf(
        "[latency] press->speak=%lu ms (speech=%lu ms; reply=%lu ms = asr %lu + llm %lu + tts %lu + other %lu)\n",
        static_cast<unsigned long>(b.total), static_cast<unsigned long>(b.speech),
        static_cast<unsigned long>(b.total - b.speech), static_cast<unsigned long>(b.asr),
        static_cast<unsigned long>(b.llm), static_cast<unsigned long>(b.tts),
        static_cast<unsigned long>(b.other));
    g_voiceActive = false;  // consumed; next press re-arms it
  }
  while (M5.Speaker.isPlaying()) {
    M5.update();
    delay(1);
  }
  M5.Speaker.end();  // hand the bus back
  M5.Mic.begin();    // return to mic mode for the next push-to-talk
  applyEvent(pyramid::TurnEvent::Done);  // -> Idle
}

// TTS (v1.2): fetch spoken audio for `text` from ElevenLabs into g_pcm as raw
// 16 kHz mono PCM16 (no decode), setting g_pcmLen so playbackCaptured() can
// play it. Returns true on success; false + a readable `err` otherwise. The
// audio is bounded by the g_pcm buffer (PYR-010 caps the text via
// TTS_MAX_CHARS so a reply fits). Network read; verified on hardware.
bool ttsFetch(const std::string& text, std::string& err) {
  if (text.empty()) {
    err = "tts: empty text";
    return false;
  }
  // Bound the reply sent to TTS (UTF-8 boundary-safe) so it fits the buffer and
  // stays cheap; the full text is already on serial regardless.
  const std::string spoken = pyramid::clampUtf8(text, TTS_MAX_CHARS);
  if (spoken.size() < text.size()) {
    logf("tts: reply truncated %u -> %u bytes (TTS_MAX_CHARS=%d)",
         static_cast<unsigned>(text.size()), static_cast<unsigned>(spoken.size()),
         static_cast<int>(TTS_MAX_CHARS));
  }

  WiFiClientSecure client;
  client.setInsecure();  // v0: no cert pinning under the private allowlist model

  HTTPClient http;
  http.setConnectTimeout(kHttpConnectMs);
  http.setTimeout(kTtsReadMs);
  const String url =
      String(TTS_ENDPOINT_BASE) + TTS_VOICE_ID + "?output_format=pcm_16000";
  if (!http.begin(client, url)) {
    err = "tts: http begin failed";
    return false;
  }
  http.addHeader("xi-api-key", TTS_API_KEY);
  http.addHeader("content-type", "application/json");
  http.addHeader("accept", "audio/pcm");

  const std::string body = pyramid::buildTtsRequest(TTS_MODEL, spoken);
  const int status = http.POST(String(body.c_str()));
  if (status != 200) {
    const String payload = http.getString();  // error body is small text/JSON
    err = "tts: http " + std::to_string(status);
    if (payload.length()) {
      err += ": " + std::string(payload.c_str()).substr(0, 120);
    }
    http.end();
    return false;
  }

  // Read raw PCM16 into g_pcm (bounded). Handle both Content-Length (raw) and
  // chunked transfer (dechunk via the host-tested Dechunker from sse.h).
  WiFiClient* stream = http.getStreamPtr();
  const int contentLen = http.getSize();  // >=0 if known, -1 if chunked
  const bool chunked = (contentLen < 0);
  uint8_t* const dst = reinterpret_cast<uint8_t*>(g_pcm);
  const size_t cap = kMaxSamples * sizeof(int16_t);
  size_t got = 0;
  uint8_t sbuf[512];
  std::string dec;
  pyramid::Dechunker dechunk;
  uint32_t lastRx = millis();

  while (got < cap) {
    if (!chunked && contentLen >= 0 && got >= static_cast<size_t>(contentLen)) break;
    const int avail = stream ? stream->available() : 0;
    if (avail <= 0) {
      if (!http.connected() && (!stream || stream->available() == 0)) break;
      if (millis() - lastRx > kTtsReadMs) {
        err = "tts: stream timeout";
        http.end();
        g_pcmLen = 0;
        return false;
      }
      delay(2);
      continue;
    }
    int want = avail < static_cast<int>(sizeof(sbuf)) ? avail
                                                      : static_cast<int>(sizeof(sbuf));
    const int n = stream->readBytes(sbuf, want);
    if (n <= 0) {
      delay(2);
      continue;
    }
    lastRx = millis();
    if (chunked) {
      dec.clear();
      dechunk.feed(reinterpret_cast<const char*>(sbuf), n, dec);
      size_t take = dec.size();
      if (take > cap - got) take = cap - got;
      memcpy(dst + got, dec.data(), take);
      got += take;
    } else {
      size_t take = static_cast<size_t>(n);
      if (take > cap - got) take = cap - got;
      memcpy(dst + got, sbuf, take);
      got += take;
    }
  }
  http.end();

  g_pcmLen = got / sizeof(int16_t);  // bytes -> samples (drop a trailing odd byte)
  if (g_pcmLen == 0) {
    err = "tts: empty audio";
    return false;
  }
  logf("tts: %u samples (%u ms)", static_cast<unsigned>(g_pcmLen),
       static_cast<unsigned>(g_pcmLen * 1000ull / TTS_SAMPLE_RATE));
  return true;
}

// ASR (v1.3): transcribe captured PCM16 via Deepgram. The buffer is encoded to
// 8-bit µ-law IN PLACE (halves the upload → fixes 408 SLOW_UPLOAD), POSTed as
// `encoding=mulaw`, with a bounded retry on transient errors (408/429/5xx/
// transport). Sets `transcript` + returns true on a non-empty result; false +
// readable `err` otherwise (the caller re-prompts — PYR-013). `pcm` is mutated
// (encoded in place); the recording isn't needed afterward. Verified on hardware.
bool asrTranscribe(int16_t* pcm, size_t samples, std::string& transcript,
                   float& confidence, std::string& err) {
  if (samples == 0) {
    err = "asr: no audio";
    return false;
  }
  // PCM16 -> µ-law in place: byte i overwrites a byte of an already-encoded
  // earlier sample, while sample i (bytes 2i,2i+1) is still intact when read.
  uint8_t* const mulaw = reinterpret_cast<uint8_t*>(pcm);
  for (size_t i = 0; i < samples; ++i) mulaw[i] = pyramid::ulawEncode(pcm[i]);
  const size_t nbytes = samples;  // 1 byte/sample

  const String url = String(ASR_ENDPOINT) + "?model=" + ASR_MODEL +
                     "&language=" + ASR_LANG +
                     "&encoding=mulaw&sample_rate=" + String(ASR_SAMPLE_RATE);

  for (int attempt = 0; attempt <= kAsrMaxRetries; ++attempt) {
    if (attempt > 0) {
      const uint32_t wait =
          pyramid::backoffDelayMs(attempt - 1, kRetryBaseMs, kRetryCapMs);
      logf("asr: retry %d/%d in %u ms (%s)", attempt, kAsrMaxRetries,
           static_cast<unsigned>(wait), err.c_str());
      delay(wait);
    }
    WiFiClientSecure client;
    client.setInsecure();  // v0: no cert pinning under the private allowlist model
    HTTPClient http;
    http.setConnectTimeout(kHttpConnectMs);
    http.setTimeout(kAsrReadMs);
    if (!http.begin(client, url)) {
      err = "asr: http begin failed";
      return false;
    }
    http.addHeader("Authorization", String("Token ") + ASR_API_KEY);
    http.addHeader("Content-Type", "application/octet-stream");

    const int status = http.POST(mulaw, nbytes);
    if (status == 200) {
      const String payload = http.getString();
      http.end();
      const bool ok =
          pyramid::parseAsrTranscript(payload.c_str(), transcript, confidence, err);
      if (ok) {
        logf("asr: \"%s\" (conf %d%%)", transcript.c_str(),
             static_cast<int>(confidence * 100));
      }
      return ok;  // a parsed 200 (incl. empty -> false) is not retried
    }

    const String payload = http.getString();
    err = "asr: http " + std::to_string(status);
    if (payload.length()) err += ": " + std::string(payload.c_str()).substr(0, 120);
    http.end();
    // Retry only transient failures (slow upload / rate-limit / server / transport).
    const bool retryable =
        (status == 408 || status == 429 || status >= 500 || status <= 0);
    if (!retryable) return false;
  }
  return false;  // retries exhausted; err holds the last failure
}

// Outcome of a single HTTP attempt: ok (reply set), or failed (err set) with a
// hint on whether retrying is worthwhile. Also carries per-attempt timing and
// the token usage reported by the API.
struct Attempt {
  bool ok = false;
  bool retryable = false;
  std::string reply;
  std::string err;
  uint32_t firstMs = 0;  // request sent -> server response received
  uint32_t totalMs = 0;  // request sent -> body fully read
  pyramid::Usage usage;
};

// One synchronous HTTPS attempt: POST persona + history to the LLM and read
// back the reply. Never hangs — connect/read timeouts are bounded.
Attempt llmAttempt(const std::vector<pyramid::Turn>& turns) {
  Attempt r;
  WiFiClientSecure client;
  client.setInsecure();  // v0: no cert pinning under the private allowlist model

  HTTPClient http;
  http.setConnectTimeout(kHttpConnectMs);
  http.setTimeout(kHttpReadMs);
  if (!http.begin(client, LLM_ENDPOINT)) {
    r.err = "http begin failed";
    return r;
  }
  http.addHeader("content-type", "application/json");
  http.addHeader("x-api-key", LLM_API_KEY);
  http.addHeader("anthropic-version", LLM_ANTHROPIC_VERSION);

  http.addHeader("accept", "text/event-stream");

  const std::string body =
      pyramid::buildChatRequest(LLM_MODEL, LLM_PERSONA, turns, LLM_MAX_TOKENS);

  const uint32_t tStart = millis();
  const int status = http.POST(String(body.c_str()));

  if (status <= 0) {
    r.err = std::string("transport error: ") +
            HTTPClient::errorToString(status).c_str();
    r.retryable = true;  // transport-level, nothing streamed yet: worth a retry
    r.totalMs = millis() - tStart;
    http.end();
    return r;
  }

  if (status < 200 || status >= 300) {
    // Errors come back as a normal (non-streamed) JSON body.
    const String payload = http.getString();
    std::string discard, perr;
    pyramid::parseChatReply(payload.c_str(), discard, perr);
    r.err = "http " + std::to_string(status) + (perr.empty() ? "" : ": " + perr);
    r.retryable = pyramid::isRetryableHttpStatus(status);
    r.totalMs = millis() - tStart;
    http.end();
    return r;
  }

  // 2xx: read the SSE stream, printing tokens as they arrive and capturing the
  // time-to-first-token + usage. Raw socket bytes -> dechunk -> SSE lines ->
  // events (all of that decode path is host-tested in sse.h).
  WiFiClient* stream = http.getStreamPtr();
  pyramid::Dechunker dechunk;
  pyramid::LineReader sseLines;
  std::string decoded, line, dataJson;
  bool gotFirst = false, sawStop = false, streamErr = false;
  uint8_t buf[256];
  uint32_t lastRx = millis();

  while (!sawStop && !streamErr) {
    const int avail = stream ? stream->available() : 0;
    if (avail <= 0) {
      if (!http.connected() && (!stream || stream->available() == 0)) break;
      if (millis() - lastRx > kHttpReadMs) {
        r.err = "stream timeout";
        break;
      }
      delay(2);
      continue;
    }
    const int want = avail < static_cast<int>(sizeof(buf)) ? avail
                                                           : static_cast<int>(sizeof(buf));
    const int n = stream->readBytes(buf, want);
    if (n <= 0) {
      delay(2);
      continue;
    }
    lastRx = millis();

    decoded.clear();
    dechunk.feed(reinterpret_cast<const char*>(buf), n, decoded);
    for (char ch : decoded) {
      if (!sseLines.feed(ch, line)) continue;
      if (!pyramid::extractSseData(line, dataJson)) continue;
      pyramid::StreamEvent ev;
      if (!pyramid::parseStreamEvent(dataJson, ev)) continue;
      switch (ev.kind) {
        case pyramid::StreamEvent::kMessageStart:
          r.usage.inputTokens = ev.inputTokens;
          r.usage.outputTokens = ev.outputTokens;
          break;
        case pyramid::StreamEvent::kTextDelta:
          if (!gotFirst) {
            gotFirst = true;
            r.firstMs = millis() - tStart;  // genuine time-to-first-token
          }
          r.reply += ev.text;
          Serial.print(ev.text.c_str());  // stream to serial as it arrives
          break;
        case pyramid::StreamEvent::kMessageDelta:
          r.usage.outputTokens = ev.outputTokens;  // cumulative final count
          break;
        case pyramid::StreamEvent::kError:
          streamErr = true;
          r.err = ev.error;
          break;
        case pyramid::StreamEvent::kMessageStop:
          sawStop = true;
          break;
        default:
          break;
      }
      if (sawStop || streamErr) break;
    }
  }

  http.end();
  r.totalMs = millis() - tStart;
  if (gotFirst) {
    Serial.println();  // terminate the streamed reply line
    r.ok = true;       // a mid-stream hiccup after text keeps the partial reply
  } else if (r.err.empty()) {
    r.err = "empty stream";
  }
  return r;  // streamed turns are committed once they print; not retried
}

// A full chat turn with bounded retry + backoff. Returns the final Attempt:
// .ok with .reply / .usage / .firstMs on success, or !ok with .err once
// retries are spent.
Attempt llmTurn(const std::vector<pyramid::Turn>& turns) {
  Attempt a;
  for (int i = 0; i <= kMaxRetries; ++i) {
    if (i > 0) {
      const uint32_t wait =
          pyramid::backoffDelayMs(i - 1, kRetryBaseMs, kRetryCapMs);
      logf("llm: retry %d/%d in %u ms (%s)", i, kMaxRetries,
           static_cast<unsigned>(wait), a.err.c_str());
      delay(wait);
    }
    a = llmAttempt(turns);
    if (a.ok) return a;
    if (!a.retryable) break;
  }
  return a;
}

// One chat turn from a user utterance (typed or transcribed): run it through the
// LLM (with history) and speak the reply via TTS. Shared by the serial text path
// and the voice path so there is a single LLM→TTS chain. The full text reply is
// always on serial; a TTS failure degrades to that text.
void handleTurn(const std::string& userText) {
  logf("text_in: \"%s\"", userText.c_str());
  if (g_offline) {
    Serial.println("offline: input paused (reconnecting)");
    return;
  }
  logf("thinking...");
  // Answer-time clock: for typed input the request is ready now; for voice,
  // voiceTurn already set g_answerStartMs to the button-release time (so the
  // voice answer time includes ASR).
  if (!g_voiceActive) g_answerStartMs = millis();
  g_userText = userText;  // for the optional on-screen transcript (SHOW_TRANSCRIPT)
  g_replyText.clear();
  applyEvent(pyramid::TurnEvent::Think);  // -> Thinking (renders the user line)

  // Build the request from history + the pending user turn; commit to history
  // only on success so a failed call can't poison the context.
  std::vector<pyramid::Turn> req = g_history.turns();
  req.push_back(pyramid::Turn{"user", userText});

  const uint32_t turnStart = millis();
  const Attempt a = llmTurn(req);  // reply streams to serial inside
  const uint32_t allMs = millis() - turnStart;
  if (g_voiceActive) g_stamps.llmMs = allMs;  // for the press->speak breakdown

  if (a.ok) {
    Serial.printf(
        "[stats] first_token=%lu ms  total=%lu ms  tokens: in=%d out=%d total=%d\n",
        static_cast<unsigned long>(a.firstMs), static_cast<unsigned long>(allMs),
        a.usage.inputTokens, a.usage.outputTokens, a.usage.total());
    g_history.addUser(userText);
    g_history.addAssistant(a.reply);
    g_replyText = a.reply;  // show the reply on screen while TTS loads (transcript mode)
#if SHOW_TRANSCRIPT
    renderState();
#endif
    // Still Thinking while we fetch the TTS audio; playbackCaptured() flips to
    // Replying once it actually plays.
    std::string terr;
    const uint32_t ttsStart = millis();
    const bool spoke = ttsFetch(a.reply, terr);
    if (g_voiceActive) g_stamps.ttsMs = millis() - ttsStart;
    if (spoke) {
      playbackCaptured();  // plays g_pcm filled by ttsFetch (mic<->spk switch)
      // Answer delivered: record total time (request-ready -> first audio) + avg.
      g_lastAnswerMs = g_stamps.speakMs - g_answerStartMs;
      g_answerCount++;
      g_answerSumMs += g_lastAnswerMs;
      const uint32_t avg = static_cast<uint32_t>(g_answerSumMs / g_answerCount);
      Serial.printf("[answer] last %u.%us  avg %u.%us  (n=%u)\n", g_lastAnswerMs / 1000,
                    (g_lastAnswerMs % 1000) / 100, avg / 1000, (avg % 1000) / 100,
                    static_cast<unsigned>(g_answerCount));
    } else {
      logf("tts failed (%s) — reply shown as text only", terr.c_str());
      applyEvent(pyramid::TurnEvent::Done);  // degraded to text -> Idle
    }
  } else {
    Serial.printf("[stats] failed after %lu ms\n",
                  static_cast<unsigned long>(allMs));
    failTurn(a.err.c_str());  // Error (auto-returns to Idle) or Offline if Wi-Fi dropped
  }
}

// Re-prompt (v1.3) when the input couldn't be used (silence / empty / low
// confidence): speak a short nudge, or log it if TTS is unavailable.
void rePrompt(const char* reason) {
  logf("voice: %s — re-prompting", reason);
  std::string terr;
  const uint32_t ttsStart = millis();
  const bool spoke = ttsFetch("Не почула, повтори, будь ласка.", terr);
  if (g_voiceActive) g_stamps.ttsMs = millis() - ttsStart;
  if (spoke) {
    playbackCaptured();
  } else {
    logf("re-prompt tts failed (%s)", terr.c_str());
    applyEvent(pyramid::TurnEvent::Done);  // -> Idle
  }
}

// Voice turn (v1.3): gate the recording, transcribe via ASR, then run the same
// handleTurn() chain. asrTranscribe reads g_pcm before ttsFetch (in handleTurn /
// rePrompt) overwrites it with reply audio, so the shared buffer is safe.
void voiceTurn() {
  if (g_pcmLen == 0) return;
  if (g_offline) {
    Serial.println("offline: input paused (reconnecting)");
    return;  // already in Offline (serviceWiFi); leave the state as-is
  }
  // Noise/length gate: skip silence and accidental taps before any network call.
  const pyramid::PcmStats st = pyramid::analyzePcm(g_pcm, g_pcmLen, 32700);
  if (!pyramid::shouldTranscribe(g_pcmLen, st.peak, AUDIO_SAMPLE_RATE, REC_MIN_MS,
                                 REC_MIN_PEAK)) {
    logf("voice: too short/quiet (%u samples, peak=%d) — ignored",
         static_cast<unsigned>(g_pcmLen), static_cast<int>(st.peak));
    applyEvent(pyramid::TurnEvent::Done);  // nothing to do -> Idle
    return;
  }

  applyEvent(pyramid::TurnEvent::Think);  // transcribing
  std::string transcript, err;
  float confidence = 0.0f;
  const uint32_t asrStart = millis();
  const bool asrOk = asrTranscribe(g_pcm, g_pcmLen, transcript, confidence, err);
  // Attribute the ASR time on EVERY outcome (success / empty / low-conf / error),
  // so re-prompt turns don't mis-report asr=0 and dump the time into `other`.
  if (g_voiceActive) g_stamps.asrMs = millis() - asrStart;
  if (!asrOk) {
    if (WiFi.status() != WL_CONNECTED) {
      failTurn(err.c_str());  // a network drop, not a misheard phrase -> Offline
    } else {
      rePrompt(err.c_str());  // empty / garbled / timeout → spoken nudge
    }
    return;
  }
  if (confidence < ASR_MIN_CONFIDENCE) {
    rePrompt("low confidence");
    return;
  }
  // Answer-time clock starts at button release, so the voice answer time covers
  // ASR + LLM + TTS (handleTurn won't reset it while g_voiceActive).
  g_answerStartMs = g_stamps.recEndMs;
  handleTurn(transcript);
}

}  // namespace

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
