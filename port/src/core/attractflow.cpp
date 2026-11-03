#include "attractflow.h"

#include <algorithm>

#include "cartoon.h"

namespace pacman::core {

bool AttractFlow::start(const datapack::Pack& pack) {
    pack_ = &pack;
    mode_ = Mode::Attract;
    turnIndex_ = 0;
    lastCheck_ = -1;
    haveKey_ = false;
    paused_ = false;
    spaceAt_ = endAt_ = -1;
    waitAt_[0] = waitAt_[1] = -1;
    return turn_.start(pack);
}

void AttractFlow::newTurn() {
    turn_.start(*pack_);
    ++turnIndex_;
    lastCheck_ = -1;
}

void AttractFlow::showPlayersText() {
    // $4217: text on, the page cleared ($4868: cursor to the top left), the first line from $427C (29 bytes; the row is not set: $25 stays 0)
    text_.clear();
    const auto s = pack_->find(0x427C, 84);
    if (s.size() >= 29) text_.puts(s.first(29));
}

void AttractFlow::enterPlayers(std::int64_t at) {
    mode_ = Mode::Players;
    ctrlV_ = false;
    spaceAt_ = at;
    spaceTurn_ = turnIndex_;
    waitAt_[0] = at + kTakenToPlayersWait;
    waitAt_[1] = -1;
    endAt_ = -1;
    // the wait is reached when the RDKEY call has been made (6 cycles after the mark $4233); the clock counts from $6FAE
    wait_ = std::max<std::int64_t>(0, waitAt_[0] + 6 - turn_.now());
    promptClock_ = turn_.now() - at;
    showPlayersText();
}

// which: 0 = "RANK OR CARTOON? " (from $41DC, 17 bytes), 1 = "CARTOON NUMBER? (1-3) " ($4201, 22 bytes), 2 = "RANK NUMBER? (1-21) " ($41ED, 20 bytes)
void AttractFlow::showLinePrompt(int which) {
    static constexpr int kOffset[3] = {0, 0x4201 - 0x41DC, 0x41ED - 0x41DC}, kLength[3] = {17, 22, 20};
    const auto s = pack_->find(0x41DC, 59);
    if (s.size() >= 59) text_.puts(s.subspan(static_cast<std::size_t>(kOffset[which]), static_cast<std::size_t>(kLength[which])));
    lineX_ = 0;
    escPending_ = false;
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

void AttractFlow::lineRestart() {                  // $FD67: a new line below, the prompt character, X = 0
    text_.put(0x8D);
    text_.put(prompt_);
    lineX_ = 0;
}

// the line input $FD6F (monitor ROM $FD35-$FD8E): Esc takes the next key as a cursor command (not ported), Ctrl-U takes the character under the cursor, lower case becomes upper case,
// Return ends the line (CLREOL, CR), Backspace and Ctrl-X as in the ROM, every other character is echoed and stored
void AttractFlow::lineKey(std::uint8_t k) {
    if (escPending_) {
        escPending_ = false;
        return;
    }
    if (k == kKeyEsc) {
        escPending_ = true;
        return;
    }
    std::uint8_t a = k;
    if (a == 0x95) a = text_.atCursor();
    if (a >= 0xE0) a &= 0xDF;
    line_[lineX_] = a;
    if (a == kKeyReturn) {
        text_.clearToEndOfLine();
        text_.put(0x8D);
        lineDone();
        return;
    }
    text_.put(a);
    if (a == kKeyBackspace) {
        if (lineX_ == 0) lineRestart();
        else --lineX_;
        return;
    }
    if (a == kKeyCtrlX) {
        text_.put(0xDC);
        lineRestart();
        return;
    }
    lineX_ = (lineX_ + 1) & 255;
    if (lineX_ == 0) {                              // 256 characters: the line is cancelled
        text_.put(0xDC);
        lineRestart();
    }
}

void AttractFlow::lineDone() {
    const unsigned len = lineX_;
    lineX_ = 0;
    if (mode_ == Mode::RankOrCartoon) {             // $4166-$4176: the first letter decides ($C3 = C)
        if (line_[0] == 0xC3) {
            mode_ = Mode::CartoonNumber;
            showLinePrompt(1);
            wait_ = kLineToCartoonPrompt;
        } else {
            mode_ = Mode::RankNumber;
            showLinePrompt(2);
            wait_ = kLineToRankPrompt;
        }
        return;
    }
    if (mode_ == Mode::CartoonNumber) {             // $4186-$4197: 1 -> 2, 2 -> 5, others -> 9 (the level numbers of the three intermissions)
        cartoon_ = line_[0] == 0xB1 ? 1 : line_[0] == 0xB2 ? 2 : 3;
        mode_ = Mode::Cartoon;
        cartoonElapsed_ = 0;
        cartoonLength_ = kCartoonCycles[cartoon_ - 1] - kCartoonGolden;
        wait_ = kCartoonLineToStart;
        return;
    }
    // the rank number ($41AF-$41D9): one character: its low nibble is the rank if it is not 0; else the first nibble times 10 plus the second nibble, below 22
    std::int64_t c = 6 + 2;
    int store = -1;
    if (len == 1) {
        c += 3 + 4 + 2;
        const int a = line_[0] & 15;
        if (a != 0) {
            c += 3 + 4 + 3;
            store = a;
        } else {
            c += 2 + 3 + 3;
        }
    } else {
        c += 2 + 4 + 2 + 2 + 4 + 2 + 2 + 2;
        const int x = line_[0] & 15;
        int a = line_[1] & 15;
        if (x == 0) {
            c += 1;                               // (BEQ taken: 3 instead of 2)
        } else {
            c += x * 6 + (x - 1) * 3 + 2;         // the loop: x passes of CLC / ADC / DEX; BNE is taken in all but the last
            a += 10 * x;
        }
        c += 2;
        if (a >= 0x16) {
            c += 3 + 3 + 3;
        } else {
            c += 2 + 4 + 3;
            store = a;
        }
    }
    if (store >= 0) rank_ = store;
    mode_ = Mode::Players;
    ctrlV_ = false;
    showPlayersText();
    wait_ = c + 11'282 + 6;
}

void AttractFlow::processPromptKey() {
    const std::uint8_t k = latch_;
    haveKey_ = false;
    if (isLineMode()) {
        lineKey(k);
        return;
    }
    if (mode_ == Mode::Players) {
        if (ctrlV_) {                                 // $4272: Ctrl-C opens the hidden menu, any other key goes back to the key wait
            ctrlV_ = false;
            if (k == kKeyCtrlC) {
                mode_ = Mode::RankOrCartoon;          // $4147: text on, clear, "RANK OR CARTOON? ", the line input
                text_.clear();
                showLinePrompt(0);
                wait_ = kCtrlCToLine;
            } else {
                wait_ = kOtherKeyBack;
            }
            return;
        }
        if (k == kKeyCtrlV) {                         // $4236
            ctrlV_ = true;
            wait_ = kCtrlVToSecondKey;
            return;
        }
        if (k < 0xB1 || k > 0xB5) return;             // $4233-$423E: other keys are ignored
        players_ = k & 7;                             // $4245
        text_.put(k);                                 // the digit is echoed at the cursor
        text_.setCursor(0, 11);                       // $4249-$4251: column 0, row $0B
        const auto s = pack_->find(0x427C, 84);       // (the second text is at $4299, 55 bytes: the table of the data pack starts at $427C)
        if (s.size() >= 0x1D + 55) text_.puts(s.subspan(0x1D, 55));
        mode_ = Mode::PressReturn;
        waitAt_[1] = waitAt_[0] + kDigitToPressWait;
        wait_ = kDigitToPressWait;
        return;
    }
    if (k == kKeySpace) {                             // $4264: back to the demo: $43D2, then a new turn
        endAt_ = waitAt_[1] + (kSpaceToTurn - 11);
        mode_ = Mode::Attract;
        newTurn();
    } else if (k == kKeyReturn) {                     // $426B: JMP $52FC
        endAt_ = waitAt_[1] + kReturnToGame;
        mode_ = Mode::Game;
    }
}

void AttractFlow::advance(std::int64_t cycles) {
    for (;;) {
        if (mode_ == Mode::Game) return;
        if (mode_ == Mode::Cartoon) {                 // the intermission: it runs by itself (the caller draws it from cartoonElapsed()), then the players prompt again
            if (wait_ > 0) {
                const std::int64_t step = std::min(wait_, cycles);
                wait_ -= step;
                cycles -= step;
                promptClock_ += step;
                if (wait_ > 0) return;
            }
            const std::int64_t step = std::min(cartoonLength_ - cartoonElapsed_, cycles);
            cartoonElapsed_ += step;
            cycles -= step;
            promptClock_ += step;
            if (cartoonElapsed_ < cartoonLength_) return;
            mode_ = Mode::Players;                    // $4144: the players prompt (the prompts' own cycles to its key wait follow)
            ctrlV_ = false;
            showPlayersText();
            wait_ = kCartoonEndToPlayersWait + 6;
            continue;
        }
        if (prompting()) {
            if (wait_ > 0) {
                const std::int64_t step = std::min(wait_, cycles);
                wait_ -= step;
                cycles -= step;
                promptClock_ += step;
                if (wait_ > 0) return;
            }
            if (!haveKey_) return;                    // waiting for a key (the time is not accounted)
            processPromptKey();
            continue;
        }
        if (paused_) {                                // $6F9A: waits for any key, which is then tested like a key of the poll
            if (!haveKey_) return;
            const std::uint8_t k = latch_;
            haveKey_ = false;
            paused_ = false;
            if (k == kKeySpace) enterPlayers(turn_.now() + 12);
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
            continue;
        }
        const std::int64_t before = turn_.now();
        turn_.advance(cycles);
        cycles -= turn_.now() - before;
        if (turn_.finished()) {
            newTurn();
            continue;
        }
        return;
    }
}

}  // namespace pacman::core
