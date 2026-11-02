#include "gamedriver.h"

namespace pacman::core {

bool GameDriver::init(const datapack::Pack& pack) {
    pack_ = &pack;
    return core_.init(pack);
}

bool GameDriver::newGame() {
    if (!core_.newGame()) return false;
    display_ = DisplayState{};
    prev_ = TickExtras{};
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

TickEvents GameDriver::tick(FrightenAware& ghosts) {
    const GameState before = core_.state();
    const bool acting = before.zp(0x59) == 1;
    TickEvents ev = core_.tick(ghosts);
    extraCycles_ = 0;
    lastCalls_.clear();
    if (ev.paused) return ev;
    const GameState& after = core_.state();
    TickExtras now;
    ghosts.frightenState(now.e1, now.e2, now.e4);
    now.e5 = core_.blink1UpCounter();
    now.e6 = core_.blink1UpPhase();
    now.e7 = core_.fruitState();
    if (ev.caught || ev.levelComplete) {
        // the high score follows the score as the score routine ($7091) would have done; the screen is rebuilt from the state
        const std::uint32_t sc = after.zp(0x52) | (after.zp(0x53) << 8) | (after.zp(0x54) << 16);
        if (sc >= (static_cast<std::uint32_t>(display_.high[0]) | (display_.high[1] << 8) | (display_.high[2] << 16))) {
            display_.high = {after.zp(0x52), after.zp(0x53), after.zp(0x54)};
        }
        extraCycles_ = kDeathSequenceCycles;   // the level end is not measured separately yet (M6)
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
