#include "turn.h"

#include <WiFi.h>

#include <vector>

#include "app_state.h"
#include "audio.h"      // analyzePcm, shouldTranscribe
#include "audio_io.h"   // ensureMicMode, playbackCaptured
#include "chat_api.h"   // pyramid::Turn
#include "cloud.h"      // ttsFetch, asrTranscribe, llmTurn, Attempt
#include "log.h"
#include "ui.h"

namespace app {

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

}  // namespace app
