// Contact with ghosts, ghosts eaten, Pac-Man caught, lives and the extra life (TASK-029f), rebuilt from $8B34-$8BDB (contact and ghost eaten),
// $8C0E-$8C18 (a life is lost) and $7124-$713B (extra life at 10,000 points, inside the score routine $7091). Checked against the oracle recordings.
//
// NOT rebuilt here: the death animation and sound ($8BDD-$8C07), the restart of a life ($5394, $53BE, $5360, $5306: Pac-Man and the ghosts are put back,
// the other player is loaded, game over is entered: the "start of life" piece), the frightened timer words $E1/$E2 that eating a ghost changes (not in
// the logical state of the recordings), and the drawing of the score image ($7DB8).
#pragma once

#include <cstddef>
#include <cstdint>

#include "gamestate.h"

namespace pacman::core {

enum class Contact {
    None,   // not on the same row/column, or 10 or more units apart
    Near,   // closer than 10 but not closer than 6: only the "near ghost" delay of level 5 and up applies (steptimer.h)
    Touch,  // closer than 6 on the same row or column: the ghost is eaten if it is frightened, otherwise Pac-Man is caught
};

// $8B34-$8B6C: the ghost's position is its record bytes 0 and 1 (the working copy $70/$71 in the original).
Contact ghostContact(const GameState& state, const GhostRecord& ghost) noexcept;

// $8B9D-$8BD9 for a FRIGHTENED ghost that was touched: the ghost becomes eyes (record byte 9 := 1, byte 8 := 0); in a real game the score rises by the
// ghost score step $68 (BCD hundreds: 2, 4, 8, $16), $68 doubles in BCD, and the extra-life check runs. In the demo only the ghost changes.
void ghostEaten(GameState& state, std::size_t ghostIndex, int player = 1);

// $8C0E-$8C18: Pac-Man was caught by a ghost that is not frightened: $58 -= 1; if its low 7 bits are then 0 the whole byte becomes 0 (no lives left).
void pacmanCaught(GameState& state) noexcept;

// $7124-$713B (inside $7091, called after every score change): when the player number is not 0, bit 7 of $58 is set (extra life not yet given) and the
// score has reached 10,000 (the high BCD byte $54 is at least 1), $58 gets +1 and loses bit 7.
void awardBonusLife(GameState& state, int player = 1) noexcept;

}  // namespace pacman::core
