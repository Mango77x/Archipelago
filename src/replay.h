#pragma once

// --- Replay: records the player's resolved input each fixed step, not world
// state — a debugging tool for reproducing a specific run bit-for-bit (see
// "Sistema de Replay" en el vault), not a save format. Only meaningful when
// played back from the same starting state the recording began at (restart
// the app, then start playback immediately) — it doesn't snapshot anything,
// it just re-feeds the same inputs against a fresh simulation.

#include <vector>

namespace archipelago {

struct ReplayFrame {
    bool thrustForward = false;
    bool thrustBackward = false;
    bool turnLeft = false;
    bool turnRight = false;
    int buyAction = 0;  // 0 = none, 1 = buy Iron-route ship, 2 = buy Steel-route ship
};

void SaveReplay(const std::vector<ReplayFrame>& frames);
bool LoadReplay(std::vector<ReplayFrame>& frames);

}  // namespace archipelago
