// TASK-030e7: the ghost effects that happen OUTSIDE a ghost's own tick: the corner/chase countdown with the reversal of all ghosts ($808C, $80C1), the energizer's effect
// on the ghosts ($6D60), the time added to the frightened timer when a ghost is eaten ($8BA4-$8BB5) and the move queues at the start of a life ($6E79).
#pragma once

#include <array>
#include <cstdint>

#include "../datapack/datapack.h"
#include "gamestate.h"
#include "ghostjunction.h"
#include "ghosttick.h"

namespace pacman::core {

struct GhostModeTables {
    std::array<std::uint16_t, 22> frightenedTime{};   // $6DEF: the length of the frightened period per level (index = min(level, 21))
    std::array<std::uint16_t, 16> eatenBonus{};       // $6E19: added to the timer when a ghost is eaten (index = min(level, 15))
    bool load(const datapack::Pack& pack);
};

// $80C1: every ghost that is not in the house reverses: its direction is flipped and put at the current queue position, followed by the terminator.
void reverseGhosts(GameState& s, GhostQueues& queues);

// $808C, called when Pac-Man acts ($7874): counts the mode countdown $EA/$EB down; at zero the phase flips (chase <-> corner), all ghosts reverse and the next countdown is loaded
// (corner -> chase: 600, chase -> corner: 140).
void modeTimerStep(GameState& s, GhostShared& shared, GhostQueues& queues);

// $6D60, called when an energizer is eaten (levels below 21): every ghost that is not eyes becomes frightened, the ghost score step is 200, the frightened timer is loaded, the mode countdown is
// set to 1028, the phase is corner, and all ghosts reverse.
void energizerGhostEffects(GameState& s, GhostShared& shared, GhostQueues& queues, const GhostModeTables& tables);

// $8BA4-$8BB5: a frightened ghost was eaten: the frightened timer grows by the level's bonus.
void ghostEatenTimer(GameState& s, GhostShared& shared, const GhostModeTables& tables);

// $6E79: the move queues at the start of a life: ghost 1 gets its exit script, ghosts 2 and 4 alternate 1, 0, 1, 0, ... (down/up bounce), ghost 3 alternates 0, 1, ...
void initGhostQueues(GhostQueues& queues, const GhostDecisionData& data);

}  // namespace pacman::core
