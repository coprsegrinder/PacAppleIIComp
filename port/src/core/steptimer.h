// Pac-Man's step timer (TASK-029d1): the countdown $59 that decides in which ticks Pac-Man (and the mode tick) act, rebuilt from $7861-$789B and the
// eating code $7AAC-$7ABB, checked against the oracle recordings (docs/PACMAN_RULES.md has the rule; docs/M4_PLAN.md the test).
//   every tick:   $59 := $59 - 1; if it is not zero nothing else happens in this part of the tick;
//   when it is 0: the timer is reloaded:  base[min(level,15)] + 1 for each frightened flag of ghosts 2 and 4 - 1 for each eyes flag of ghosts 1 and 3
//                 + 1 if the dot penalty counter $F3 is non-zero (and $F3 is decremented);
//   when Pac-Man eats a dot or energizer (later in the same acting tick): $59 += penalty[min(level,15)] at once and $F3 := f3Start[min(level,15)].
// $F3 is not part of the logical state of the golden traces, so the StepTimer keeps it.
#pragma once

#include <array>
#include <cstdint>

#include "../datapack/datapack.h"
#include "gamestate.h"

namespace pacman::core {

struct StepTables {
    std::array<std::uint8_t, 16> base{};      // $7851: reload value per level class (index = min(level, 15))
    std::array<std::uint8_t, 16> penalty{};   // $7A8C: immediate delay when an item is eaten
    std::array<std::uint8_t, 16> f3Start{};   // $7A9C: how many later reloads get one extra tick
};

// False (tables untouched) if the pack lacks one of the three tables.
bool loadStepTables(const datapack::Pack& pack, StepTables& out);

class StepTimer {
public:
    // One pass of $7861-$789B. Returns true when the timer reached zero (Pac-Man acts in this tick). The ghost flags are read from the state.
    bool tick(GameState& state, const StepTables& tables);
    // The eating code $7AAC-$7ABB: call once for every dot or energizer eaten (Pac-Man acts, so it follows a tick() that returned true).
    void itemEaten(GameState& state, const StepTables& tables);
    std::uint8_t f3() const noexcept { return f3_; }

private:
    std::uint8_t f3_ = 0;
};

}  // namespace pacman::core
