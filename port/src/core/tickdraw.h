// The sprite drawing of a tick (TASK-029g4, first part): which images the original draws when Pac-Man and the ghosts take a step, rebuilt from $79A4-$79CA
// (Pac-Man), $88A2-$88DB (ghost image choice) and the erase calls $699E. Returned as a list of calls with the parameters the original's routines receive,
// to be executed with the verified hires routines and compared with the call log of the oracle (tools/record_tick_calls.py).
//
// Rules (read from the code, checked against 700 ticks): an entity that moved gets (1) a masked clear at its OLD position minus (4, 4) with the mask table
// $69CD and (2) its 3 x 10 image at its NEW position. Order in a tick: Pac-Man first, then the ghosts 4, 3, 2, 1 ($86EA).
// Ghost image: X = 2 * ghost number; frightened: X = 0 (blue) or, while the frightened timer is below $80 and bit 4 of its low byte is clear, 6 (white);
// eyes: X = 10; then (normal drawing mode $1D <> 0) the flag $1E := table $88E1[X / 2] and X += 12; image = word at $9E5C + X.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "../datapack/datapack.h"
#include "gamestate.h"

namespace pacman::core {

struct DrawCall {
    int type = 0;                         // 3 = masked clear (p0 = mask table, p1 = x, p2 = y); 1 = 3 x 10 image (p0 = image base, p1 = x, p2 = y, p3 = $1D, p4 = $1E)
    std::array<int, 5> p{};
    bool operator==(const DrawCall& o) const { return type == o.type && p == o.p; }
};

// The frightened timer words $E1 (low) / $E2 (high) are outside the logical state: passed in (0 = not frightened / unknown).
std::uint16_t ghostImageBase(const GameState& state, std::size_t ghost, std::uint16_t frightenedTimer, const datapack::Pack& pack, int& flag1E);

// Sprite calls for the step from `before` to `after` (positions compared). Empty if nothing moved or the pack lacks the tables.
// pacmanActed: Pac-Man's step timer ran out in this tick. An acting Pac-Man that did not move is BLOCKED ($7905-$7912): he is erased and redrawn standing
// ($66 was set to 2) at the same place.
std::vector<DrawCall> spriteCalls(const GameState& before, const GameState& after, const datapack::Pack& pack, bool pacmanActed = false, std::uint16_t frightenedTimer = 0);

}  // namespace pacman::core
