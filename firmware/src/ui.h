#pragma once

// Pyramid — turn-state machine + LCD rendering (app namespace).
//
// One source of truth for what the device is doing. The transition logic is
// pure + host-tested (states.h); this drives the current state to the LCD
// (colored state screen, or the optional small-font transcript) and exposes
// applyEvent() so every path updates state the same way.

#include "states.h"

namespace app {

void renderState();                      // redraw the LCD for the current state
void setState(pyramid::TurnState s);     // set state + render (stamps Error dwell)
void applyEvent(pyramid::TurnEvent e);   // transition via the pure state machine

}  // namespace app
