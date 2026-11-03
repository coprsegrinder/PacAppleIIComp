// TASK-029h: GameCore + the drawing of every tick on a hi-res page. One call of tick() = one pass of the original main loop with its screen output:
// the core step (gamecore.h), then the drawing calls of the tick (tickdraw.h) executed on the page. A caught Pac-Man or a completed maze replaces the page by the
// rebuilt screen (the death animation and the maze flash are available as call lists from restartCalls() and are not played here).
#pragma once

#include <cstdint>
#include <vector>

#include "gamecore.h"
#include "screen.h"
#include "tickdraw.h"

namespace pacman::core {

// A ghost source may also say what the frightened-ghost timers outside the logical state are ($E1/$E2, $E4): the drawing needs them (blue / white frames).
class FrightenAware : public GhostSource {
public:
    virtual void frightenState(std::uint8_t& e1, std::uint8_t& e2, std::uint8_t& e4) const { e1 = e2 = e4 = 0; }
};

// Cycles the original spends on the long sequences (measured with tools/tick_cycles.py on scenario A): the death animation and the pause before the new life.
inline constexpr std::int64_t kDeathSequenceCycles = 6'043'263;
// TASK-033-2: the wait after the last drawing of a restart screen until the first tick, measured on the oracle (tools/ready_timing.py): 3,114,542 cycles = 3.05 s, the SAME
// at the start of a game (after the 5.15M of the jingle) and after a death. So a death sequence = the animation (2,928,721 cycles until its last drawing) + this wait.
inline constexpr std::int64_t kReadyPauseCycles = 3'114'542;
inline constexpr std::int64_t kDeathAnimationCycles = kDeathSequenceCycles - kReadyPauseCycles;   // 2,928,721, matches the oracle's last drawing at 2,928,721
// The level end (the pause after the last dot until the next level starts, without a cartoon): the tick-to-tick cost around the level change was 4,334,935 cycles
// (levels 3 and 12; 4,334,936 on one run) = 4.24 s, minus the cost of one ordinary tick (tools/level_end_cycles.py, TASK-033-1b). V for the gross figure; the ~4,870 deduction is approximate.
inline constexpr std::int64_t kLevelEndCycles = 4'330'065;
inline constexpr std::int64_t kTypicalTickCycles = 4'870;   // mean cost of an ordinary tick (scenario A, 3,000 ticks without the death); refined in M8

// TASK-033-1c: the cost of an ordinary tick from what it draws. Fitted (least squares) to the oracle's per-tick cycle counts of scenarios A, B, C, D (5,231 ticks, the ones
// with call logs): 3,033 cycles + per call of type 1..6 {1,458; 1,789; 2,026; 3,745; 377; 864}. Fit quality: R2 0.970, residual 566 cycles per tick (a flat 4,870 gives 3,279);
// held-out scenario totals within 0.6 % (flat: up to 4.5 %). Types 7-12 are not in the fit (0). An approximation, not the original's exact per-tick count (I).
inline constexpr std::int64_t kTickBaseCycles = 3'033;
inline constexpr std::int64_t kCallTypeCycles[7] = {0, 1'458, 1'789, 2'026, 3'745, 377, 864};
inline std::int64_t tickCostCycles(const std::vector<DrawCall>& calls) {
    std::int64_t c = kTickBaseCycles;
    for (const auto& d : calls)
        if (d.type >= 1 && d.type <= 6) c += kCallTypeCycles[d.type];
    return c;
}

// TASK-033-3: the death animation as a timed list of steps (opt-in, setPlayDeath). Times are cycles after the end of the catching tick (the oracle's offsets from the tick
// start minus the 14,682 cycles of that tick's own drawing; tools/ready_timing.py). The calls are the first 19 of restartCalls(Death): 4 ghost clears, Pac-Man clear, Pac-Man's
// last image, clear, then six death frames each followed by a clear. After the frames the screen is rebuilt and READY is shown; the last step ends the sequence.
struct SequenceStep {
    std::int64_t at = 0;
    std::vector<DrawCall> calls;
    bool rebuild = false;   // replace the page by the rebuilt screen of the new life (no READY)
    bool ready = false;     // draw the READY text
    int deathSound = 0;     // 6..1: the death sound step that starts with this step (0 = none)
};

class GameDriver {
public:
    bool init(const datapack::Pack& pack);
    bool newGame();                        // new core state and the first screen
    TickEvents tick(FrightenAware& ghosts);   // the ghosts are supplied by the caller (tests)
    TickEvents tick();                        // the ghosts of the game core (M5)
    void syncExtras() noexcept {              // after the shared ghost variables were replaced from outside
        prev_.e1 = core_.ghostShared().e1;
        prev_.e2 = core_.ghostShared().e2;
        prev_.e4 = core_.ghostShared().e4;
    }
    void nextLevel() { core_.startNextLevel(); rebuildScreen(); }   // see GameCore::startNextLevel
    void rebuildScreen();                  // the page from the state: walls, door, dots, labels, score, high score, icons, Pac-Man (no ghosts)
    GameCore& core() noexcept { return core_; }
    const hires::Page& page() const noexcept { return page_; }
    DisplayState& display() noexcept { return display_; }
    void setStaticGhosts(bool on) noexcept { staticGhosts_ = on; }    // live program without ghost logic: draw the four ghosts once at their start places
    void setPreviousFrighten(std::uint8_t e1, std::uint8_t e2, std::uint8_t e4) noexcept { prev_.e1 = e1; prev_.e2 = e2; prev_.e4 = e4; }
    const std::vector<DrawCall>& lastCalls() const noexcept { return lastCalls_; }   // the drawing calls of the last ordinary tick
    void setPlayDeath(bool on) noexcept { playDeath_ = on; }   // live game: a catch leaves the page as it was and fills sequence() instead of rebuilding at once
    const std::vector<SequenceStep>& sequence() const noexcept { return sequence_; }
    void applySequenceStep(const SequenceStep& step);
    void showReady();    // the READY text (and Pac-Man again) on the page, as the original draws it after the rebuild
    std::int64_t tickCycles() const noexcept { return tickCycles_; }   // modelled cost of the last tick itself (tickCostCycles; kTypicalTickCycles for a special tick)
    std::int64_t extraCycles() const noexcept { return extraCycles_; }   // cycles of the long sequence of the last tick (0 for an ordinary tick)

private:
    void drawStaticGhosts();
    TickEvents tickImpl(FrightenAware* fed);
    void buildDeathSequence(const GameState& before);
    const datapack::Pack* pack_ = nullptr;
    GameCore core_;
    hires::Page page_;
    DisplayState display_;
    TickExtras prev_;
    bool staticGhosts_ = false;
    std::int64_t extraCycles_ = 0;
    std::int64_t tickCycles_ = kTypicalTickCycles;
    bool playDeath_ = false;
    std::vector<SequenceStep> sequence_;
    bool newGamePending_ = false;   // after GAME OVER the next tick starts a new game
    std::vector<DrawCall> lastCalls_;
};

}  // namespace pacman::core
