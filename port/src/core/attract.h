// TASK-033-4b: the screens of the attract mode, rebuilt from the original program ($52BB, $5260, $43D2-$44E5, $42D0 and the drawing routines they call).
// A screen is a TIMED SCRIPT: every drawing the original does, with the cycle (counted from the entry of $52BB) at which it happens. The cycle counts are not measured data:
// they are summed up instruction by instruction while the script is built (page-crossing and branch-crossing cycles included), the same way the original's routines would
// spend them; tests/test_attract.cpp compares every routine entry of one whole title turn (11,549 events) with the oracle: all equal to the cycle.
// The pictures are made with the verified hires routines (tickdraw.h `executeCall`), the text with the glyph table of the data pack.
#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "../datapack/datapack.h"
#include "../hires/hires.h"
#include "attractmemory.h"
#include "gamedriver.h"
#include "tickdraw.h"

namespace pacman::core {

enum class ScreenOp : std::uint8_t {
    LoresFill,        // lo-res page 2 gets the value `a` in every byte ($52BB: $DD)
    ShowHires,        // the hi-res page 1 is displayed ($6F2F)
    WipeStart,        // the page clear $6F46 starts: the page turns black byte by byte, 8,192 bytes in 90,575 cycles
    Draw,             // one drawing call (tickdraw.h DrawCall)
};

struct ScreenStep {
    std::int64_t at = 0;
    ScreenOp op = ScreenOp::Draw;
    DrawCall call;
    int a = 0;
};

// One routine entry of the original, for the tests (kinds as in tools/export_title_events.py: 1 clear, 2 logo line, 3 blit row, 4 key poll, 5 delay step, 6 image, 7 dot, 8 tile)
struct TraceEvent {
    std::int64_t at = 0;
    int kind = 0;
    std::array<int, 5> p{};
};

struct ScreenScript {
    std::vector<ScreenStep> steps;
    std::int64_t length = 0;            // the cycle at which the next part of the attract loop starts
    std::vector<TraceEvent> trace;      // filled only when asked for
    std::vector<std::pair<std::int64_t, std::int64_t>> busy;   // [start, end) of every drawing routine of the original: inside such a window the original's picture is half drawn
    std::vector<std::int64_t> polls;        // the cycles at which the original calls the key poll $6F5E (TASK-033-4e; always filled)
    std::vector<std::pair<std::int64_t, int>> spaceChecks;  // the cycle of every `LDA $C000` that tests for Space ($5292 after each logo line, $52EE after each delay pass of the lo-res effect) and the cycles from there to the poll's entry when it is taken (19, 12)
};

// A place where the original looks at the keyboard. `at` = the cycle of its first instruction; `toPoll` > 0 = a Space test ($5292, $52EE: only Space acts, the key stays in the latch
// otherwise; taken, the poll $6F5E is entered `toPoll` cycles later), 0 = the key poll $6F5E itself (takes the key out of the latch; Space jumps to the prompt, 36 cycles after the entry).
struct KeyCheck {
    std::int64_t at = 0;
    int toPoll = 0;
};

// The title turn: from the entry of $52BB (lo-res flash, the logo, the typed "Character / Nickname" screen) to the first call of the chase. False if the pack lacks data.
bool buildTitleScript(const datapack::Pack& pack, ScreenScript& out, bool withTrace);

// The title turn and the chase that follows it (the two chase loops and the four ghost-eaten animations), up to the start of the demo game at cycle 22,486,410.
bool buildAttractScript(const datapack::Pack& pack, ScreenScript& out, bool withTrace, const AttractMemory* memory = nullptr);

// The end of the attract game: Pac-Man is caught, the ghosts and he are erased and his six death frames are drawn (no sound), then the loop goes back to the title. Times count from the
// first instruction of the death routine ($8BDD); `length` is the cycle at which the next turn's `JSR $52BB` starts (= its cycle 0). ghosts: x, y of ghosts 1-4, caughtGhost: 1-4.
bool buildDemoDeathScript(const datapack::Pack& pack, ScreenScript& out, const std::array<std::pair<int, int>, 4>& ghosts, int pacX, int pacY, int caughtGhost, bool withTrace,
                          const AttractMemory* memory = nullptr);

// The cycles the original's drawing routine spends on one drawing call, from its entry to its RTS (the instructions of the routine, summed up; types 1-6, 9, 10, 11 of DrawCall;
// 0 for the others). The glue of the call site is not in it.
std::int64_t drawCallCycles(const DrawCall& call, const datapack::Pack& pack);

// The cycles of the routine $550A (the labels and scores of all players and the high score) for `players` players with the given scores (BCD, index = player 1-5), lives bytes ($58:
// bit 7 = the bonus life is still to come) and high score, and the extra it costs over the screen of one player showing the current player's score alone (TASK-033-6b: this is what
// the rebuild of the screen after a catch needs more with several players; the bonus life itself, a score of 10,000 or more with bit 7 still set, is not counted).
std::int64_t hudCycles(const datapack::Pack& pack, int players, const std::array<std::uint32_t, 6>& scores, const std::array<int, 6>& lives, std::uint32_t high);
std::int64_t hudExtraCycles(const datapack::Pack& pack, int players, int current, const std::array<std::uint32_t, 6>& scores, const std::array<int, 6>& lives, std::uint32_t high);

// Plays a script. `advance` moves time forward and applies every step that has become due. The hi-res page is exact at every cycle apart from the drawing
// calls themselves, which take effect at the cycle they start (the original needs up to ~800 cycles to finish a glyph).
class ScreenPlayer {
public:
    void start(const ScreenScript* script, const datapack::Pack* pack, const hires::Page* initial = nullptr);   // `initial`: the page the script draws on (default: black)
    void advance(std::int64_t cycles);
    std::int64_t now() const noexcept { return now_; }
    bool finished() const noexcept { return script_ == nullptr || now_ >= script_->length; }
    bool hiresVisible() const noexcept { return hiresShown_; }
    const hires::Page& hires() const noexcept { return page_; }   // with the page clear applied up to `now`
    std::uint8_t loresByte(std::size_t offset) const noexcept { return offset < lores_.size() ? lores_[offset] : std::uint8_t{0}; }

private:
    void applyWipeUpTo(std::int64_t t);
    const ScreenScript* script_ = nullptr;
    const datapack::Pack* pack_ = nullptr;
    std::size_t next_ = 0;
    std::int64_t now_ = 0;
    hires::Page page_;
    std::array<std::uint8_t, 1024> lores_{};
    bool hiresShown_ = false;
    std::int64_t wipeAt_ = -1;
    std::size_t wiped_ = 0;
};

// One whole turn of the attract loop (TASK-033-4d): the title, the chase and the set-up of the demo game (exact scripts), the demo game itself (the game core playing the
// original's recorded moves; its ticks cost what the cost model of gamedriver.h says), and the end of the demo (exact script), after which the next turn starts at cycle `now()` = 0.
class AttractTurn {
public:
    bool start(const datapack::Pack& pack, const AttractMemory& memory = AttractMemory{});   // the memory the turn starts from (players, scores, high score, level)
    void advance(std::int64_t cycles);
    std::int64_t now() const noexcept { return now_; }
    bool finished() const noexcept { return phase_ == Phase::Done; }
    bool hiresVisible() const noexcept { return phase_ == Phase::Intro ? introPlayer_.hiresVisible() : true; }
    const hires::Page& hires() const noexcept;
    std::uint8_t loresByte(std::size_t offset) const noexcept { return introPlayer_.loresByte(offset); }
    // the demo game
    std::size_t demoTicks() const noexcept { return demoTick_; }
    std::int64_t nextTickStart() const noexcept { return tickStart_; }       // the cycle at which the next tick starts (after runToDemoTick(k): the start of tick k)             // ticks started so far
    bool inDemo() const noexcept { return phase_ == Phase::Demo; }
    std::int64_t demoStart() const noexcept { return introLength_; }          // the cycle of the first tick
    std::int64_t length() const noexcept { return length_; }                  // the length of the turn once it has ended (0 before)
    // The first place AFTER the cycle `after` where the original reads the keyboard (TASK-033-4e); false if there is none left in this turn (the death of the demo reads no key).
    bool nextCheck(std::int64_t after, KeyCheck& out) const;
    // The memory as it stands now (TASK-033-6c): what the next turn starts from when this one ends or is left by a Space. In the demo the blink counters and the penalty counter move on.
    AttractMemory memoryNow() { AttractMemory m = memory_; if (phase_ == Phase::Demo || phase_ == Phase::Death || phase_ == Phase::Done) driver_.core().storeBlinkState(m); return m; }
    // Runs until `k` ticks are complete (hires() is then the page the original shows at the start of tick k). False if the demo ended before.
    bool runToDemoTick(std::size_t k);
    GameDriver& driver() noexcept { return driver_; }

private:
    enum class Phase { Intro, Demo, Death, Done };
    void beginDemo();
    void startTick();
    void beginDeath();
    const datapack::Pack* pack_ = nullptr;
    AttractMemory memory_;
    Phase phase_ = Phase::Intro;
    ScreenScript intro_, death_;
    ScreenPlayer introPlayer_, deathPlayer_;
    GameDriver driver_;
    hires::Page shown_;
    hires::Page pending_;
    std::int64_t now_ = 0;
    std::int64_t introLength_ = 0;
    std::int64_t tickStart_ = 0;     // the start of the tick that is running
    std::int64_t tickEnd_ = 0;
    bool tickRunning_ = false;
    bool tickWasLast_ = false;
    static std::int64_t pollOffset(std::size_t tick) noexcept { return tick == 0 ? 17 : 11; }   // the poll of a tick, from the tick's start (found in the oracle)
    std::size_t demoTick_ = 0;
    std::size_t completed_ = 0;     // ticks whose pictures are shown
    std::int64_t deathStart_ = 0;
    std::int64_t length_ = 0;
};

}  // namespace pacman::core
