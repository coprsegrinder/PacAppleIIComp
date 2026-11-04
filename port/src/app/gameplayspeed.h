// TASK-034-11 (modernization, user request 2026-11-04): Gameplay Speed, next to the FPS setting.
//   Original  the game's time as the original spent it: every tick costs what its drawing and logic cost (2,987 .. 24,463 cycles, mean about 4,870), so the speed dips when a lot is drawn
//             (the slowdowns of the original), and the game stands still while a sound plays that blocked the original's loop.
//   Steady    every ordinary tick takes the same time, the original's mean (kTypicalTickCycles = 4,870 cycles, 210 ticks a second), so the average pace is the original's and the dips
//             are gone. The game's logic is the same, tick for tick: only the time between ticks changes (the driver, the state and the keyboard handling never see the setting).
// What stays as the original has it in both: the deliberate waits (the intro jingle and READY, the death animation and the READY after it, the pause at the end of a level, the cartoons)
// and the stand-still of a ghost being eaten (the points stay on the screen for the length of that sound: it is part of how the game looks). What Steady drops are the hitches: the cost
// differences between ticks (a drawn dot, an energizer's 5,650 extra cycles, ...) and the short stand-stills of the energizer, fruit and bonus-life sounds (9, 88 and 96 ms): those sounds
// still play whole, at the moment of the event, and the next sound waits for the nominal end of the one before it (audio.h), exactly as before.
#pragma once

#include <cstdint>
#include <string>

#include "../core/gamedriver.h"
#include "../core/sound.h"

namespace pacman::app {

enum class GameplaySpeed { Original, Steady };

inline constexpr int kGameplaySpeedCount = 2;

inline const char* gameplaySpeedKey(GameplaySpeed m) noexcept { return m == GameplaySpeed::Steady ? "steady" : "original"; }
inline const char* gameplaySpeedLabel(GameplaySpeed m) noexcept { return m == GameplaySpeed::Steady ? "STEADY" : "ORIGINAL"; }
inline bool parseGameplaySpeed(const std::string& v, GameplaySpeed& out) {
    for (int i = 0; i < kGameplaySpeedCount; ++i)
        if (v == gameplaySpeedKey(static_cast<GameplaySpeed>(i))) {
            out = static_cast<GameplaySpeed>(i);
            return true;
        }
    return false;
}
inline GameplaySpeed nextGameplaySpeed(GameplaySpeed m) noexcept { return m == GameplaySpeed::Original ? GameplaySpeed::Steady : GameplaySpeed::Original; }

// How many cycles beyond the ordinary tick cost the game stands still for a sound. The dot sound is part of the measured mean tick cost (kTypicalTickCycles) and the six death steps are
// part of the measured death sequence (kDeathSequenceCycles); in Original every other sound is added; in Steady the energizer, fruit and bonus-life sounds do not stop the game.
inline std::int64_t soundStandStill(GameplaySpeed speed, core::SoundId id) noexcept {
    switch (id) {
    case core::SoundId::DotEaten:
    case core::SoundId::Death1:
    case core::SoundId::Death2:
    case core::SoundId::Death3:
    case core::SoundId::Death4:
    case core::SoundId::Death5:
    case core::SoundId::Death6:
        return 0;
    case core::SoundId::Energizer:
    case core::SoundId::Fruit:
    case core::SoundId::BonusLife:
        return speed == GameplaySpeed::Steady ? 0 : core::soundFreezeCycles(id);
    default:
        return core::soundFreezeCycles(id);   // the ghost being eaten (and the jingle, which is waited out elsewhere): the stand-still is part of the picture
    }
}

// The time one tick takes: `tick` = the modelled cost of the tick itself, `extra` = the long sequence of that tick (death, level end; 0 for an ordinary tick), `standStill` = sum of
// soundStandStill() of the sounds of the tick.
inline std::int64_t tickCost(GameplaySpeed speed, std::int64_t tick, std::int64_t extra, std::int64_t standStill) noexcept {
    if (speed == GameplaySpeed::Steady && extra == 0) return core::kTypicalTickCycles + standStill;
    return tick + extra + standStill;
}

}  // namespace pacman::app
