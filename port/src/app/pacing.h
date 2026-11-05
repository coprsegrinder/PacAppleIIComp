// TASK-034-11 (modernization): how long the game's ticks last in real time, for the modern FPS modes (30, 60, 120, 144, Unlocked). Native keeps the original's timing exactly as it was.
//
// WHY THE GAME'S TICKS HAVE DIFFERENT LENGTHS (found by investigation, 2026-11-04): the original's pace is not "one tick = one fixed time". A tick takes as long as the drawing it does
// (2,987 cycles when nothing is drawn, about 7,000 when something moves, about 16,000 when a dot is eaten), and the original's logic is TUNED TO THAT: Pac-Man's step countdown is 9 ticks
// (level 1), plus one for each frightened ghost among ghosts 2 and 4 = 11 ticks while the ghosts are blue, while the ticks get about 17 % shorter. A fixed tick time makes Pac-Man about 20 %
// slower at a power pill.
//
// WHAT THE MODERN MODES DO (user, 2026-11-05: the gameplay speed is constant, independent of the rendering and of recent computational costs): NOTHING IS MEASURED OR LEARNED. The length of
// a tick is a constant (kModernTickCycles, the original's mean cost of an ordinary tick in calm play) times  (the countdown Pac-Man's step would have without the ghosts' effects) /
// (the countdown the game gave his step): a step takes the same real time, calm or at a power pill, eating or not, whatever the ticks of the original cost. The countdown is the game's own
// state ($59 after the tick in which he acted; the effect of the ghosts is read before that tick, as the reload sees it): a rule of the game, not a measurement. The ghosts keep their own
// rule (their delay tables are in ticks). The cycles here are the Apple II's (the model of the original's timing); the host's speed does not enter.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../core/gamedriver.h"
#include "../core/sound.h"

namespace pacman::app {

// The mean cost of the original's ordinary ticks in calm play (the per-second means of real play, level 1: 4,961 .. 4,998; the average of those runs). Fixed.
inline constexpr std::int64_t kModernTickCycles = 4'960;

class SmoothPacer {
public:
    void reset() noexcept { calm_ = now_ = 1; }
    // Pac-Man's step countdown was loaded in the tick that just ran: `countdown` = the ticks it gave him ($59 after the tick: reload, the extra tick of a recent meal and the delay of the
    // item eaten in that tick), `ghostEffect` = what the ghosts' states added to the reload (reloadValue - calmReloadValue, as before that tick). The step is then paced as if the ghosts'
    // effect were not there.
    void startStep(unsigned countdown, int ghostEffect) noexcept {
        now_ = (std::max)(1u, countdown);
        calm_ = (std::max)(1, static_cast<int>(now_) - ghostEffect);
    }
    // The length of a tick of the step in progress.
    std::int64_t duration() const noexcept { return static_cast<std::int64_t>(std::llround(static_cast<double>(kModernTickCycles) * static_cast<double>(calm_) / static_cast<double>(now_))); }

private:
    int calm_ = 1;
    int now_ = 1;
};

// How many cycles beyond the ordinary tick cost the game stands still for a sound. The dot sound is part of the measured tick cost and the six death steps are part of the measured death
// sequence; in the original every other sound is added. In the modern modes the energizer, fruit and bonus-life sounds (9, 88 and 96 ms) do not stop the game: they still play whole at the
// moment of the event. What stays in all modes: the stand-still of a ghost being eaten (the points stay on the screen for the length of that sound: it is part of the picture).
inline std::int64_t soundStandStill(bool modern, core::SoundId id) noexcept {
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
        return modern ? 0 : core::soundFreezeCycles(id);
    default:
        return core::soundFreezeCycles(id);
    }
}


// The time one tick takes. `tick` = the modelled cost of the tick itself (what the original spent), `extra` = the long sequence of that tick (death, level end; 0 for an ordinary tick),
// `standStill` = the sum of soundStandStill() of its sounds. Native: the original's cost. The modern modes: the pace of the step in progress (SmoothPacer).
inline std::int64_t tickCost(bool modern, const SmoothPacer& pacer, std::int64_t tick, std::int64_t extra, std::int64_t standStill) noexcept {
    if (modern && extra == 0) return pacer.duration() + standStill;
    return tick + extra + standStill;
}

}  // namespace pacman::app
