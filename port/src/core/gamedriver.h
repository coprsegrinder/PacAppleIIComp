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
inline constexpr std::int64_t kTypicalTickCycles = 4'870;   // mean cost of an ordinary tick (scenario A, 3,000 ticks without the death); refined in M8

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
    void nextLevelUnverified() { core_.startNextLevelUnverified(); rebuildScreen(); }   // see GameCore::startNextLevelUnverified
    void rebuildScreen();                  // the page from the state: walls, door, dots, labels, score, high score, icons, Pac-Man (no ghosts)
    GameCore& core() noexcept { return core_; }
    const hires::Page& page() const noexcept { return page_; }
    DisplayState& display() noexcept { return display_; }
    void setStaticGhosts(bool on) noexcept { staticGhosts_ = on; }    // live program without ghost logic: draw the four ghosts once at their start places
    void setPreviousFrighten(std::uint8_t e1, std::uint8_t e2, std::uint8_t e4) noexcept { prev_.e1 = e1; prev_.e2 = e2; prev_.e4 = e4; }
    const std::vector<DrawCall>& lastCalls() const noexcept { return lastCalls_; }   // the drawing calls of the last ordinary tick
    std::int64_t extraCycles() const noexcept { return extraCycles_; }   // cycles of the long sequence of the last tick (0 for an ordinary tick)

private:
    void drawStaticGhosts();
    TickEvents tickImpl(FrightenAware* fed);
    const datapack::Pack* pack_ = nullptr;
    GameCore core_;
    hires::Page page_;
    DisplayState display_;
    TickExtras prev_;
    bool staticGhosts_ = false;
    std::int64_t extraCycles_ = 0;
    bool newGamePending_ = false;   // after GAME OVER the next tick starts a new game
    std::vector<DrawCall> lastCalls_;
};

}  // namespace pacman::core
