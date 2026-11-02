// TASK-030e4: junction detection, the steering decision and the move queue of a ghost: $87BA-$880F and $8C50-$8CB1 of the original.
// A ghost that acts looks whether its position is the position of its current junction ($73) or of the junction it is heading to ($74); on a match that junction becomes the
// current one, the decision routine for (junction, ghost) picks the next direction - toward the ghost's own corner in the corner phase and while frightened, toward Pac-Man
// otherwise, or along the fixed return route when the ghost is only eyes - and the answer is appended to the ghost's move queue (a 256-byte page at $0800 + 256 (n-1)).
// The next queue entry becomes the direction; after a change of direction the countdown is replaced from the "turn" table; the next junction is read from the graph.
#pragma once

#include <array>
#include <cstdint>

#include "../datapack/datapack.h"
#include "gamestate.h"
#include "ghostdecide.h"
#include "ghosttick.h"
#include "junction.h"

namespace pacman::core {

struct GhostDecisionData {
    std::array<std::uint8_t, 256> routineIndex{};      // $8CBE: routine number for (junction * 4 + ghost - 1)
    std::array<std::uint16_t, 48> routineAddress{};    // $8DBE: entry address of each routine
    std::array<std::uint8_t, 10> corners{};            // $8CB4 + 2 * ghost: Y, X of the ghost's corner
    std::array<std::uint8_t, 64> returnRoute{};        // $A360: direction per junction for an eaten ghost
    bool load(const datapack::Pack& pack);
};

using GhostQueues = std::array<std::array<std::uint8_t, 256>, 4>;   // $0800, $0900, $0A00, $0B00

struct JunctionStepResult {
    bool onJunction = false;       // the ghost stood on one of its two junctions
    bool decided = false;          // the decision routine was run (the queue had no further move)
    std::uint8_t junction = 0;     // the junction it stood on
};

// Runs $87BA-$880F for ghost `ghostNumber` (= $E0) AFTER ghostHead(); `sh` is not needed, `cornerPhase` is read from the state ($E9). Pac-Man's position and direction are read from the state.
JunctionStepResult ghostJunction(GameState& s, int ghostNumber, GhostQueues& queues, const JunctionGraph& graph, const GhostDecisionData& data, const GhostTables& tables);

}  // namespace pacman::core
