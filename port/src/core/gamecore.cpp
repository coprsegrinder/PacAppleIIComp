#include "gamecore.h"

#include "cycles.h"

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
    if (rank > 1) {
        state_.zp(0x55) = static_cast<std::uint8_t>(rank);   // the first level of the game ($54E1)
        startOfLife(state_, start_, savePlayer(state_));      // $53E6 reads the level: the in-house countdowns of the ghosts are the level's
    }
    for (int p = 1; p <= players_; ++p) {   // $54F4-$5503: every player's bytes and maze are saved
        stores_[static_cast<std::size_t>(p)].save = savePlayer(state_);
        stores_[static_cast<std::size_t>(p)].map = state_.map;
    }
    lifeStartPoll(false);
    return true;
}

int GameCore::jinglePollExtra() const noexcept {
    if (!port_.joystickMode) return 0;
    return joystickRoutine7041(false, state_.pacDirection(), port_.paddleX, port_.paddleY).cycles + 17 - 98;
}

void GameCore::lifeStartPoll(bool demo) {
    g_stepCounter1F = 0;   // $541E: STA $1F with A = 0 in the set-up of every life
    lifeStartCycles_ = 0;
    if (!port_.joystickMode) return;   // (keyboard mode: the poll finds no key at this moment; the keys wait for the next tick's poll)
    const std::uint8_t saved = state_.demo;
    state_.demo = demo ? 1 : 0;
    const PollResult pr = poller_.poll(state_, keys_, port_);
    state_.demo = saved;
    lifeStartCycles_ = pr.extraCycles;
}

bool GameCore::newDemo(const AttractMemory* memory) {
    if (!newGame()) return false;
    if (memory != nullptr) {                  // the demo starts from what the last game / prompt / demo left: his score (not changed in the demo), the order of players, the blink and penalty counters
        players_ = memory->players;
        player_ = memory->current;
        for (int i = 0; i < 3; ++i) state_.zp(static_cast<std::uint16_t>(0x52 + i)) = memory->live[static_cast<std::size_t>(i)];
        timer_.setF3(memory->f3);
    }
    state_.demo = 1;
    lifeStartPoll(true);
    state_.zp(0x58) = 0;
    state_.mode79 = 0;
    e5_ = memory != nullptr ? memory->e5 : 0;   // $E5 = 0 at the first tick of the first demo (recorded: tools/record_demo_full.py); later ones start with what the last one left
    if (memory != nullptr) e6_ = memory->e6;
    for (std::size_t i = 15; i < queues_[0].size(); ++i) queues_[0][i] = 0xDD;   // $52BB filled the lo-res page with $DD; $6E79 wrote only the first 15 bytes of this queue page
    return true;
}

AttractMemory GameCore::attractMemory(const AttractMemory& before) const {
    AttractMemory m = before;
    m.players = players_;
    m.current = player_;
    for (int i = 0; i < 8; ++i) m.live[static_cast<std::size_t>(i)] = state_.zp(static_cast<std::uint16_t>(0x52 + i));
    for (int p = 1; p <= players_; ++p) m.saves[static_cast<std::size_t>(p)] = stores_[static_cast<std::size_t>(p)].save.bytes;
    storeBlinkState(m);
    return m;
}

