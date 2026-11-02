// Pac-Man's step (TASK-029d3, first part): the position update, the tunnel transit, the animation counter and the cell counters, rebuilt from
// $7962-$79A1, $79FD-$7A5D and $7B2F-$7BAA and checked against the oracle recordings. NOT included (other pieces): the item test and eating
// ($7A5F-$7B2E, TASK-029d4/029e), the fruit spot ($79CD-$79FB), the drawing ($79A4, 029g).
//
// Pac-Man moves 2 units per step. The cell counters say where in the 6-unit grid of the dot map Pac-Man is: ($6A, $6B) = column and row of the
// cell, ($6C, $6D) = 0, 2 or 4 units into it. The dot map cell of an item test is reported to the caller.
#pragma once

#include <cstdint>
#include <optional>

#include "gamestate.h"
#include "junction.h"

namespace pacman::core {

struct ItemTest {
    std::uint8_t column = 0, row = 0;   // the map cell ($623A + 32 * column + row) the original looks at ($7A6B)
};

// One step of an acting tick after junctionStep(): for Move the normal step, for Tunnel the transit code, for Blocked nothing (Pac-Man stands).
// Returns the map cell to test for an item when the step reaches the point where the original tests ($7A5F), otherwise nothing.
std::optional<ItemTest> stepPacman(GameState& state, JunctionOutcome outcome);

}  // namespace pacman::core
