#include "gamecore.h"

#include <algorithm>

namespace pacman::core {

bool GameCore::init(const datapack::Pack& pack) {
    pack_ = &pack;
    if (!graph_.load(pack) || !loadStepTables(pack, steps_) || !loadStartTables(pack, start_)) return false;
    if (!ghostTables_.load(pack) || !ghostData_.load(pack) || !ghostModes_.load(pack)) return false;
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

bool GameCore::newGame(int players, int rank) {
    players_ = players < 1 ? 1 : players > 5 ? 5 : players;
    player_ = 1;
    ghostShared_ = GhostShared{};
    ghostShared_.e2 = 0xFF;                   // $E1/$E2 at the start of a game: $FF00 (recorded at tick 0)
    ghostShared_.ea = 0xA0;                   // $53E6: 160 mode ticks of corner phase
    initGhostQueues(queues_, ghostData_);
    e5_ = 3;                  // $E5 at the start of a game (recorded at tick 0 of every scenario); $E6 = $E7 = 0
    e6_ = e7_ = 0;
    demoPointer_ = DemoStream::kStart;
    timer_ = StepTimer();
    if (pack_ == nullptr || !newGameState(*pack_, state_)) return false;
    if (rank > 1) state_.zp(0x55) = static_cast<std::uint8_t>(rank);   // the first level of the game ($54E1)
    for (int p = 1; p <= players_; ++p) {   // $54F4-$5503: every player's bytes and maze are saved
        stores_[static_cast<std::size_t>(p)].save = savePlayer(state_);
        stores_[static_cast<std::size_t>(p)].map = state_.map;
    }
    return true;
}

bool GameCore::newDemo() {
    if (!newGame()) return false;
    state_.demo = 1;
    state_.zp(0x58) = 0;
    state_.mode79 = 0;
    e5_ = 0;                                  // $E5 = 0 at the first tick of the demo (recorded: tools/record_demo_full.py)
    for (std::size_t i = 15; i < queues_[0].size(); ++i) queues_[0][i] = 0xDD;   // $52BB filled the lo-res page with $DD; $6E79 wrote only the first 15 bytes of this queue page
    return true;
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
void GameCore::fruitEatenCheck(TickEvents& ev) {
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
    const std::uint8_t livesBefore = s.lives() & 0x7F;
    awardBonusLife(s, player_);                                        // $7091 contains the extra-life test
    if ((s.lives() & 0x7F) > livesBefore) ev.sounds.push_back(SoundId::BonusLife);   // $6CB2: the score first (and with it the bonus life), the fruit sound is its last call
    ev.sounds.push_back(SoundId::Fruit);
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

void GameCore::startNextLevel() {
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
    // the ghost side of $53E6, as after a catch: queues ($6E79), corner phase for 160 mode ticks, no frightened period
    initGhostQueues(queues_, ghostData_);
    ghostShared_.ea = 0xA0;
    ghostShared_.eb = 0;
    ghostShared_.e4 = 0;
}

bool GameCore::contactTest(int g, TickEvents& ev) {
    GameState& s = state_;
    GhostRecord& gh = s.ghosts[static_cast<std::size_t>(g)];
    if (gh.b[9] != 0 || gh.b[17] != 0) return false;     // eyes and ghosts still in the house (in-house flag $81 = byte 17): no contact
    const Contact c = ghostContact(s, gh);
    if (c == Contact::None) return false;
    StepTimer::applyNearGhostDelay(s, gh);                // $8B4A-$8B65 (level 5 and up)
    if (c != Contact::Touch) return false;
    if (gh.b[8] != 0) {                                   // frightened: eaten
        const std::uint8_t livesBefore = s.lives() & 0x7F;
        ghostEaten(s, static_cast<std::size_t>(g), player_);
        if (s.demo == 0) {   // the score (with the bonus-life test) first, then the freeze routine plays the ghost sound ($7DFC)
            if ((s.lives() & 0x7F) > livesBefore) ev.sounds.push_back(SoundId::BonusLife);
            ev.sounds.push_back(SoundId::GhostEaten);
        }
        ghostEatenTimer(s, ghostShared_, ghostModes_);    // $8BA4-$8BB5
        s.mode78 = 0;
        s.mode79 = 1;
        ev.ghostEaten[static_cast<std::size_t>(g)] = true;
        return false;
    }
    pacmanCaught(s);                                      // $8C0E: DEC $58
    ev.caught = true;
    if (s.demo == 0) {   // the death animation plays its six steps ($0A = 6 ... 1) in this tick
        for (SoundId d : {SoundId::Death6, SoundId::Death5, SoundId::Death4, SoundId::Death3, SoundId::Death2, SoundId::Death1}) ev.sounds.push_back(d);
    }
    caughtState_ = s;   // where Pac-Man and the ghosts stood when he was caught (the death animation starts there)
    if (s.demo != 0) {   // the attract game ends here: $8C07 jumps back to the title ($43D2)
        ev.gameOver = false;
        ev.demoOver = true;
        return true;
    }
    // $8C1A-$8C4D: this player's maze and bytes are saved ($5394, $53BE); then the next player (wrapping round) whose lives are not 0 is loaded ($53CF, $5360); when it comes
    // back to the same player and he has no life left, the game is over ($6EC3)
    PlayerStore& mine = stores_[static_cast<std::size_t>(player_)];
    mine.save = savePlayer(s);
    mine.map = s.map;
    int p = player_;
    bool found = false;
    for (;;) {
        if (++p > players_) p = 1;
        loadPlayer(s, stores_[static_cast<std::size_t>(p)].save);
        if (p == player_) {
            found = s.lives() != 0;
            break;
        }
        if (s.lives() != 0) {
            found = true;
            break;
        }
    }
    ev.gameOver = !found;
    if (found) {
        ev.playerSwitched = p != player_;
        player_ = p;
        s.map = stores_[static_cast<std::size_t>(p)].map;
        startOfLife(s, start_, stores_[static_cast<std::size_t>(p)].save);
        e7_ = 0;                                          // $541C: the bonus fruit is gone
        // the ghost side of $53E6: queues ($6E79), corner phase for 160 mode ticks, no frightened period
        initGhostQueues(queues_, ghostData_);
        ghostShared_.ea = 0xA0;
        ghostShared_.eb = 0;
        ghostShared_.e4 = 0;
    }
    return true;
}

TickEvents GameCore::tick(GhostSource& ghosts) { return tickImpl(&ghosts); }

TickEvents GameCore::tick() { return tickImpl(nullptr); }

TickEvents GameCore::tickImpl(GhostSource* fed) {
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
        if (fed == nullptr) modeTimerStep(s, ghostShared_, queues_);   // $7874: JSR $808C
        const auto outcome = junctionStep(s, graph_, StreamByteSource([&] {
            ++demoPointer_;
            const auto b = pack_->find(demoPointer_, 1);
            return b.empty() ? std::uint8_t{0xFF} : b[0];
        }));
        const auto test = stepPacman(s, outcome);
        fruitEatenCheck(ev);
        if (test) {
            const std::uint8_t livesBefore = s.lives() & 0x7F;
            const EatResult r = eatAt(s, *test, timer_, steps_, player_);
            ev.ate = r.ate;
            ev.energizer = r.energizer;
            ev.outsideMap = r.outsideMap;
            // (outside the map the original reads a master copy behind it and may eat a dot there - with its sound; not modelled, as for the score)
            if (r.ate && s.demo == 0) {   // $7B09 / $7AC7: the sound first, then the score ($7B12) with the bonus-life test
                ev.sounds.push_back(r.energizer ? SoundId::Energizer : SoundId::DotEaten);
                if ((s.lives() & 0x7F) > livesBefore) ev.sounds.push_back(SoundId::BonusLife);
            }
            if (r.energizer) {
                if (fed == nullptr) {
                    energizerGhostEffects(s, ghostShared_, queues_, ghostModes_);   // $6D60 (levels below 21)
                } else if (s.level() < 0x15) {
                    s.zp(0x68) = 2;                                                 // the fed ghost source supplies the rest
                    s.cornerPhase = 1;
                }
            }
            if (r.levelComplete) {
                ev.levelComplete = true;
                return ev;
            }
        }
    }
    // ---- $86EA: the ghosts ----
    if (fed != nullptr) {
        const std::array<bool, 4> moved = fed->step(s, ev.energizer);
        for (int g = 3; g >= 0; --g) {
            if (!moved[static_cast<std::size_t>(g)]) continue;   // only a ghost that acted runs the contact test
            if (contactTest(g, ev)) return ev;
        }
    } else {
        for (int n = 4; n >= 1; --n) {
            const HeadResult h = ghostHead(s, n, ghostShared_, ghostTables_);
            if (!h.acted) continue;
            ghostJunction(s, n, queues_, graph_, ghostData_, ghostTables_);
            ghostMove(s, n, queues_, ghostData_);
            if (contactTest(n - 1, ev)) return ev;
        }
        s.mode78 = s.ghosts[0].b[8];   // the working registers keep the last ghost processed (ghost 1)
        s.mode79 = s.ghosts[0].b[9];
    }
    // ---- $6A7C: the bonus fruit ----
    fruit();
    // ---- $570C ----
    blinks();
    return ev;
}

}  // namespace pacman::core
