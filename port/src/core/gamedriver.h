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

// TASK-033-4d: the cost of a tick = the idle tick + what the drawing routines spend (exact: attract.h drawCallCycles, summed up from the original's instructions) + a fitted
// "glue" per call and a few logic extras. Found on the 7,893 ticks of the attract game, measured against the oracle (tools/record_demo_full.py, tests/test_attract.cpp):
//  * a tick without any drawing costs EXACTLY 2,927 cycles (4,757 of 4,757 ticks);
//  * for a tick with drawing the remaining difference is 2,939 + per call {type 1: -106, 2: 66, 3: 609, 5: 134, 6: 92, 10: 40, 11: 59} + 144 when a dot is eaten + 5,650 when an
//    energizer is eaten; standard deviation 111 cycles per tick; over the whole demo the model is off by 1,094 cycles of 36.6 million (0.003 %). Not exact per tick: the logic of
//    the ghosts and of Pac-Man takes a few different paths (I). The real game's keyboard poll is dearer by 60 cycles (scenario A's cheapest tick is 2,987) (I, not fitted).
inline constexpr std::int64_t kIdleTickCyclesDemo = 2'927;
inline constexpr std::int64_t kIdleTickCyclesGame = 2'987;
// The tick in which Pac-Man is caught costs 6,273 to 18,916 cycles up to the end of its drawing (mean 8,917 over 245 deaths of the oracle); the death sequence counts from there.
inline constexpr std::int64_t kCatchTickCycles = 8'917;
inline constexpr std::int64_t kDemoCatchTickCycles = 3'996;      // the attract game's last tick up to the start of the death routine at $8BDD (that tick is always the same)
inline constexpr std::int64_t kDemoGhostEatenTickCycles = 277'633;   // the attract game's one ghost-eaten tick (277,633 in the first turn of a boot, 277,723 in the turn after a 3-player game: the sequence of $7DB8 is not counted yet; the animation with its delay instead of the sound)
std::int64_t tickCostCycles(const std::vector<DrawCall>& calls, const datapack::Pack& pack, bool demo, bool ate, bool energizer);

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
    bool newGame(int players = 1, int rank = 1);   // players 1-5 (TASK-033-6), every player starts at level `rank`                        // new core state and the first screen
    bool newDemo(const hires::Page& screen, const AttractMemory* memory = nullptr);   // the attract game: its first tick's state on the given screen (the screen the set-up has drawn)
    TickEvents tick(FrightenAware& ghosts);   // the ghosts are supplied by the caller (tests)
    TickEvents tick();                        // the ghosts of the game core (M5)
    void syncExtras() noexcept {              // after the shared ghost variables were replaced from outside
        prev_.e1 = core_.ghostShared().e1;
        prev_.e2 = core_.ghostShared().e2;
        prev_.e4 = core_.ghostShared().e4;
        prev_.e5 = core_.blink1UpCounter();   // the blink counters the core starts with (the attract game after a game: TASK-033-6c)
        prev_.e6 = core_.blink1UpPhase();
        prev_.e7 = core_.fruitState();
    }
    void nextLevel() { core_.startNextLevel(); rebuildScreen(); }   // see GameCore::startNextLevel
    void rebuildScreen();                  // the page from the state: walls, door, dots, labels, score, high score, icons, Pac-Man (no ghosts)
    GameCore& core() noexcept { return core_; }
    // The memory the attract game starts from after this game, with the high score of the game's display (TASK-033-6c)
    AttractMemory attractMemory(const AttractMemory& before) const { AttractMemory m = core_.attractMemory(before); m.high = display_.high; return m; }
    const hires::Page& page() const noexcept { return page_; }
    // For the smooth motion of the window (src/app/motion.h): the page without the images of Pac-Man and the ghosts (the same page, minus what the sprite draws of the ordinary ticks put
    // on it), where the actors moved to in the last tick, and whether the last tick was an ordinary one (false: the page was rebuilt or drawn by a sequence; the window shows it as it is).
    struct ActorMove {
        int actor, x, y;
        hires::Page image;   // the actor's image alone, drawn on an empty page: which pixels are the sprite's (and not the background's)
        hires::Page solo;    // the background with this actor's image alone drawn on it (no other actor): what the picture is by this actor's draw, whoever else stands near
    };
    const hires::Page& backgroundPage() const noexcept { return bgPage_; }
    const std::vector<ActorMove>& lastMoves() const noexcept { return moves_; }
    bool interpolatable() const noexcept { return interpOk_; }
    DisplayState& display() noexcept { return display_; }
    void setStaticGhosts(bool on) noexcept { staticGhosts_ = on; }    // live program without ghost logic: draw the four ghosts once at their start places
    void setPreviousFrighten(std::uint8_t e1, std::uint8_t e2, std::uint8_t e4) noexcept { prev_.e1 = e1; prev_.e2 = e2; prev_.e4 = e4; }
    const std::vector<DrawCall>& lastCalls() const noexcept { return lastCalls_; }   // the drawing calls of the last ordinary tick
    void setPlayDeath(bool on) noexcept { playDeath_ = on; }   // live game: a catch leaves the page as it was and fills sequence() instead of rebuilding at once
    const std::vector<SequenceStep>& sequence() const noexcept { return sequence_; }
    bool gameOverPending() const noexcept { return newGamePending_; }   // the last life is lost: after the sequence the program returns to the attract loop (033-4g)
    void applySequenceStep(const SequenceStep& step);
    void showReady();    // the READY text (and Pac-Man again) on the page, as the original draws it after the rebuild
    std::int64_t tickCycles() const noexcept { return tickCycles_ + inputCycles_; }   // modelled cost of the last tick itself (tickCostCycles; kTypicalTickCycles for a special tick)
    std::int64_t extraCycles() const noexcept { return extraCycles_; }   // cycles of the long sequence of the last tick (0 for an ordinary tick)

private:
    void drawStaticGhosts();
    TickEvents tickImpl(FrightenAware* fed);
    void buildDeathSequence(const GameState& caught, bool gameOver);
    const datapack::Pack* pack_ = nullptr;
    GameCore core_;
    hires::Page page_;
    hires::Page bgPage_;                // page_ without the actors' images (see backgroundPage())
    std::vector<ActorMove> moves_;
    bool interpOk_ = false;
    void resyncBackground() { bgPage_ = page_; interpOk_ = false; moves_.clear(); }
    DisplayState display_;
    TickExtras prev_;
    bool staticGhosts_ = false;
    std::int64_t extraCycles_ = 0;
    std::int64_t tickCycles_ = kTypicalTickCycles;
    std::int64_t inputCycles_ = 0;   // joystick mode: the paddle timing of the tick's input poll (TickEvents::inputCycles)
    bool playDeath_ = false;
    std::vector<SequenceStep> sequence_;
    bool newGamePending_ = false;   // after GAME OVER the next tick starts a new game
    std::vector<DrawCall> lastCalls_;
};

}  // namespace pacman::core
