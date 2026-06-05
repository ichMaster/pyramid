#include "audio_io.h"

#include <M5Unified.h>

#include "app_state.h"
#include "audio.h"       // analyzePcm, capSamples, PcmStats
#include "log.h"
#include "vad.h"         // Endpointer
#include "ws_client.h"   // wsSendBinary

namespace app {

// Leave the shared ES8311 / I2S bus in mic mode no matter how a turn ended: a
// mid-turn abort during the mic<->speaker switch must never leave the speaker
// begun or the mic stopped. Safe to call from any state.
void ensureMicMode() {
  if (M5.Speaker.isEnabled()) M5.Speaker.end();
  if (!M5.Mic.isEnabled()) M5.Mic.begin();
}

// Streaming push-to-talk capture. Reuses the v1.3/v1.4 safe pattern — never
// drain the mic queue per chunk (that crashed M5's mic_task); instead feed the
// endpointer and the network from the region the real-time DMA has surely filled
// by now (tracked by elapsed time), and drain once at the end. The time-confirmed
// filled region is streamed to the server as binary `audio` frames.
void streamCapture() {
  if (!M5.Mic.isEnabled()) M5.Mic.begin();

  pyramid::Endpointer ep{VAD_SILENCE_PEAK, VAD_HANGOVER_MS, RECOG_PATIENCE_MS};
  size_t total = 0;     // samples queued/captured into g_pcm
  size_t analyzed = 0;  // samples fed to the endpointer
  size_t sent = 0;      // samples already streamed to the server
  constexpr size_t kChunk = 512;
  constexpr uint32_t kChunkMs = kChunk * 1000u / AUDIO_SAMPLE_RATE;  // 32 ms
  bool endedByPause = false;
  const uint32_t recStart = millis();

  while (M5.BtnA.isPressed() && total < kMaxSamples) {
    const size_t want = pyramid::capSamples(kChunk, kMaxSamples - total);
    if (M5.Mic.record(&g_pcm[total], want, AUDIO_SAMPLE_RATE)) {
      total += want;
    }
    size_t filled =
        static_cast<size_t>((millis() - recStart) * 1ull * AUDIO_SAMPLE_RATE / 1000);
    if (filled > total) filled = total;

    // Stream the time-confirmed filled region (binary audio frames).
    while (sent + kChunk <= filled) {
      wsSendBinary(reinterpret_cast<const uint8_t*>(&g_pcm[sent]), kChunk * 2);
      sent += kChunk;
    }
    // Pause-based end-of-utterance on the same filled region.
    while (analyzed + kChunk <= filled) {
      const pyramid::PcmStats cs = pyramid::analyzePcm(&g_pcm[analyzed], kChunk, 32700);
      analyzed += kChunk;
      if (ep.feed(cs.peak, kChunkMs)) {
        endedByPause = true;
        break;
      }
    }
    if (endedByPause) break;
    M5.update();
  }
  while (M5.Mic.isRecording()) delay(1);  // single clean drain (v1.3 safe pattern)

  // Flush any captured tail not yet streamed.
  if (total > sent) {
    wsSendBinary(reinterpret_cast<const uint8_t*>(&g_pcm[sent]),
                 (total - sent) * 2);
    sent = total;
  }
  const uint32_t ms = static_cast<uint32_t>(total * 1000ull / AUDIO_SAMPLE_RATE);
  logf("stream: %u samples (%u ms) end=%s", static_cast<unsigned>(total),
       static_cast<unsigned>(ms), endedByPause ? "pause" : "release");
}

// Claim the shared bus for playback (mic -> speaker). End the mic before
// beginning the speaker so I2S isn't torn down under a live capture task.
void beginSpeaker() {
  while (M5.Mic.isRecording()) delay(1);
  if (M5.Mic.isEnabled()) M5.Mic.end();
  if (!M5.Speaker.begin()) {
    logf("speaker begin failed");
    return;
  }
  M5.Speaker.setVolume(SPK_VOLUME);
}

// Queue one PCM16 chunk for immediate playback (M5Speaker copies into its DMA
// queue and plays asynchronously, so chunks stream out as they arrive).
void playPcmChunk(const uint8_t* data, size_t len) {
  if (len < 2) return;
  M5.Speaker.playRaw(reinterpret_cast<const int16_t*>(data),
                     len / 2, AUDIO_SAMPLE_RATE);  // mono PCM16
}

// Drain the queue, then hand the bus back to the mic for the next push-to-talk.
void endSpeaker() {
  while (M5.Speaker.isPlaying()) {
    M5.update();
    delay(1);
  }
  if (M5.Speaker.isEnabled()) M5.Speaker.end();
  M5.Mic.begin();
}

}  // namespace app