// $570C: the 1UP blink ($58B3) and the energizer phase.
void GameCore::blinks() {
    GameState& s = state_;
    const std::size_t idx = std::min<std::size_t>(s.level(), 15);   // $6D1E
    const int classCost = s.level() < 15 ? 14 : 15;                  // $6D1E: LDA, CMP, BCC taken, RTS / LDA, CMP, BCC, LDA #$F, RTS
    // $58B3: only in the tick before Pac-Man acts ($59 = 1); in the demo the original also calls $80E3 (its cycles are the glue of the two OR blits of tickdraw.cpp)
    // (the idle path is LDA $59, CMP #1, BNE taken, RTS)
    if (s.pacStepCounter() == 1) {
        if (s.demo == 0) {
            cy(-1 + 3 + 3 + 3);                                      // BNE not taken instead of taken, LDA $E8, BEQ taken, LDA $E5
        } else {
            cy(-1 + 3 + 2 + 3);                                      // the same, BEQ not taken; JSR $80E3 follows: its 119 cycles are the glue of the two OR blits (tickdraw.cpp)
        }
        if (e5_ != 0) {
            cy(2 + 5);                                               // BEQ not taken, DEC $E5
        } else {
            cy(3 + 6 + classCost + 2 + 4 + 3 + 3);                   // BEQ taken, JSR $6D1E (+ body), TAX, LDA $5903,X, STA $E5, LDA $E6
            if (e6_ != 0) cy(2 + 6 + 26 + 2 + 3 + 2 + 5);            // BEQ not taken, JSR $58E7 (+ body), LDA #0, STA $E6, CLC, ROR $E5 (RTS is the idle path's)
            else cy(3 + 6 + 14 + 2 + 3);                             // BEQ taken, JSR $58FA (+ body up to the JMP $556E), LDA #1, STA $E6
        }
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
    // $570F: the energizer phase (the idle path is LDA $5E, BEQ not taken, DEC $5E, RTS)
    if (s.zp(0x5E) != 0) {
        --s.zp(0x5E);
        return;
    }
    cy(-5 + 1 + 6 + classCost + 2 + 4 + 3 + 3);                      // no DEC, BEQ taken, JSR $6D1E (+ body), TAX, LDA $5733,X, STA $5E, LDA $5F
    s.zp(0x5E) = energizerBlink_[idx];
    cy(s.zp(0x5F) != 0 ? 2 + 6 + 2 + 3 : 3 + 6 + 2 + 3);              // BEQ (not) taken, JSR $5743 / $57BE, LDA #0 / #1, STA $5F
    cy(110);                                                         // the loop over the four energizer places in $5743 / $57BE, without the drawing calls (their glue is in tickdraw.cpp); measured as the same whatever the number of places left
    s.zp(0x5F) = s.zp(0x5F) != 0 ? 0 : 1;
}

// $79CD-$79FA: Pac-Man on the fruit's place (y $6B, x $8A-$8D) while it is shown: it is eaten (points by level, $6CB2).
void GameCore::fruitEatenCheck(TickEvents& ev) {
    GameState& s = state_;
    cy(3 + 2);                                                         // $79CD: LDA $61, CMP #$6B
    if (s.pacY() != 0x6B) {
        cy(3);                                                         // BNE taken
        return;
    }
    cy(2 + 3 + 2);                                                     // BNE, LDA $60, CMP #$8A
    if (s.pacX() < 0x8A) {
        cy(3);
        return;
    }
    cy(2 + 2);                                                         // BCC, CMP #$8E
    if (s.pacX() >= 0x8E) {
        cy(3);
        return;
    }
    cy(2 + 3 + 2);                                                     // BCS, LDA $E7, CMP #1
    if (e7_ != 1) {
        cy(3);
        return;
    }
    cy(2 + 2 + 3 + 2 + 3 + 2 + 3 + 2 + 3 + 2 + 3 + 6);                 // BNE, the stores of $79E3-$79F5, JSR $6978 (the box clear is a drawing call); then JSR $6CB2 (not counted yet)
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
    // the idle path: LDA $57, BNE taken, LDA $59, CMP #1, BNE taken, RTS
    if (s.zp(0x57) == 0) {
        const std::size_t i = std::min<std::size_t>(s.level(), 26) * 4;
        cy(-1 + 3 + 2 + (s.level() < 26 ? 3 : 2 + 2) + 2 + 2 + 2);      // BNE not taken, LDA $55, CMP #$1A, BCC (LDA #$1A), ASL, ASL, TAX
        cy(4 + cyCross(0x6AFE, static_cast<int>(i)) + 3 + 4 + cyCross(0x6AFF, static_cast<int>(i)) + 3 + 4 + cyCross(0x6B00, static_cast<int>(i)) + 3 + 4 + cyCross(0x6B01, static_cast<int>(i)) + 3);
        cy(2 + 3 + 2 + 3);                                              // LDA #$60, STA $57, LDA #1, STA $E7
        s.zp(0x5C) = fruitTable_[i];
        s.zp(0x5D) = fruitTable_[i + 1];
        s.zp(0x5A) = fruitTable_[i + 2];
        s.zp(0x5B) = fruitTable_[i + 3];
        s.zp(0x57) = 0x60;
        e7_ = 1;
    }
    if (s.pacStepCounter() != 1) return;
    // $6AA7: BNE not taken (the idle path took it), then the test of $E7
    cy(-1 + 3);                                                         // BNE not taken instead of taken, LDA $E7
    if (e7_ == 0) {
        cy(3);                                                          // BEQ taken
        return;
    }
    cy(2 + 3);                                                          // BEQ not taken, LDA $5A
    if (s.zp(0x5A) == 0) {
        cy(2 + 5);                                                      // BNE, DEC $5B
        --s.zp(0x5B);
    } else {
        cy(3);                                                          // BNE taken
    }
    --s.zp(0x5A);
    cy(5 + 2 + 3 + 3);                                                  // DEC $5A, CLC, LDA $5A, ADC $5B
    const unsigned sum = s.zp(0x5A) + s.zp(0x5B);
    if ((sum & 0xFF) == 0 && sum < 0x100) {                             // $6AC0: BCC after a zero sum: the lifetime has run out
        cy(2 + 3 + 2 + 3 + 2 + 3 + 2 + 3 + 6);                          // BNE not taken, BCC taken, $6AEB: LDA #$10, STA $04, LDA #$66, STA $10, LDA #0, STA $E7, JSR $6978 (the box clear is a drawing call)
        e7_ = 0;
        return;
    }
    cy((sum & 0xFF) != 0 ? 3 : 2 + 2);                                  // BNE taken / BNE, BCC not taken
    cy(3 + 2);                                                          // $6AC2: LDA $E7, CMP #1
    if (e7_ == 1) {
        cy(3 + 3 + 3 + 3 - 6);                                          // BEQ taken, LDA $5C, LDY $5D, JMP $6EDA (the image: a drawing call, ending in the RTS of the routine: the idle path's RTS is not run)
    } else {
        cy(2 + 2 + 3 + 2 + 3 + 3 + 3 + 2 + 3 + 6 + (s.level() < 15 ? 14 : 15) + 2 + 4 + 3 + 3 - 6);   // the points: $6AC8-$6AE1, JMP $70F5 (the digit routine: calls, and its RTS ends the routine)
    }
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
    lifeStartPoll(false);
}

namespace {

// $8B34: the contact test of a ghost that has taken a step: JSR, the distance along the row or column, and for a distance below 10 the extra tick of the level 5 and up rule; for a
// touch (distance below 6) the count ends where the routine branches to the catch or the meal ($8B7E), whose cost is not counted here.
void contactCost(const GameState& s, const GhostRecord& g) {
    const unsigned px = s.pacX(), py = s.pacY(), gx = g.b[0], gy = g.b[1], dir = g.b[2];
    cy(6 + 3 + 3);                                             // JSR $8B34, LDA $60, CMP $70
    unsigned a, b;
    if (px == gx) {
        cy(2 + 2 + 3 + 3);                                     // BNE not taken, SEC, LDA $61, SBC $71
        a = py;
        b = gy;
    } else {
        cy(3 + 3 + 3);                                         // BNE taken, LDA $61, CMP $71
        if (py != gy) {
            cy(3 + 6);                                         // BNE taken, RTS
            return;
        }
        cy(2 + 2 + 3 + 3 + 3);                                 // BNE not taken, SEC, LDA $60, SBC $70, JMP $8B3F
        a = px;
        b = gx;
    }
    unsigned d;
    if (a >= b) {
        cy(3);                                                 // BCS taken
        d = a - b;
    } else {
        cy(2 + 2 + 2);                                         // BCS, EOR #$FF, ADC #1
        d = b - a;
    }
    cy(2);                                                     // CMP #$0A
    if (d >= 10) {
        cy(3 + 6);                                             // BCS taken, RTS
        return;
    }
    cy(2 + 2 + 3 + 2);                                         // BCS, TAX, LDA $55, CMP #5
    if (s.level() < 5) {
        cy(3);                                                 // BCC taken
    } else {
        cy(2 + 3 + 2);                                         // BCC, LDA $72, AND #2
        unsigned looked;
        if ((dir & 2) == 0) {
            cy(3 + 3);                                         // BEQ taken, LDA $60
            looked = px;
        } else {
            cy(2 + 3);                                         // BEQ, LDA $61
            if (py != 0) {
                cy(3);                                         // BNE taken
                looked = py;
            } else {
                cy(2 + 3);                                     // BNE, LDA $60
                looked = px;
            }
        }
        cy(2);                                                 // AND #2
        if ((looked & 2) != 0) cy(3);                          // BNE taken
        else cy(2 + 2 + 3 + 2 + 3);                            // BNE, CLC, LDA $59, ADC #1, STA $59
    }
    cy(2 + 2);                                                 // TXA, CMP #6
    if (d >= 6) cy(3 + 6);                                     // BCS taken, RTS
    else cy(2 + 3);                                            // BCS, JMP $8B7E
}

}  // namespace

// $7DB8-$7DDB: the choice of the score image by the step $68 (2: 15 cycles, 4: 19, 8: 20, 16: 22); the constant is the cost of the step 2
static int ghostEatenHeaderExtra(unsigned step) {
    return step == 0x02 ? 0 : step == 0x04 ? 4 : step == 0x08 ? 5 : 7;
}

// $7DF6-$7E08: six passes of $805D (the middle of them the sound, $7BE6). Per pass and per ghost 4..1 the pass copies the ghost to the work area, looks at its eyes flag $79 and, for
// an eyes ghost, runs the ghost step $8763 twice with the countdown $80 set to 1 (LDA #1, STA $80, JSR $8763: 11 cycles each); the step is the main loop's (head, junction, move, contact test).
// The constant of the freeze counts the passes of no eyes ghost (LDA $79, BEQ taken: 6); an eyes ghost's 27 are 21 more.
void GameCore::freezeWalk(int eaten, TickEvents& ev) {
    GameState& s = state_;
    freeze_.eaten = eaten;
    freeze_.atStart = s;
    for (int pass = 0; pass < 6; ++pass) {
        for (int n = 4; n >= 1; --n) {
            GhostRecord& gh = s.ghosts[static_cast<std::size_t>(n - 1)];
            if (gh.b[9] == 0) continue;
            cy(21);
            for (int k = 0; k < 2; ++k) {
                FreezeStep step;
                step.ghost = n - 1;
                step.before = s;
                s.ghosts[static_cast<std::size_t>(n - 1)].b[16] = 1;   // LDA #1, STA $80
                const HeadResult h = ghostHead(s, n, ghostShared_, ghostTables_);
                if (h.acted) {
                    ghostJunction(s, n, queues_, graph_, ghostData_, ghostTables_);
                    ghostMove(s, n, queues_, ghostData_, &ghostShared_);
                    contactTest(n - 1, ev);
                }
                cy(13);   // the call's own DEC / BNE / RTS beyond what the head replaced (as ghostCall_)
                step.after = s;
                step.e1 = ghostShared_.e1;
                step.e2 = ghostShared_.e2;
                freeze_.steps.push_back(std::move(step));
            }
        }
    }
    freeze_.atEnd = s;
}

bool GameCore::contactTest(int g, TickEvents& ev) {
    GameState& s = state_;
    GhostRecord& gh = s.ghosts[static_cast<std::size_t>(g)];
    if (gh.b[9] != 0 || gh.b[17] != 0) return false;     // eyes and ghosts still in the house (in-house flag $81 = byte 17): no contact
    contactCost(s, gh);
    const Contact c = ghostContact(s, gh);
    if (c == Contact::None) return false;
    if (StepTimer::nearGhostDelays(s, gh)) ++nearDelays_;   // (counted for the tests)
    StepTimer::applyNearGhostDelay(s, gh);                // $8B4A-$8B65 (level 5 and up)
    if (c != Contact::Touch) return false;
    if (gh.b[8] != 0) {                                   // frightened: eaten
        const std::uint8_t livesBefore = s.lives() & 0x7F;
        const unsigned step68 = s.zp(0x68);   // the score step of this ghost (2, 4, 8, 16): the freeze chooses its image by it
        if (s.demo == 0 && !fedTick_) freezeWalk(g, ev);   // (the freeze runs before $8BA4: the eaten ghost is not eyes yet)
        ghostEaten(s, static_cast<std::size_t>(g), player_);
        if (s.demo == 0) {   // the score (with the bonus-life test) first, then the freeze routine plays the ghost sound ($7DFC)
            if ((s.lives() & 0x7F) > livesBefore) ev.sounds.push_back(SoundId::BonusLife);
            ev.sounds.push_back(SoundId::GhostEaten);
        }
        if (s.demo == 0) cy(kGhostEatenTickExtraCycles + ghostEatenHeaderExtra(step68) + cost79A4(s.zp(0x66)));   // the part of the freeze that is not the sound, with Pac-Man's draw prologue ($79A4) of the path it takes
        ghostEatenTimer(s, ghostShared_, ghostModes_);    // $8BA4-$8BB5
        s.mode78 = 0;
        s.mode79 = 1;
        ev.ghostEaten[static_cast<std::size_t>(g)] = true;
        return false;
    }
    pacmanCaught(s);                                      // $8C0E: DEC $58
    ev.caught = true;
    caughtBy_ = g;
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
        lifeStartPoll(false);
    }
    return true;
}

