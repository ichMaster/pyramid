// Definitions for the shared firmware globals declared in app_state.h.

#include "app_state.h"

namespace app {

pyramid::LineReader g_reader;
pyramid::History g_history(HISTORY_MAX_TURNS);

bool g_offline = false;
int g_wifiAttempt = 0;
uint32_t g_nextWifiTryMs = 0;

int16_t g_pcm[kMaxSamples];
size_t g_pcmLen = 0;

pyramid::VoiceStamps g_stamps;
bool g_voiceActive = false;

pyramid::TurnState g_state = pyramid::TurnState::Offline;
uint32_t g_errorSinceMs = 0;

std::string g_userText;
std::string g_replyText;

uint32_t g_answerStartMs = 0;
uint32_t g_lastAnswerMs = 0;
uint32_t g_answerCount = 0;
uint64_t g_answerSumMs = 0;

}  // namespace app
