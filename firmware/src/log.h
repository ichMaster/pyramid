#pragma once

// Pyramid — status logging (app namespace).
//
// All status logging routes through logf(), gated by DEBUG_SERIAL, so the device
// can be quietened without touching call sites. Header-inline so every module
// can log without a separate TU. (Named app::logf to avoid clashing with the C
// library's ::logf.)

#include <Arduino.h>

#include <cstdarg>
#include <cstdio>

#include "config.h"

namespace app {

inline void logf(const char* fmt, ...) {
  if (!DEBUG_SERIAL) return;
  char line[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  Serial.print("[log] ");
  Serial.println(line);
}

}  // namespace app
