#include "gamedriver.h"

namespace pacman::core {

bool GameDriver::init(const datapack::Pack& pack) {
    pack_ = &pack;
    return core_.init(pack);
}

bool GameDriver::newGame() {
    if (!core_.newGame()) return false;
    const auto high = display_.high;   // the high score survives into the next game
    display_ = DisplayState{};
    display_.high = high;
    prev_ = TickExtras{};
    syncExtras();
    rebuildScreen();
    return true;
}

void GameDriver::drawStaticGhosts() {
    const GameState& s = core_.state();
    for (std::size_t g = 0; g < 4; ++g) {
        int flag = 0;
        const std::uint16_t base = ghostImageBase(s, g, 0, *pack_, flag);
        executeCall(page_, DrawCall{1, {base, s.ghosts[g].x(), s.ghosts[g].y(), 1, flag}}, *pack_);
    }
}

void GameDriver::rebuildScreen() {
    const GameState& s = core_.state();
    page_ = hires::Page();
    drawPlayfield(page_, s, *pack_);
    drawHud(page_, s, *pack_, display_.high);
    drawPacman(page_, s, *pack_, false);
    display_.lastFlag1E = 0;
    if (staticGhosts_) drawStaticGhosts();
}

void GameDriver::showReady() {
    executeCall(page_, DrawCall{2, {0x5890, 17, 104, 5, 7}}, *pack_);
    drawPacman(page_, core_.state(), *pack_, false);
}

void GameDriver::applySequenceStep(const SequenceStep& step) {
    for (const auto& c : step.calls) executeCall(page_, c, *pack_);
    if (step.rebuild) rebuildScreen();
    if (step.ready) showReady();
}

namespace {
// Oracle offsets (cycles from the start of the catching tick) of the death sequence, scenario A, tick 1976 (tools/ready_timing.py)
constexpr std::int64_t kOwnDrawing = 14'682;
constexpr std::int64_t kClearAt[5] = {776'432, 778'312, 780'192, 782'074, 783'615};
constexpr std::int64_t kLastImageAt = 785'127, kClearAfterImageAt = 1'562'563;
constexpr std::int64_t kFrameAt[6] = {1'568'093, 1'722'496, 1'877'589, 2'036'066, 2'200'735, 2'368'572};
constexpr std::int64_t kFrameClearAt[6] = {1'716'944, 1'872'037, 2'030'514, 2'195'183, 2'363'020, 2'505'076};
constexpr std::int64_t kRebuildAt = 2'821'980, kReadyAt = 2'927'563;
constexpr std::int64_t kSoundAfterFrame = 1'610;   // the sound of a step starts this long after its frame is drawn
}  // namespace

void GameDriver::buildDeathSequence(const GameState& before) {
    sequence_.clear();
    RestartInputs in;
    in.pacX = before.pacX();
    in.pacY = before.pacY();
    in.pacImage = 0x9BE6;   // $7EC1: LDA #$E6 / LDY #$9B / JSR $6A20 - the death always starts with this one image (frame 6 of the table at $96E8), whatever frame Pac-Man had (oracle: A and L catches)
    for (int g = 0; g < 4; ++g) {
        in.ghostXY[static_cast<std::size_t>(g) * 2] = before.ghosts[g].x();
        in.ghostXY[static_cast<std::size_t>(g) * 2 + 1] = before.ghosts[g].y();
    }
    DisplayState scratchDisplay = display_;
    const auto calls = restartCalls(RestartKind::Death, in, core_.state(), *pack_, scratchDisplay);
    if (calls.size() < 19) return;
    auto step = [&](std::int64_t at, std::size_t from, std::size_t to) {
        SequenceStep s;
        s.at = at - kOwnDrawing;
        s.calls.assign(calls.begin() + static_cast<std::ptrdiff_t>(from), calls.begin() + static_cast<std::ptrdiff_t>(to));
        sequence_.push_back(std::move(s));
    };
    for (std::size_t i = 0; i < 5; ++i) step(kClearAt[i], i, i + 1);   // ghosts 4, 3, 2, 1 and Pac-Man erased
    step(kLastImageAt, 5, 6);                                          // Pac-Man's last image
    step(kClearAfterImageAt, 6, 7);
    for (std::size_t k = 0; k < 6; ++k) {
        step(kFrameAt[k], 7 + 2 * k, 8 + 2 * k);
        sequence_.back().deathSound = 6 - static_cast<int>(k);
        step(kFrameClearAt[k], 8 + 2 * k, 9 + 2 * k);
    }
    SequenceStep rebuild;
    rebuild.at = kRebuildAt - kOwnDrawing;
    rebuild.rebuild = true;
    sequence_.push_back(rebuild);
    SequenceStep ready;
    ready.at = kReadyAt - kOwnDrawing;
    ready.ready = true;
    sequence_.push_back(ready);
    SequenceStep end;   // the READY wait is over: the page of the first tick (no READY) and the first tick follows
    end.at = kDeathSequenceCycles - kOwnDrawing;
    end.rebuild = true;
    sequence_.push_back(end);
}

TickEvents GameDriver::tick(FrightenAware& ghosts) { return tickImpl(&ghosts); }

TickEvents GameDriver::tick() { return tickImpl(nullptr); }

TickEvents GameDriver::tickImpl(FrightenAware* fed) {
    if (newGamePending_) {   // the pause after GAME OVER is over
        newGamePending_ = false;
        newGame();
        extraCycles_ = 0;
        tickCycles_ = kTypicalTickCycles;
        return TickEvents{};
    }
    const GameState before = core_.state();
    const bool acting = before.zp(0x59) == 1;
    TickEvents ev = fed != nullptr ? core_.tick(*fed) : core_.tick();
    extraCycles_ = 0;
    tickCycles_ = kTypicalTickCycles;
    lastCalls_.clear();
    sequence_.clear();
    if (ev.paused) return ev;
    const GameState& after = core_.state();
    TickExtras now;
    if (fed != nullptr) {
        fed->frightenState(now.e1, now.e2, now.e4);
    } else {
        now.e1 = core_.ghostShared().e1;
        now.e2 = core_.ghostShared().e2;
        now.e4 = core_.ghostShared().e4;
    }
    now.e5 = core_.blink1UpCounter();
    now.e6 = core_.blink1UpPhase();
    now.e7 = core_.fruitState();
    if (ev.gameOver) {   // the last life: the words GAME OVER ($811D, $8139) on the playfield, then a new game after a pause (no death animation yet)
        executeCall(page_, DrawCall{2, {0x811D, 15, 105, 4, 7}}, *pack_);
        executeCall(page_, DrawCall{2, {0x8139, 21, 105, 4, 7}}, *pack_);
        extraCycles_ = kDeathSequenceCycles;
        newGamePending_ = true;
        return ev;
    }
    if (ev.caught || ev.levelComplete) {
        // the high score follows the score as the score routine ($7091) would have done; the screen is rebuilt from the state
        const std::uint32_t sc = after.zp(0x52) | (after.zp(0x53) << 8) | (after.zp(0x54) << 16);
        if (sc >= (static_cast<std::uint32_t>(display_.high[0]) | (display_.high[1] << 8) | (display_.high[2] << 16))) {
            display_.high = {after.zp(0x52), after.zp(0x53), after.zp(0x54)};
        }
        if (playDeath_ && ev.caught && !ev.levelComplete) {   // the live game plays the animation (sequence()); the page stays as it was until then
            buildDeathSequence(core_.caughtState());
            if (!sequence_.empty()) {
                prev_ = now;
                return ev;
            }
        }
        extraCycles_ = ev.levelComplete ? kLevelEndCycles : kDeathSequenceCycles;   // level end: measured in 033-1b
        rebuildScreen();
        prev_ = now;
        return ev;
    }
    const auto calls = tickCalls(before, after, *pack_, acting && ev.acted, display_, prev_, now);
    lastCalls_ = calls;
    tickCycles_ = tickCostCycles(calls);
    for (const auto& c : calls) executeCall(page_, c, *pack_);
    prev_ = now;
    return ev;
}

}  // namespace pacman::core
