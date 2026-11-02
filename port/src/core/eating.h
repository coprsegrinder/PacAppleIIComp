// Eating a dot or an energizer (TASK-029d4), rebuilt from $7A5F-$7B2E: the item test at the map cell, the delay of Pac-Man (StepTimer::itemEaten), the
// score in BCD, the item counters and the cleared map cell. Checked against the oracle recordings.
//
// NOT rebuilt here (other pieces): what an ENERGIZER does to the ghosts ($6D60: frightened flags, timers, score step $68, $E4 ...: ghost logic, M5),
// the sounds ($7BDC, $7C45) and the drawing/high-score update ($7091, $5743), the level end ($6CF4 when the last item is gone: levels, M6),
// the sounds and drawing. The extra life at 10,000 points is checked here (it is inside the score routine $7091, which $7B12 calls): see lives.h.
#pragma once

#include <cstdint>

#include "gamestate.h"
#include "pacstep.h"
#include "steptimer.h"

namespace pacman::core {

struct EatResult {
    bool ate = false;            // there was an item in the cell
    bool energizer = false;      // it was an energizer (the caller must apply the ghost effects)
    bool levelComplete = false;  // the last item of the maze was eaten ($56 reached 0, real game only)
    bool outsideMap = false;     // the cell counters pointed past the 896-byte map (only after the position was poked without the counters): the original then reads and
                                 // clears bytes of the master copy that follows the map ($65BA...), which is not part of the logical state; nothing is done here
};

// BCD addition on the three score bytes $52 (low), $53, $54 (high), as the 6502's decimal mode does it.
void addToScore(GameState& state, std::uint8_t bcdLow);

// The item test of $7A5F for the cell Pac-Man's step reported. Cell value 0: nothing happens. Otherwise: StepTimer::itemEaten, then
//   real game: dot (value 1): score + 10; energizer (any other non-zero value): score + 50; cell := 0; $57 -= 1; $56 -= 1.
//   attract demo ($E8 <> 0): cell := 0 and $56 -= 1 only ($7B1F-$7B2E: no score, no fruit counter).
EatResult eatAt(GameState& state, const ItemTest& cell, StepTimer& timer, const StepTables& tables, int player = 1);

}  // namespace pacman::core
