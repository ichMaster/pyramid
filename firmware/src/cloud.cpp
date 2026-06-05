#include "cloud.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <cstring>

#include "app_state.h"
#include "asr_api.h"
#include "backoff.h"
#include "chat_api.h"
#include "log.h"
#include "sse.h"
#include "tts_api.h"
#include "ulaw.h"

namespace app {

// TTS (v1.2): fetch spoken audio for `text` from ElevenLabs into g_pcm as raw
// 16 kHz mono PCM16 (no decode), setting g_pcmLen so playbackCaptured() can play
// it. The audio is bounded by the g_pcm buffer (TTS_MAX_CHARS caps the text).
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
// 8-bit µ-law IN PLACE (halves the upload -> fixes 408 SLOW_UPLOAD), POSTed as
// `encoding=mulaw`, with a bounded retry on transient errors.
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

namespace {

// One synchronous HTTPS attempt: POST persona + history to the LLM and read back
// the streamed reply. Never hangs — connect/read timeouts are bounded.
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
  // events (that decode path is host-tested in sse.h).
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

}  // namespace

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

}  // namespace app
