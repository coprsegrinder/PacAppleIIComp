#include "gamecore.h"

#include <algorithm>

namespace pacman::core {

bool GameCore::init(const datapack::Pack& pack) {
    pack_ = &pack;
    if (!graph_.load(pack) || !loadStepTables(pack, steps_) || !loadStartTables(pack, start_)) return false;
    const auto a = pack.find(0x5733, 16), b = pack.find(0x5903, 16);
    if (a.empty() || b.empty()) return false;
    std::copy(a.begin(), a.end(), energizerBlink_.begin());
    std::copy(b.begin(), b.end(), oneUpBlink_.begin());
    const auto fp = pack.find(0x6CD4, 16), ft = pack.find(0x6AFE, 27 * 4);
    if (fp.empty() || ft.empty()) return false;
    std::copy(fp.begin(), fp.end(), fruitPoints_.begin());
    std::copy(ft.begin(), ft.end(), fruitTable_.begin());
    return true;
}

bool GameCore::newGame(int players) {
    (void)players;
    player_ = 1;
    e5_ = 3;                  // $E5 at the start of a game (recorded at tick 0 of every scenario); $E6 = $E7 = 0
    e6_ = e7_ = 0;
    demoPointer_ = DemoStream::kStart;
    timer_ = StepTimer();
    return pack_ != nullptr && newGameState(*pack_, state_);
}

// $570C: the 1UP blink ($58B3) and the energizer phase.
void GameCore::blinks() {
    GameState& s = state_;
    const std::size_t idx = std::min<std::size_t>(s.level(), 15);   // $6D1E
    // $58B3: only in the tick before Pac-Man acts ($59 = 1); in the demo the original also calls $80E3 (not rebuilt)
    if (s.pacStepCounter() == 1) {
        if (e5_ != 0) {
            --e5_;
        } else {
            e5_ = oneUpBlink_[idx];
            if (e6_ != 0) {
                e6_ = 0;
                e5_ = static_cast<std::uint8_t>(e5_ >> 1);   // CLC, ROR $E5
            } else {
                e6_ = 1;
            }
        }
    }
    // $570F: the energizer phase
    if (s.zp(0x5E) != 0) {
        --s.zp(0x5E);
        return;
    }
    s.zp(0x5E) = energizerBlink_[idx];
    s.zp(0x5F) = s.zp(0x5F) != 0 ? 0 : 1;
}

// $79CD-$79FA: Pac-Man on the fruit's place (y $6B, x $8A-$8D) while it is shown: it is eaten (points by level, $6CB2).
void GameCore::fruitEatenCheck() {
    GameState& s = state_;
    if (s.pacY() != 0x6B || s.pacX() < 0x8A || s.pacX() >= 0x8E || e7_ != 1) return;
    s.zp(0x5A) = 0x30;
    s.zp(0x5B) = 0;
    e7_ = 2;
    if (s.demo != 0) return;                                           // $6CB2: no points in the demo
    const unsigned add = fruitPoints_[std::min<std::size_t>(s.level(), 15)];   // BCD hundreds
    unsigned lo = (s.zp(0x53) & 0x0Fu) + (add & 0x0Fu);
    unsigned hi = (s.zp(0x53) >> 4) + (add >> 4);
    if (lo > 9) { lo -= 10; ++hi; }
    const bool carry = hi > 9;
    if (carry) hi -= 10;
    s.zp(0x53) = static_cast<std::uint8_t>((hi << 4) | lo);
    unsigned top = (s.zp(0x54) & 0x0Fu) + (carry ? 1u : 0u), topHi = s.zp(0x54) >> 4;
    if (top > 9) { top -= 10; ++topHi; }
    s.zp(0x54) = static_cast<std::uint8_t>(((topHi > 9 ? topHi - 10 : topHi) << 4) | top);
    awardBonusLife(s, player_);                                        // $7091 contains the extra-life test
}

// $6A7C: the bonus fruit. $57 = 0 starts it; while it is shown its lifetime ($5A/$5B) counts down in the tick before Pac-Man acts.
void GameCore::fruit() {
    GameState& s = state_;
    if (s.zp(0x57) == 0) {
        const std::size_t i = std::min<std::size_t>(s.level(), 26) * 4;
        s.zp(0x5C) = fruitTable_[i];
        s.zp(0x5D) = fruitTable_[i + 1];
        s.zp(0x5A) = fruitTable_[i + 2];
        s.zp(0x5B) = fruitTable_[i + 3];
        s.zp(0x57) = 0x60;
        e7_ = 1;
    }
    if (s.pacStepCounter() != 1 || e7_ == 0) return;
    if (s.zp(0x5A) == 0) --s.zp(0x5B);
    --s.zp(0x5A);
    const unsigned sum = s.zp(0x5A) + s.zp(0x5B);
    if ((sum & 0xFF) == 0 && sum < 0x100) e7_ = 0;                      // $6AC0: BCC after a zero sum: the lifetime has run out
}

void GameCore::startNextLevelUnverified() {
    GameState& s = state_;
    PlayerSave save = savePlayer(s);   // $52-$59
    save.bytes[3] = static_cast<std::uint8_t>(s.level() + 1);
    save.bytes[4] = 0xF8;              // items left
    save.bytes[5] = 0x40;              // fruit counter
    save.bytes[7] = 1;                 // step counter
    const auto map = pack_->find(0x65BA, 896);
    if (!map.empty()) std::copy(map.begin(), map.end(), s.map.begin());
    startOfLife(s, start_, save);
    e7_ = 0;
}

TickEvents GameCore::tick(GhostSource& ghosts) {
    TickEvents ev;
    GameState& s = state_;
    // ---- $7861: Pac-Man ----
    if (s.demo == 0) {
        const PollResult pr = poller_.poll(s, keys_, port_);
        if (pr.paused) {
            ev.paused = true;
            return ev;
        }
    }
    ev.acted = timer_.tick(s, steps_);
    if (ev.acted) {
        const auto outcome = junctionStep(s, graph_, StreamByteSource([&] {
            ++demoPointer_;
            const auto b = pack_->find(demoPointer_, 1);
            return b.empty() ? std::uint8_t{0xFF} : b[0];
        }));
        const auto test = stepPacman(s, outcome);
        fruitEatenCheck();
        if (test) {
            const EatResult r = eatAt(s, *test, timer_, steps_, player_);
            ev.ate = r.ate;
            ev.energizer = r.energizer;
            ev.outsideMap = r.outsideMap;
            if (r.energizer && s.level() < 0x15) {              // $6D60-$6D9D (the ghosts' part and the timer $E1/$E2 are ghost logic, M5)
                s.zp(0x68) = 2;                                  // the next ghost eaten is worth 200
                s.cornerPhase = 1;                               // $E9
            }
            if (r.levelComplete) {
                ev.levelComplete = true;
                return ev;
            }
        }
    }
    // ---- $86EA: the ghosts ----
    const std::array<bool, 4> moved = ghosts.step(s, ev.energizer);
    for (int g = 3; g >= 0; --g) {
        GhostRecord& gh = s.ghosts[static_cast<std::size_t>(g)];
        if (!moved[static_cast<std::size_t>(g)]) continue;   // only a ghost that acted runs the contact test
        if (gh.b[9] != 0 || gh.b[17] != 0) continue;      // eyes and ghosts still in the house (in-house flag $81 = byte 17): no contact (Inferred from scenario D, tick 830)
        const Contact c = ghostContact(s, gh);
        if (c == Contact::None) continue;
        StepTimer::applyNearGhostDelay(s, gh);            // $8B4A-$8B65 (level 5 and up)
        if (c != Contact::Touch) continue;
        if (gh.b[8] != 0) {                               // frightened: eaten
            ghostEaten(s, static_cast<std::size_t>(g), player_);
            s.mode78 = 0;
            s.mode79 = 1;
            ev.ghostEaten[static_cast<std::size_t>(g)] = true;
            continue;
        }
        pacmanCaught(s);                                  // $8C0E: DEC $58
        ev.caught = true;
        ev.gameOver = s.lives() == 0;
        if (!ev.gameOver) {
            const PlayerSave kept = savePlayer(s);        // $8C1A / $8C1D
            startOfLife(s, start_, kept);
            e7_ = 0;                                      // $541C: the bonus fruit is gone
        }
        return ev;
    }
    // ---- $6A7C: the bonus fruit ----
    fruit();
    // ---- $570C ----
    blinks();
    return ev;
}

}  // namespace pacman::core
