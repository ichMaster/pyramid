#include "audio_io.h"

#include <M5Unified.h>

#include "app_state.h"
#include "audio.h"
#include "log.h"
#include "timing.h"
#include "turn.h"  // failTurn (mid-turn recovery on speaker failure)
#include "ui.h"
#include "vad.h"

namespace app {

// Leave the shared ES8311 / I2S bus in mic mode no matter how a turn ended
// (v1.4): a mid-turn abort during the mic<->speaker switch must never leave the
// speaker begun or the mic stopped. Safe to call from any state.
void ensureMicMode() {
  if (M5.Speaker.isEnabled()) M5.Speaker.end();
  if (!M5.Mic.isEnabled()) M5.Mic.begin();
}

// Push-to-talk capture: record 16 kHz mono PCM16 from the Echo Base mic while
// BtnA is held, bounded by REC_MAX_MS, ending on release OR a trailing pause
// (v1.4 VAD).
void recordWhileHeld() {
  applyEvent(pyramid::TurnEvent::Listen);  // -> Listening
  logf("rec: start");
  // Sit in mic mode on the shared ES8311 / I2S (as in v1.1/v1.3). record() also
  // auto-begins if needed.
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

// Playback: play g_pcm through the Echo Base speaker, then return to mic mode.
// Mic and speaker share the ES8311 / I2S, so (per M5Unified's Microphone example)
// we drain the mic DMA, end the mic, begin the speaker, play, end the speaker,
// and re-begin the mic — ending the active side before claiming the bus avoids
// tearing down I2S under a live task.
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

}  // namespace app
