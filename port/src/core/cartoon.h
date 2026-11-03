// TASK-031-6: the intermission cartoons as GAME LOGIC. A cartoon is produced as a list of drawing calls in the format of the hires replay ('CSCR' script), written from the
// disassembly ($81AE...), and executed with hires::replayScript. Nothing here comes from a recorded run.
//   cartoon 1 ($81AE): levels with (level mod 13) = 2   -- model equal to the oracle call for call (docs/CARTOONS.md section 3)
//   cartoon 2 and 3: later steps (031-7)
#pragma once

#include <cstdint>
#include <vector>

#include "../datapack/datapack.h"

namespace pacman::core {

// Which cartoon follows the end of level `level` ($8155): 1, 2 or 3, or 0 for none. Level mod 13 = 2, 5 or 9 (docs/CARTOONS.md section 1).
int cartoonAfterLevel(unsigned level) noexcept;

// The drawing calls of cartoon 1 (page clear first), as a 'CSCR' script for hires::replayScript. Empty if the pack lacks the frame tables $96E8 / $9E5C.
// The script's recorded-hash field is zero. `colour` is the original's $1D flag (the real game: true).
std::vector<std::uint8_t> cartoon1Script(const datapack::Pack& pack, bool colour = true);

}  // namespace pacman::core
