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

TickEvents GameDriver::tick(FrightenAware& ghosts) { return tickImpl(&ghosts); }

TickEvents GameDriver::tick() { return tickImpl(nullptr); }

TickEvents GameDriver::tickImpl(FrightenAware* fed) {
    if (newGamePending_) {   // the pause after GAME OVER is over
        newGamePending_ = false;
        newGame();
        extraCycles_ = 0;
        return TickEvents{};
    }
    const GameState before = core_.state();
    const bool acting = before.zp(0x59) == 1;
    TickEvents ev = fed != nullptr ? core_.tick(*fed) : core_.tick();
    extraCycles_ = 0;
    lastCalls_.clear();
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
        extraCycles_ = ev.levelComplete ? kLevelEndCycles : kDeathSequenceCycles;   // level end: measured in 033-1b
        rebuildScreen();
        prev_ = now;
        return ev;
    }
    const auto calls = tickCalls(before, after, *pack_, acting && ev.acted, display_, prev_, now);
    lastCalls_ = calls;
    for (const auto& c : calls) executeCall(page_, c, *pack_);
    prev_ = now;
    return ev;
}

}  // namespace pacman::core
