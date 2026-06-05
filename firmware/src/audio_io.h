#pragma once

// Pyramid v2.1 — Echo Base mic/speaker I/O on the shared ES8311 / I2S (app
// namespace). The mic is streamed up as binary `audio` frames; inbound
// `tts_audio` chunks are played as they arrive (early/streaming playback).

#include <cstddef>
#include <cstdint>

namespace app {

void ensureMicMode();  // leave the shared bus in mic mode (recovery-safe)

// Push-to-talk streaming capture: record 16 kHz mono PCM16 while BtnA is held,
// streaming each filled chunk to the server (ws binary), ending on release or a
// trailing pause (v1.4 VAD). Bounded by REC_MAX_MS.
void streamCapture();

// Early/streaming playback: switch the shared bus mic->speaker once (first
// chunk), play each chunk as it arrives, then drain + switch back on tts_end.
void beginSpeaker();
void playPcmChunk(const uint8_t* data, size_t len);
void endSpeaker();

}  // namespace app
