// TASK-030e3: the first part of the ghost tick $8763 (M5): the step countdown, the shared frightened timer with its end ($6D49) and the choice of the next
// countdown (speed tables). The rest of the tick (junctions, decisions, the move) is added piece by piece in 030e4-030e8; see docs/M5_PLAN.md.
#pragma once

#include <array>
#include <cstdint>

#include "../datapack/datapack.h"
#include "gamestate.h"

namespace pacman::core {

// Variables of the ghost logic that are shared between the ghosts and outside the logical state of the recordings (zero page, V by tools/record_ghost_ram.py).
struct GhostShared {
    std::uint8_t e1 = 0, e2 = 0;   // $E1/$E2: the frightened countdown (16 bit, low byte first)
    std::uint8_t e4 = 0;           // $E4: the frightened period is running
    std::uint8_t ea = 0, eb = 0;   // $EA/$EB: the corner/chase mode countdown (16 bit)
};

struct GhostTables {
    std::array<std::uint8_t, 16> normal{}, turn{}, frightened{}, eyes{};   // $8723, $8733, $8743, $8753: countdown per level class
    bool load(const datapack::Pack& pack);
};

struct HeadResult {
    bool acted = false;             // the countdown reached zero: the ghost takes its turn (the rest of the tick follows)
    bool frightenedEnded = false;   // the shared timer ran out in this call ($6D49 ran)
    std::uint8_t reload = 0;        // the new countdown chosen before the junction logic (the junction logic may replace it: `turn` table after a change of direction)
};

// $8763-$87BF for the ghost with number `ghostNumber` (= $E0, 1..4) whose record is `s.ghosts[ghostNumber - 1]`.
HeadResult ghostHead(GameState& s, int ghostNumber, GhostShared& shared, const GhostTables& tables);

}  // namespace pacman::core
