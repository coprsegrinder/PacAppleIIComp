#include "gamedriver.h"

#include "attract.h"
#include "deathtiming.h"

namespace pacman::core {

bool GameDriver::init(const datapack::Pack& pack) {
    pack_ = &pack;
    return core_.init(pack);
}

bool GameDriver::newGame(int players, int rank) {
    if (!core_.newGame(players, rank)) return false;
    const auto high = display_.high;   // the high score survives into the next game
    display_ = DisplayState{};
    display_.high = high;
    prev_ = TickExtras{};
    syncExtras();
    rebuildScreen();
    return true;
}

bool GameDriver::newDemo(const hires::Page& screen, const AttractMemory* memory) {
    if (!core_.newDemo(memory)) return false;
    display_ = DisplayState{};
    if (memory != nullptr) display_.high = memory->high;
    prev_ = TickExtras{};
    syncExtras();
    page_ = screen;
    resyncBackground();
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
    std::array<std::uint32_t, 6> scores{};
    for (int p = 1; p <= core_.players(); ++p) scores[static_cast<std::size_t>(p)] = core_.savedScore(p);
    drawHudPlayers(page_, s, *pack_, display_.high, core_.players(), core_.player(), scores);
    drawPacman(page_, s, *pack_, false);
    display_.lastFlag1E = 0;
    if (staticGhosts_) drawStaticGhosts();
    resyncBackground();
}

void GameDriver::showReady() {
    executeCall(page_, DrawCall{2, {0x5890, 17, 104, 5, 7}}, *pack_);
    drawPacman(page_, core_.state(), *pack_, false);
    resyncBackground();
}

void GameDriver::applySequenceStep(const SequenceStep& step) {
    for (const auto& c : step.calls) executeCall(page_, c, *pack_);
    resyncBackground();
    if (step.rebuild) rebuildScreen();
    if (step.ready) showReady();
}

namespace {
int bcdDigits(std::uint32_t bcd) {   // the digits the screen shows for a BCD score: at least one
    int n = 1;
    for (bcd >>= 4; bcd != 0; bcd >>= 4) ++n;
    return n;
}
}  // namespace

// The death sequence (and, for the last life, the GAME OVER sequence) as timed steps. All times come from deathTimeline() (TASK-033-3b, measured on 245 deaths of the oracle);
// they count from the end of the catching tick's own drawing, which the live loop has already paid for with tickCycles_.
void GameDriver::buildDeathSequence(const GameState& caught, bool gameOver) {
    sequence_.clear();
    RestartInputs in;
    in.pacX = caught.pacX();
    in.pacY = caught.pacY();
    in.pacImage = 0x9BE6;   // $7EC1: LDA #$E6 / LDY #$9B / JSR $6A20 - the death always starts with this one image (frame 6 of the table at $96E8), whatever frame Pac-Man had (oracle: A and L catches)
    for (int g = 0; g < 4; ++g) {
        in.ghostXY[static_cast<std::size_t>(g) * 2] = caught.ghosts[g].x();
        in.ghostXY[static_cast<std::size_t>(g) * 2 + 1] = caught.ghosts[g].y();
    }
    DisplayState scratchDisplay = display_;
    const auto calls = restartCalls(gameOver ? RestartKind::GameOver : RestartKind::Death, in, core_.state(), *pack_, scratchDisplay);
    if (calls.size() < (gameOver ? 21u : 19u)) return;
    DeathTimingInput ti;
    ti.pacX = caught.pacX();
    for (int col = 0; col < 27; ++col)
        for (int row = 1; row < 31; ++row) {
            const std::uint8_t cell = core_.state().map[static_cast<std::size_t>(col) * 32 + static_cast<std::size_t>(row)];
            if (cell == 0) continue;
            if (cell & 1) ++ti.dots;
            else ++ti.energizers;
        }
    const std::uint32_t score = core_.state().zp(0x52) | (core_.state().zp(0x53) << 8) | (core_.state().zp(0x54) << 16);
    ti.scoreDigits = bcdDigits(score);
    ti.highDigits = bcdDigits(static_cast<std::uint32_t>(display_.high[0]) | (display_.high[1] << 8) | (display_.high[2] << 16));
    // the rebuild draws the lives of the player who plays NEXT (the same player when there is only one): GameCore stores the state after "DEC $58" and the switch
    ti.livesBefore = ((gameOver ? caught.lives() : core_.state().lives()) & 0x7F) + 1;
    if (core_.players() > 1 && !gameOver) {   // the labels and scores of the other players are drawn too ($550A loops over all players)
        std::array<std::uint32_t, 6> scores{};
        std::array<int, 6> lives{};
        for (int p = 1; p <= core_.players(); ++p) {
            const bool cur = p == core_.player();
            scores[static_cast<std::size_t>(p)] = cur ? (core_.state().zp(0x52) | (core_.state().zp(0x53) << 8) | (core_.state().zp(0x54) << 16)) : core_.savedScore(p);
            lives[static_cast<std::size_t>(p)] = cur ? core_.state().lives() : core_.savedLives(p);
        }
        ti.extraHud = hudExtraCycles(*pack_, core_.players(), core_.player(), scores, lives, static_cast<std::uint32_t>(display_.high[0]) | (display_.high[1] << 8) | (display_.high[2] << 16));
    }
    const DeathTimeline tl = deathTimeline(ti);
    auto step = [&](std::int64_t at, std::size_t from, std::size_t to) {
        SequenceStep s;
        s.at = at;
        s.calls.assign(calls.begin() + static_cast<std::ptrdiff_t>(from), calls.begin() + static_cast<std::ptrdiff_t>(to));
        sequence_.push_back(std::move(s));
    };
    for (std::size_t i = 0; i < 5; ++i) step(tl.clear[i], i, i + 1);   // ghosts 4, 3, 2, 1 and Pac-Man erased
    step(tl.image, 5, 6);                                              // Pac-Man's last image
    step(tl.imageClear, 6, 7);
    for (std::size_t k = 0; k < 6; ++k) {
        step(tl.frame[k], 7 + 2 * k, 8 + 2 * k);
        sequence_.back().deathSound = 6 - static_cast<int>(k);
        step(tl.frameClear[k], 8 + 2 * k, 9 + 2 * k);
    }
    if (gameOver) {   // the words, then the long wait of the oracle (the attract screens follow; until they exist a new game starts after it)
        const std::int64_t text = tl.frameClear[5] + kGameOverTextAfterLastClear;
        step(text, 19, 21);
        SequenceStep end;
        end.at = text + kGameOverTextDraw + kGameOverToAttractStart;   // the attract loop starts here ($52BB, 033-4a); the caller then returns to AttractFlow
        sequence_.push_back(end);
        return;
    }
    SequenceStep rebuild;
    rebuild.at = tl.rebuild;
    rebuild.rebuild = true;
    sequence_.push_back(rebuild);
    SequenceStep ready;
    ready.at = tl.ready;
    ready.ready = true;
    sequence_.push_back(ready);
    SequenceStep end;   // the READY wait is over: the page of the first tick (no READY) and the first tick follows
    end.at = tl.total;
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
    moves_.clear();
    interpOk_ = false;
    if (ev.paused) return ev;
    const GameState& after = core_.state();
    TickExtras now;
    now.player = core_.player();
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
    if (ev.gameOver && playDeath_) {   // the live game plays the animation and the words as a timed sequence (sequence()), then a new game
        buildDeathSequence(core_.caughtState(), true);
        if (!sequence_.empty()) {
            tickCycles_ = kCatchTickCycles;
            newGamePending_ = true;
            return ev;
        }
    }
    if (ev.gameOver) {   // the last life: the words GAME OVER ($811D, $8139) on the playfield, then a new game after a pause (no death animation yet)
        executeCall(page_, DrawCall{2, {0x811D, 15, 105, 4, 7}}, *pack_);
        executeCall(page_, DrawCall{2, {0x8139, 21, 105, 4, 7}}, *pack_);
        resyncBackground();
        extraCycles_ = kDeathSequenceCycles;
        newGamePending_ = true;
        return ev;
    }
    if (ev.demoOver) {   // the attract game ends: this tick's drawings up to the contact, from the state at the catch (the death script follows)
        const auto calls = tickCalls(before, core_.caughtState(), *pack_, acting && ev.acted, display_, prev_, now);
        lastCalls_ = calls;
        tickCycles_ = tickCostCycles(calls, *pack_, before.demo != 0, ev.ate, ev.energizer);
        for (const auto& c : calls) executeCall(page_, c, *pack_);
        resyncBackground();
        prev_ = now;
        return ev;
    }
    if (ev.caught || ev.levelComplete) {
        // the high score follows the score as the score routine ($7091) would have done; the screen is rebuilt from the state
        const std::uint32_t sc = after.zp(0x52) | (after.zp(0x53) << 8) | (after.zp(0x54) << 16);
        if (sc >= (static_cast<std::uint32_t>(display_.high[0]) | (display_.high[1] << 8) | (display_.high[2] << 16))) {
            display_.high = {after.zp(0x52), after.zp(0x53), after.zp(0x54)};
        }
        if (playDeath_ && ev.caught && !ev.levelComplete) {   // the live game plays the animation (sequence()); the page stays as it was until then
            buildDeathSequence(core_.caughtState(), false);
            if (!sequence_.empty()) {
                tickCycles_ = kCatchTickCycles;
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
    tickCycles_ = tickCostCycles(calls, *pack_, before.demo != 0, ev.ate, ev.energizer);
    for (const auto& c : calls) {
        executeCall(page_, c, *pack_);
        const bool actorImage = c.type == 1 && c.actor >= 0;
        if (!actorImage) executeCall(bgPage_, c, *pack_);   // the background gets everything but the images of the actors (the erases of their old places too)
        else moves_.push_back(ActorMove{c.actor, c.p[1], c.p[2]});
    }
    interpOk_ = true;
    prev_ = now;
    return ev;
}

}  // namespace pacman::core
