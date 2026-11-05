// The facts about one tick of a real game that the achievements are checked with (achievements.h), taken from the driver and the tick's events. Read-only: nothing of the game is changed.
// `level` and `player` are what the tick was played in (read before it: a level change or a change of player happens inside the tick), `scoreBefore` the score before it, `cycles` its cost.
#pragma once

#include <cstdint>

#include "achievements.h"
#include "gamedriver.h"

namespace pacman::core {

inline TickFacts tickFacts(GameDriver& drv, const TickEvents& ev, int level, int player, std::uint32_t scoreBefore, std::int64_t cycles) {
    const GameState& st = drv.core().state();
    TickFacts f;
    f.player = player;
    f.level = level;
    f.score = ev.playerSwitched ? scoreBefore : st.score();   // (after a catch that hands over to another player the state is that player's)
    f.acted = ev.acted;
    f.pacX = st.pacX();
    f.pacY = st.pacY();
    f.pellet = ev.ate && !ev.energizer;
    f.energizer = ev.energizer;
    for (const bool g : ev.ghostEaten) f.ghostsEaten += g ? 1 : 0;
    f.frightened = drv.core().ghostShared().e4 != 0;
    for (const SoundId id : ev.sounds) {
        if (id == SoundId::Fruit) f.fruitEaten = true;
        if (id == SoundId::BonusLife) f.bonusLife = true;
    }
    f.caught = ev.caught;
    f.levelComplete = ev.levelComplete;
    f.cycles = cycles;
    return f;
}

}  // namespace pacman::core