TickEvents GameCore::tick(GhostSource& ghosts) { return tickImpl(&ghosts); }

TickEvents GameCore::tick() { return tickImpl(nullptr); }

TickEvents GameCore::tickImpl(GhostSource* fed) {
    TickEvents ev;
    meter_.reset();
    freeze_ = FreezeLog{};
    fedTick_ = fed != nullptr;
    struct MeterScope {   // the counting is on while this tick runs
        CycleMeter* previous;
        explicit MeterScope(CycleMeter* m) : previous(g_cycleMeter) { g_cycleMeter = m; }
        ~MeterScope() { g_cycleMeter = previous; }
    } meterScope(&meter_);
    GameState& s = state_;
    // ---- $7861: Pac-Man ----
    // Joystick mode: `LDA $F5 / BNE $7868` skips the poll at the start of the tick. The keyboard tick pays LDA (3) + BNE (2) + JSR (6) + the poll's body (62 in the attract game, 122 in a game) and
    // the joystick tick LDA (3) + BNE taken (3): 67 / 127 cycles less. In a tick in which Pac-Man steps, the poll of $786D comes back and the difference to the keyboard tick is the whole
    // PollResult::extraCycles (set below), which already contains the skipped poll (V: tools/record_joystick_demo.py, tools/record_joystick_ticks.py, tests/test_attract.cpp, test_gamedriver.cpp).
    if (port_.joystickMode) ev.inputCycles = s.demo != 0 ? -67 : -127;
    if (s.demo == 0 && !port_.joystickMode) {   // keyboard mode: $7865 polls at the start of every tick (joystick mode polls below, when the step timer runs out: $7871)
        const PollResult pr = poller_.poll(s, keys_, port_);
        if (pr.paused) {
            ev.paused = true;
            return ev;
        }
    }
    ev.acted = timer_.tick(s, steps_);
    if (ev.acted && s.demo != 0 && port_.joystickMode) {   // the same poll in the attract game: $7041 finds $E8 <> 0 and answers $80, which is written into $63 (the demo's own moves come from its stream)
        const JoystickResult j = joystickRoutine7041(true, s.pacDirection(), port_.paddleX, port_.paddleY);
        s.requestedDirection() = j.a;
        ev.inputCycles = j.cycles + 17 - 38;   // (the whole difference to the keyboard tick: the skipped start-of-tick poll is in it)
    }
    if (ev.acted && s.demo == 0 && port_.joystickMode) {   // $786D: LDA $F5; BEQ; JSR $6F5E - the paddle routines cost about 2,700 cycles, once per step of Pac-Man
        const PollResult pr = poller_.poll(s, keys_, port_);
        ev.inputCycles = pr.extraCycles;       // (likewise)
        if (pr.paused) {
            ev.paused = true;
            return ev;
        }
    }
    if (ev.acted) {
        if (fed == nullptr) modeTimerStep(s, ghostShared_, queues_);   // $7874: JSR $808C
        const auto outcome = junctionStep(s, graph_, StreamByteSource([&] {
            ++demoPointer_;
            if ((demoPointer_ & 255) == 0) cy(2 + 5 - 3);   // $78DD: the low byte wrapped, BNE is not taken and INC $1C follows
            const auto b = pack_->find(demoPointer_, 1);
            return b.empty() ? std::uint8_t{0xFF} : b[0];
        }));
        const auto test = stepPacman(s, outcome);
        if (outcome == JunctionOutcome::Move) fruitEatenCheck(ev);   // ($7B2F and $7905 do not come back through $79CD)
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
            const std::int64_t before = meter_.total();
            const HeadResult h = ghostHead(s, n, ghostShared_, ghostTables_);
            if (!h.acted) {
                ghostCall_[static_cast<std::size_t>(n - 1)] = 13;
                continue;
            }
            ghostJunction(s, n, queues_, graph_, ghostData_, ghostTables_);
            ghostMove(s, n, queues_, ghostData_, &ghostShared_);
            {   // the draw is the end of the step: the flags and the timer as they stand now (the contact test that follows may eat the ghost, and $8BA4 then adds to the timer)
                const GhostRecord& gr = s.ghosts[static_cast<std::size_t>(n - 1)];
                drawInfo_[static_cast<std::size_t>(n - 1)] = GhostDrawInfo{static_cast<std::uint16_t>(ghostShared_.e1 | (ghostShared_.e2 << 8)), gr.b[8], gr.b[9]};
            }
            const bool over = contactTest(n - 1, ev);
            ghostCall_[static_cast<std::size_t>(n - 1)] = meter_.total() - before + 13;
            if (over) return ev;
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
