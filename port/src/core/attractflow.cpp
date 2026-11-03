#include "attractflow.h"

#include <algorithm>

namespace pacman::core {

namespace {
constexpr std::uint16_t kRdkey = 0xFD0C, kGetln = 0xFD6F;
// the texts of the game ($427C: "HOW MANY PLAYERS?  (1 - 5)   " 29 bytes and the 55 bytes of the PRESS RETURN prompt; $41DC: "RANK OR CARTOON? " 17, "RANK NUMBER? (1-21) " 20, "CARTOON NUMBER? (1-3) " 22)
constexpr std::size_t kPlayersLen = 29, kPressLen = 55, kHiddenLen = 17, kRankLen = 20, kCartoonLen = 22;
}  // namespace

bool AttractFlow::start(const datapack::Pack& pack, const std::string& romPath) {
    pack_ = &pack;
    if (!monitor_.loadRom(romPath)) return false;
    monitor_.reset();
    monitor_.poke(0x54E1, 1);
    mem_ = AttractMemory{};
    players_ = 1;
    monitor_.poke(0x6FBA, 0x88);                 // the keyboard mode's keys: left arrow, right arrow, A, Z
    monitor_.poke(0x6FB6, 0x95);
    monitor_.poke(0x6FB2, 0xC1);
    monitor_.poke(0x6FBE, 0xDA);
    mode_ = Mode::Attract;
    turnIndex_ = 0;
    lastCheck_ = -1;
    haveKey_ = false;
    paused_ = false;
    ctrlV_ = false;
    spaceAt_ = endAt_ = -1;
    return turn_.start(pack, mem_);
}

bool AttractFlow::startBoot(const datapack::Pack& pack, const std::string& romPath) {
    if (!start(pack, romPath)) return false;
    monitor_.poke(0x4E, 0);                      // the random counter starts at 0: it counts up in the key waits of the menus
    monitor_.poke(0x4F, 0);
    monitor_.setCycles(0);
    // $4027: LDA #1, STA $51, JSR $54D4 (the new-game state: measured 47,555 cycles), the reset vectors and flags, JSR $4821 -> JMP $48A6 (V: the oracle's marks, 47,637 cycles in all)
    monitor_.addCycles(47'637);
    monitor_.poke(0x51, 1);
    bootModeScreen();
    busy_ = monitor_.cycles();
    syncText();
    return true;
}

void AttractFlow::newTurn() {
    turn_.start(*pack_, mem_);
    ++turnIndex_;
    lastCheck_ = -1;
}

void AttractFlow::syncText() {
    std::array<std::uint8_t, 1024> page{};
    for (std::size_t i = 0; i < 1024; ++i) page[i] = monitor_.peek(static_cast<std::uint16_t>(0x800 + i));
    text_.assign(page, monitor_.peek(0x24), monitor_.peek(0x25));
    waitClock_ = monitor_.cycles();
    for (std::size_t i = 0; i < 256; ++i) waitZp_[i] = monitor_.peek(static_cast<std::uint16_t>(i));
}

std::array<std::uint8_t, 256> AttractFlow::lineBuffer() const {
    std::array<std::uint8_t, 256> b{};
    for (std::size_t i = 0; i < 256; ++i) b[i] = monitor_.peek(static_cast<std::uint16_t>(0x200 + i));
    return b;
}

// $4217: JSR $6F3C (text on), JSR $4868 (the page cleared), CH = 0, VTAB for the row of $25 (the LDA #6 before it is not used), the 29 bytes of the first line, JSR $FD0C
void AttractFlow::playersScreen() {
    monitor_.addCycles(6);
    monitor_.gameSetTextMode();
    monitor_.addCycles(6);
    monitor_.gameClearPage();
    monitor_.addCycles(2 + 3 + 2);
    monitor_.poke(0x24, 0);
    monitor_.addCycles(6);
    monitor_.gameVtab();
    const auto s = pack_->find(0x427C, 84);
    if (s.size() >= kPlayersLen) monitor_.gamePrint(s.first(kPlayersLen), 0x427C);
    monitor_.addCycles(6);
    monitor_.call(kRdkey);
    mode_ = Mode::Players;
    ctrlV_ = false;
}

// $4147: the same with row 0 and "RANK OR CARTOON? " (from $41DC), then JSR $FD6F
void AttractFlow::hiddenScreen() {
    monitor_.addCycles(6);
    monitor_.gameSetTextMode();
    monitor_.addCycles(6);
    monitor_.gameClearPage();
    monitor_.addCycles(2 + 3 + 2);
    monitor_.poke(0x24, 0);
    monitor_.addCycles(6);
    monitor_.gameVtab();
    const auto s = pack_->find(0x41DC, 59);
    if (s.size() >= 59) monitor_.gamePrint(s.first(kHiddenLen), 0x41DC);
    monitor_.addCycles(6);
    monitor_.call(kGetln);
    mode_ = Mode::RankOrCartoon;
}

// which: 1 = "CARTOON NUMBER? (1-3) " ($4201), 2 = "RANK NUMBER? (1-21) " ($41ED): the print loop and JSR $FD6F
void AttractFlow::linePrompt(int which) {
    const auto s = pack_->find(0x41DC, 59);
    if (s.size() >= 59) {
        if (which == 1) monitor_.gamePrint(s.subspan(kHiddenLen + kRankLen, kCartoonLen), 0x4201);
        else monitor_.gamePrint(s.subspan(kHiddenLen, kRankLen), 0x41ED);
    }
    monitor_.addCycles(6);
    monitor_.call(kGetln);
    mode_ = which == 1 ? Mode::CartoonNumber : Mode::RankNumber;
}

// $48A6: JSR $6F3C, JSR $4868, the cursor to the top left, VTAB, the text of $48D9 (ends with a zero byte), JSR $FD0C
void AttractFlow::bootModeScreen() {
    monitor_.addCycles(6);
    monitor_.gameSetTextMode();
    monitor_.addCycles(6);
    monitor_.gameClearPage();
    monitor_.addCycles(2 + 3 + 3);
    monitor_.poke(0x24, 0);
    monitor_.poke(0x25, 0);
    monitor_.addCycles(6);
    monitor_.gameVtab();
    const auto s = pack_->find(0x48D9, 86);
    monitor_.gamePrintZ(s, 0x48D9);
    monitor_.addCycles(6);
    monitor_.call(kRdkey);
    mode_ = Mode::BootMode;
}

// $492F: JSR $4868, JSR $6F3C, the cursor to the top left, VTAB, the 38 characters of $4A01, JSR $FD0C
void AttractFlow::bootKeyboardScreen() {
    monitor_.addCycles(6);
    monitor_.gameClearPage();
    monitor_.addCycles(6);
    monitor_.gameSetTextMode();
    monitor_.addCycles(2 + 3 + 3);
    monitor_.poke(0x24, 0);
    monitor_.poke(0x25, 0);
    monitor_.addCycles(6);
    monitor_.gameVtab();
    const auto s = pack_->find(0x4A01, 105);
    if (s.size() >= 38) monitor_.gamePrint(s.first(38), 0x4A01);
    monitor_.addCycles(6);
    monitor_.call(kRdkey);
    mode_ = Mode::BootKeyboard;
}

// the prompt for the key of direction `index` (0 left, 1 right, 2 up, 3 down) at column $16: the text at $4A3A, $4A46, $4A52, $4A5E (12 bytes each), then JSR $FD0C
void AttractFlow::bootKeyPrompt(int index) {
    const auto s = pack_->find(0x4A01, 105);
    const std::uint16_t addr = static_cast<std::uint16_t>(0x4A3A + 12 * index);
    if (s.size() >= static_cast<std::size_t>(addr - 0x4A01 + 12)) monitor_.gamePrint(s.subspan(addr - 0x4A01, 12), addr);
    monitor_.addCycles(6);
    monitor_.call(kRdkey);
    mode_ = Mode::BootKeys;
    keyIndex_ = index;
}

// the RTS of $4821 returns to $4061: JMP $43D2, then $52BB (11 cycles later) starts the first turn
void AttractFlow::finishBoot() {
    monitor_.addCycles(3 + 11);
    bootT0_ = monitor_.cycles();
    mode_ = Mode::Attract;
    turn_.start(*pack_, mem_);
    lastCheck_ = -1;
}

void AttractFlow::afterBootKey(std::uint8_t k) {
    if (mode_ == Mode::BootMode) {                 // $48C5
        monitor_.addCycles(2);
        if (k == 0xB1) {                           // BEQ $48D2: LDA #0, STA $F5, JMP $492F
            monitor_.addCycles(3 + 2 + 3 + 3);
            monitor_.poke(0xF5, 0);
            bootKeyboardScreen();
            return;
        }
        monitor_.addCycles(2 + 2);                 // BEQ (not taken), CMP #$B2
        if (k != 0xB2) {
            monitor_.addCycles(3 + 6);             // BNE $48C2, JSR $FD0C
            monitor_.call(kRdkey);
            return;
        }
        monitor_.addCycles(2 + 2 + 3 + 6);         // BNE (not taken), LDA #1, STA $F5, RTS
        monitor_.poke(0xF5, 1);
        finishBoot();
        return;
    }
    if (mode_ == Mode::BootKeyboard) {             // $494E
        monitor_.addCycles(2);
        if (k != 0xC3) {                           // BEQ (not taken), JMP $4A00, RTS
            monitor_.addCycles(2 + 3 + 6);
            finishBoot();
            return;
        }
        monitor_.addCycles(3 + 6);                 // BEQ $4955, JSR $4A6A: the C is echoed
        monitor_.gameCout(k);
        monitor_.addCycles(2 + 3 + 2 + 3);         // LDA #4, STA $25, LDA #0, STA $24
        monitor_.poke(0x25, 4);
        monitor_.poke(0x24, 0);
        monitor_.addCycles(6);
        monitor_.gameVtab();
        const auto s = pack_->find(0x4A01, 105);
        if (s.size() >= 0x4A26 - 0x4A01 + 20) monitor_.gamePrint(s.subspan(0x4A26 - 0x4A01, 20), 0x4A26);
        monitor_.addCycles(2 + 3 + 2 + 3);         // LDA #$16, STA $24, LDA #6, STA $25
        monitor_.poke(0x24, 0x16);
        monitor_.poke(0x25, 6);
        monitor_.addCycles(6);
        monitor_.gameVtab();
        bootKeyPrompt(0);
        return;
    }
    // BootKeys: the key for direction keyIndex_; it must differ from the earlier ones (CMP abs, BEQ back to the key wait)
    static constexpr std::uint16_t kAddr[4] = {0x6FBA, 0x6FB6, 0x6FB2, 0x6FBE};
    for (int j = 0; j < keyIndex_; ++j) {
        monitor_.addCycles(4);
        if (monitor_.peek(kAddr[j]) == k) {
            monitor_.addCycles(3 + 6);             // BEQ (taken), JSR $FD0C
            monitor_.call(kRdkey);
            return;
        }
        monitor_.addCycles(2);
    }
    monitor_.addCycles(4);                         // STA $6FBx
    monitor_.poke(kAddr[keyIndex_], k);
    if (keyIndex_ == 3) {
        monitor_.addCycles(6);                     // falls into $4A00: RTS
        finishBoot();
        return;
    }
    monitor_.addCycles(5 + 2 + 3);                 // INC $25, LDA #$16, STA $24
    monitor_.poke(0x25, static_cast<std::uint8_t>(monitor_.peek(0x25) + 1));
    monitor_.poke(0x24, 0x16);
    monitor_.addCycles(6);
    monitor_.gameVtab();
    bootKeyPrompt(keyIndex_ + 1);
}

void AttractFlow::enterPlayers(std::int64_t at) {
    mem_ = turn_.memoryNow();                    // what the turn leaves behind (the blink and penalty counters of the demo) starts the next one
    mem_.players = players_;
    spaceAt_ = at;
    spaceTurn_ = turnIndex_;
    endAt_ = -1;
    monitor_.setCycles(0);                       // the clock of the prompts counts from $6FAE
    monitor_.addCycles(3 + 3);                   // JMP $4144, JMP $4217
    playersScreen();
    busy_ = (at - turn_.now()) + monitor_.cycles();
    syncText();
}

void AttractFlow::takeKeyAtCheck(const KeyCheck& c) {
    const std::uint8_t k = latch_;
    if (c.toPoll > 0) {                 // $5292 / $52EE: only Space leaves from here; every other key stays in the latch for the next poll
        if (k != kKeySpace) return;
        haveKey_ = false;
        enterPlayers(c.at + c.toPoll + 36);   // (LDA, CMP, BEQ taken, [PLA, STA,] JMP $6F5E), then the poll up to $6FAE
        return;
    }
    haveKey_ = false;                   // $6F7C: STA $C010 clears the latch
    if (k == kKeyCtrlS) {
        soundOn_ = !soundOn_;
    } else if (k == kKeyEsc) {
        paused_ = true;
    } else if (k == kKeySpace) {
        enterPlayers(c.at + 36);        // $6FA6-$6FAE (E8 <> 0): JMP $4144
    }
}

// the rank number ($41AF-$41D9): one character: its low nibble is the rank if it is not 0; else the first nibble times 10 plus the second nibble, below 22
void AttractFlow::rankLine() {
    const unsigned len = monitor_.x();
    std::int64_t c = 2;                          // CPX #1
    int store = -1;
    if (len == 1) {
        c += 3 + 4 + 2;                          // BEQ (taken), LDA $0200, AND #$0F
        const int a = monitor_.peek(0x200) & 15;
        if (a != 0) {
            c += 3 + 4 + 3;                      // BNE (taken), STA $54E1, JMP $4217
            store = a;
        } else {
            c += 2 + 3 + 3;                      // BNE (not taken), JMP $4144, JMP $4217
        }
    } else {
        c += 2 + 4 + 2 + 2 + 4 + 2 + 2 + 2;      // BEQ, LDA, AND, TAX, LDA, AND, CPX #0, BEQ (not taken)
        const int x = monitor_.peek(0x200) & 15;
        int a = monitor_.peek(0x201) & 15;
        if (x == 0) {
            c += 1;                              // (BEQ taken: 3 instead of 2)
        } else {
            c += x * 6 + (x - 1) * 3 + 2;        // the loop: x passes of CLC / ADC / DEX; BNE is taken in all but the last
            a += 10 * x;
        }
        c += 2;                                  // CMP #$16
        if (a >= 0x16) {
            c += 3 + 3 + 3;                      // BCS (taken), JMP $4144, JMP $4217
        } else {
            c += 2 + 4 + 3;                      // BCS (not taken), STA $54E1, JMP $4217
            store = a;
        }
    }
    monitor_.addCycles(c);
    if (store >= 0) monitor_.poke(0x54E1, static_cast<std::uint8_t>(store));
    playersScreen();
}

// the line of the hidden menu is complete (GETLN returned with X = its length)
void AttractFlow::afterLine() {
    if (mode_ == Mode::RankOrCartoon) {          // $4166: LDA #7, JSR $485C (VTAB), LDA #0, STA $24, LDA $0200, CMP #$C3, BNE
        monitor_.addCycles(2 + 6);
        monitor_.gameVtab();
        monitor_.addCycles(2 + 3 + 4 + 2);
        monitor_.poke(0x24, 0);
        if (monitor_.peek(0x200) == 0xC3) {
            monitor_.addCycles(2);
            linePrompt(1);
        } else {
            monitor_.addCycles(3);
            linePrompt(2);
        }
        return;
    }
    if (mode_ == Mode::CartoonNumber) {          // $4186: LDX $0200, LDA #2, CPX #$B1, BEQ, LDA #5, CPX #$B2, BEQ, LDA #9, STA $55, JSR $8155
        const std::uint8_t k = monitor_.peek(0x200);
        std::uint8_t v;
        monitor_.addCycles(4 + 2 + 2);
        if (k == 0xB1) {
            monitor_.addCycles(3);
            v = 2;
            cartoon_ = 1;
        } else {
            monitor_.addCycles(2 + 2 + 2);
            if (k == 0xB2) {
                monitor_.addCycles(3);
                v = 5;
                cartoon_ = 2;
            } else {
                monitor_.addCycles(2 + 2);
                v = 9;
                cartoon_ = 3;
            }
        }
        monitor_.addCycles(3 + 6);
        monitor_.poke(0x55, v);
        mode_ = Mode::Cartoon;
        cartoonElapsed_ = 0;
        cartoonLength_ = kHiddenCartoonCycles[cartoon_ - 1];
        return;
    }
    rankLine();
}

// the RDKEY of a prompt returned with the key in A
void AttractFlow::afterPlayersKey(std::uint8_t k) {
    if (mode_ == Mode::Players && ctrlV_) {      // $4275: CMP #$83, BNE $4233 / JMP $4147
        monitor_.addCycles(2);
        ctrlV_ = false;
        if (k == kKeyCtrlC) {
            monitor_.addCycles(2 + 3);
            hiddenScreen();
        } else {
            monitor_.addCycles(3 + 6);
            monitor_.call(kRdkey);
        }
        return;
    }
    if (mode_ == Mode::Players) {                // $4236
        monitor_.addCycles(2);
        if (k == kKeyCtrlV) {                    // BEQ $4272: JSR $FD0C
            monitor_.addCycles(3 + 6);
            ctrlV_ = true;
            monitor_.call(kRdkey);
            return;
        }
        monitor_.addCycles(2 + 2);               // BEQ (not taken), CMP #$B1
        if (k < 0xB1) {
            monitor_.addCycles(3 + 6);           // BCC $4233, JSR $FD0C
            monitor_.call(kRdkey);
            return;
        }
        monitor_.addCycles(2 + 2);               // BCC (not taken), CMP #$B6
        if (k >= 0xB6) {
            monitor_.addCycles(3 + 6);           // BCS $4233, JSR $FD0C
            monitor_.call(kRdkey);
            return;
        }
        monitor_.addCycles(2 + 6);               // BCS (not taken), JSR $4A6A: the digit is echoed
        monitor_.gameCout(k);
        monitor_.addCycles(2 + 3);               // AND #7, STA $51
        players_ = k & 7;                             // $4245
        mem_.players = players_;                      // `$51` stays: the attract game that follows shows this many players (TASK-033-6c)
        monitor_.poke(0x51, static_cast<std::uint8_t>(players_));
        monitor_.addCycles(2 + 3 + 2 + 3);       // LDA #0, STA $24, LDA #$0B, STA $25
        monitor_.poke(0x24, 0);
        monitor_.poke(0x25, 0x0B);
        monitor_.addCycles(6);
        monitor_.gameVtab();
        const auto s = pack_->find(0x427C, 84);
        if (s.size() >= kPlayersLen + kPressLen) monitor_.gamePrint(s.subspan(kPlayersLen, kPressLen), 0x4299);
        monitor_.addCycles(6);
        monitor_.call(kRdkey);
        mode_ = Mode::PressReturn;
        return;
    }
    // PressReturn, $4264: CMP #$A0, BNE; CMP #$8D, BNE $4261
    monitor_.addCycles(2);
    if (k == kKeySpace) {
        monitor_.addCycles(2 + 3);               // BNE (not taken), JMP $43D2
        endAt_ = monitor_.cycles();
        mode_ = Mode::Attract;
        newTurn();
        return;
    }
    monitor_.addCycles(3 + 2);                   // BNE (taken), CMP #$8D
    if (k == kKeyReturn) {
        monitor_.addCycles(2 + 3);               // BNE (not taken), JMP $52FC
        endAt_ = monitor_.cycles();
        mode_ = Mode::Game;
        return;
    }
    monitor_.addCycles(3 + 6);                   // BNE $4261, JSR $FD0C
    monitor_.call(kRdkey);
}

void AttractFlow::advance(std::int64_t cycles) {
    for (;;) {
        if (mode_ == Mode::Game) return;
        if (mode_ == Mode::Cartoon) {                 // the intermission: it runs by itself (the caller draws it from cartoonElapsed()), then the players prompt again
            if (busy_ > 0) {
                const std::int64_t step = std::min(busy_, cycles);
                busy_ -= step;
                cycles -= step;
                if (busy_ > 0) return;
            }
            const std::int64_t step = std::min(cartoonLength_ - cartoonElapsed_, cycles);
            cartoonElapsed_ += step;
            cycles -= step;
            if (cartoonElapsed_ < cartoonLength_) return;
            monitor_.addCycles(cartoonLength_);       // $8155 returns: JMP $4144, JMP $4217: the players prompt again
            const std::int64_t before = monitor_.cycles();
            monitor_.addCycles(3 + 3);
            playersScreen();
            busy_ = monitor_.cycles() - before;
            syncText();
            continue;
        }
        if (prompting()) {
            if (busy_ > 0) {
                const std::int64_t step = std::min(busy_, cycles);
                busy_ -= step;
                cycles -= step;
                if (busy_ > 0) return;
            }
            if (!haveKey_) {                          // waiting for a key: the ROM loops in KEYIN (and counts $4E/$4F)
                if (cycles > 0) monitor_.spin(cycles);
                return;
            }
            haveKey_ = false;
            const std::int64_t before = monitor_.cycles();
            monitor_.pressKey(latch_);
            const Monitor::Status st = monitor_.resume();
            if (st == Monitor::Status::Done) {
                if (isBootMode()) afterBootKey(monitor_.a());
                else if (isLineMode()) afterLine();
                else afterPlayersKey(monitor_.a());
            }
            busy_ = monitor_.cycles() - before;
            syncText();
            continue;
        }
        if (paused_) {                                // $6F9A: waits for any key, which is then tested like a key of the poll
            if (!haveKey_) return;
            const std::uint8_t k = latch_;
            haveKey_ = false;
            paused_ = false;
            if (k == kKeySpace) {
                enterPlayers(turn_.now() + 12);
                return;
            }
            continue;
        }
        if (cycles <= 0) return;
        KeyCheck c;
        const bool has = turn_.nextCheck(lastCheck_, c);
        const std::int64_t target = turn_.now() + cycles;
        if (has && c.at <= target) {
            cycles -= c.at - turn_.now();
            turn_.advance(c.at - turn_.now());
            lastCheck_ = c.at;
            if (haveKey_) takeKeyAtCheck(c);
            if (prompting()) return;                  // (the rest of the call is dropped: the prompt is just being built)
            continue;
        }
        const std::int64_t before = turn_.now();
        turn_.advance(cycles);
        cycles -= turn_.now() - before;
        if (turn_.finished()) {
            mem_ = turn_.memoryNow();
            newTurn();
            continue;
        }
        return;
    }
}

}  // namespace pacman::core
