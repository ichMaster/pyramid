#pragma once

// Pyramid — Echo Base mic/speaker I/O on the shared ES8311 / I2S (app namespace).
// Capture fills g_pcm (push-to-talk + VAD end-of-utterance); playback plays
// g_pcm (the captured clip in v1.1, or cloud TTS audio from v1.2+).

namespace app {

void ensureMicMode();      // leave the shared bus in mic mode (recovery-safe)
void recordWhileHeld();    // capture into g_pcm until release or a trailing pause
void playbackCaptured();   // play g_pcm through the speaker, then back to mic mode

}  // namespace app
