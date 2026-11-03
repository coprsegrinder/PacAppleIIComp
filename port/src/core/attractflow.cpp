#include "attractflow.h"

#include <algorithm>

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

void AttractFlow::enterPlayers(std::int64_t at) {
    mode_ = Mode::Players;
    spaceAt_ = at;
    spaceTurn_ = turnIndex_;
    waitAt_[0] = at + kTakenToPlayersWait;
    waitAt_[1] = -1;
    endAt_ = -1;
    wait_ = std::max<std::int64_t>(0, waitAt_[0] - turn_.now());
    // $4217: text on, the page cleared ($4868: cursor to the top left), the first line from $427C (29 bytes; the row is not set: $25 stays 0)
    text_.clear();
    const auto s = pack_->find(0x427C, 84);
    if (s.size() >= 29) text_.puts(s.first(29));
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

void AttractFlow::processPromptKey() {
    const std::uint8_t k = latch_;
    haveKey_ = false;
    if (mode_ == Mode::Players) {
        if (k < 0xB1 || k > 0xB5) return;             // $4233-$423E: other keys are ignored (the Ctrl-V Ctrl-C hidden menu is not ported yet)
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
        if (prompting()) {
            if (wait_ > 0) {
                const std::int64_t step = std::min(wait_, cycles);
                wait_ -= step;
                cycles -= step;
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
