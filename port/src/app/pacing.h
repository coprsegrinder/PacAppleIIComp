// TASK-034-11 (modernization): how long the game's ticks last in real time, for the modern FPS modes (30, 60, 120, 144, Unlocked). Native keeps the original's timing exactly as it was.
//
// WHY THE GAME'S TICKS HAVE DIFFERENT LENGTHS, AND WHY A FIXED TICK TIME IS WRONG (found by investigation, 2026-11-04):
// The original's pace is not "one tick = one fixed time". A tick takes as long as the drawing it does (2,987 cycles when nothing is drawn, about 7,000 when something moves, about 16,000
// when a dot is eaten), and the original's logic is TUNED TO THAT: Pac-Man's step countdown is reloaded with 9 ticks (level 1), plus one for each frightened ghost among ghosts 2 and 4 =
// 11 ticks while the ghosts are blue (docs/PACMAN_RULES.md: 797 of 797 reloads matched). Blue ghosts are drawn less, so the ticks of that phase are about 17 % shorter; the longer
// countdown and the shorter ticks nearly cancel. A fixed tick time (the first "Steady") removed the shorter ticks but kept the longer countdown: Pac-Man became about 20 % slower at a
// power pill. Every fixed number is wrong in some situation.
//
// WHAT THE MODERN MODES DO (user, 2026-11-05: Pac-Man's pace and the pellets must not change when a power pill is eaten, in every mode but Native): the time of a tick is
//     (the running average of what the original's ticks cost while nothing changes the countdown)  x  (the calm countdown / the countdown now),
// so Pac-Man's step takes exactly the same real time whatever the ghosts are doing (calm: 9 x t; blue: 11 x t x 9/11 = 9 x t), and the pace of the game as a whole is the original's
// average. The ghosts keep their own rule (their delay tables are in ticks: blue ghosts stay about half as fast as calm ones, relative to Pac-Man exactly as the original's logic says).
// The average is taken over about 128 ticks (0.6 s) of calm play, so a dot eaten (16 ms) no longer stops the game for a frame. The game's logic never sees the tick time.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../core/gamedriver.h"
#include "../core/sound.h"

namespace pacman::app {

class SmoothPacer {
public:
    static constexpr int kWindowTicks = 128;   // the average forgets the older ticks over about this many ticks
    void reset() noexcept {
        calm256_ = core::kTypicalTickCycles * 256;
        extra_ = 0.0;
        lastReload_ = 0;
    }
    // `tickCycles` = what the original spent on the tick; only ticks with nothing changing the countdown (reloadNow == reloadCalm) teach the average (the others are the original's
    // compensation for its own shorter ticks, which this pacing replaces).
    void observe(std::int64_t tickCycles, unsigned reloadNow, unsigned reloadCalm) noexcept {
        if (reloadNow == reloadCalm) calm256_ += (tickCycles * 256 - calm256_) / kWindowTicks;
    }
    // Pac-Man acted (his step countdown ran out): `ticksSincePrev` = the ticks since he acted the time before, `reloadNow` = the countdown he was given. The ticks of a step beyond the
    // countdown are what his eating adds (the delay of the item, one more tick on the reloads after it): learned, an average over about 8 steps.
    void act(unsigned ticksSincePrev, unsigned reloadNow) noexcept {
        if (lastReload_ != 0) {
            const double sample = static_cast<double>(ticksSincePrev) - static_cast<double>(lastReload_);
            extra_ += (std::clamp(sample, 0.0, 12.0) - extra_) / 8.0;
        }
        lastReload_ = reloadNow;
    }
    // The tick time that gives Pac-Man's whole step (countdown and eating delay) the same real time as in calm play: the calm tick average times
    // (calm countdown + eating ticks) / (countdown now + eating ticks).
    std::int64_t duration(unsigned reloadNow, unsigned reloadCalm) const noexcept {
        if (reloadNow == 0 || reloadCalm == 0) return (calm256_ + 128) / 256;
        const double ratio = (static_cast<double>(reloadCalm) + extra_) / (static_cast<double>(reloadNow) + extra_);
        return static_cast<std::int64_t>(static_cast<double>(calm256_) * ratio / 256.0 + 0.5);
    }
    double eatingTicks() const noexcept { return extra_; }

private:
    std::int64_t calm256_ = core::kTypicalTickCycles * 256;
    double extra_ = 0.0;
    unsigned lastReload_ = 0;
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
// `standStill` = the sum of soundStandStill() of its sounds, `reloadNow` / `reloadCalm` = Pac-Man's step countdown now and without ghost effects (StepTimer::reloadValue). The pacer sees
// every ordinary tick (so it is ready when a modern mode is chosen in the middle of a game).
inline std::int64_t tickCost(bool modern, SmoothPacer& pacer, std::int64_t tick, std::int64_t extra, std::int64_t standStill, unsigned reloadNow, unsigned reloadCalm) noexcept {
    if (extra == 0) pacer.observe(tick, reloadNow, reloadCalm);
    if (modern && extra == 0) return pacer.duration(reloadNow, reloadCalm) + standStill;
    return tick + extra + standStill;
}

}  // namespace pacman::app
