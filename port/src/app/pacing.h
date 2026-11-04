// TASK-034-11 (modernization): how long the game's ticks last in real time, for the modern FPS modes (30, 60, 120, 144, Unlocked). Native keeps the original's timing exactly as it was.
//
// WHY THE GAME'S TICKS HAVE DIFFERENT LENGTHS, AND WHY A FIXED TICK TIME IS WRONG (found by investigation, 2026-11-04):
// The original's pace is not "one tick = one fixed time". A tick takes as long as the drawing it does (2,987 cycles when nothing is drawn, about 7,000 when something moves, about 16,000
// when a dot is eaten), and the original's logic is TUNED TO THAT: Pac-Man's step countdown is reloaded with 9 ticks (level 1), plus one for each frightened ghost among ghosts 2 and 4 =
// 11 ticks while the ghosts are blue (docs/PACMAN_RULES.md: 797 of 797 reloads matched). Blue ghosts are drawn less often, so the ticks of that phase are about 17 % shorter (4,100 against
// 4,900 cycles); the longer countdown and the shorter ticks cancel: Pac-Man's real-time speed stays about the same when a power pill is eaten (measured: 21 steps a second before, 21 after).
// A fixed tick time (the first "Steady") removed the shorter ticks but kept the longer countdown: Pac-Man became about 20 % slower the moment a power pill was eaten. Every fixed number is
// wrong in some situation; that is why neither 4,870 nor 3,650 cycles felt right.
//
// WHAT THE MODERN MODES DO INSTEAD: they keep the original's average pace in every situation but take out the hitches. The tick time is the running average of the cost the original's
// ticks have (an exponential average over about 128 ticks = 0.6 s): a dot eaten (16 ms) no longer stops the game for a frame, and when the situation changes (the ghosts turn blue) the tick time
// follows the original's within a fraction of a second, so the speeds that the original's logic relies on stay in balance. The game's logic is untouched (it never sees the tick time).
#pragma once

#include <cstdint>

#include "../core/gamedriver.h"
#include "../core/sound.h"

namespace pacman::app {

class SmoothPacer {
public:
    static constexpr int kWindowTicks = 128;   // the average forgets the older ticks over about this many ticks
    void reset() noexcept { ema256_ = core::kTypicalTickCycles * 256; }
    void observe(std::int64_t tickCycles) noexcept { ema256_ += (tickCycles * 256 - ema256_) / kWindowTicks; }
    std::int64_t duration() const noexcept { return (ema256_ + 128) / 256; }

private:
    std::int64_t ema256_ = core::kTypicalTickCycles * 256;
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
// `standStill` = the sum of soundStandStill() of its sounds. The pacer sees every ordinary tick (so it is ready when a modern mode is chosen in the middle of a game).
inline std::int64_t tickCost(bool modern, SmoothPacer& pacer, std::int64_t tick, std::int64_t extra, std::int64_t standStill) noexcept {
    if (extra == 0) pacer.observe(tick);
    if (modern && extra == 0) return pacer.duration() + standStill;
    return tick + extra + standStill;
}

}  // namespace pacman::app
