#include "monitor.h"

#include <fstream>
#include <iterator>
#include <vector>

namespace pacman::core {

bool Monitor::loadRom(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    const std::vector<char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() != 2048) return false;
    for (std::size_t i = 0; i < 2048; ++i) mem_[0xF800 + i] = static_cast<std::uint8_t>(d[i]);
    loaded_ = true;
    return true;
}

void Monitor::reset() {
    for (std::size_t i = 0; i < 0xF800; ++i) mem_[i] = 0;
    mem_[0x20] = 0;      // WNDLFT
    mem_[0x21] = 40;     // WNDWDTH
    mem_[0x22] = 0;      // WNDTOP
    mem_[0x23] = 24;     // WNDBTM
    mem_[0x28] = 0x00;   // BASL/BASH: row 0 of text page 1
    mem_[0x29] = 0x04;
    mem_[0x32] = 0xFF;   // INVFLG: normal characters
    mem_[0x33] = 0xDD;   // PROMPT: ']' (I: as the program is started from DOS 3.3 and Applesoft)
    mem_[0x36] = 0xF0;   // CSW = COUT1
    mem_[0x37] = 0xFD;
    mem_[0x38] = 0x1B;   // KSW = KEYIN
    mem_[0x39] = 0xFD;
    mem_[0x4E] = 2;      // RNDL: the state the oracle's boot leaves (tools/capture_prompts.py): it counts up while a key is awaited
    kbd_ = 0;
    waiting_ = false;
    calling_ = false;
    textPage2_ = true;
    spinDebt_ = 0;
    cpu_.sp = 0xFD;
    cpu_.p = 0x24;
    cpu_.cycles = 0;
}

std::uint8_t Monitor::read(std::uint16_t a) {
    if (a >= 0xC000 && a <= 0xC0FF) {
        if (a == 0xC000) return kbd_;
        if (a == 0xC010) {
            kbd_ &= 0x7F;
            return kbd_;
        }
        return 0;
    }
    return mem_[a];
}

void Monitor::write(std::uint16_t a, std::uint8_t v) {
    if (a >= 0xC000) {
        if (a == 0xC010) kbd_ &= 0x7F;      // (the speaker and the video switches have no effect here)
        return;                             // ROM and I/O
    }
    mem_[a] = v;
}

Monitor::Status Monitor::run() {
    for (;;) {
        if (cpu_.pc == kSentinel) {
            waiting_ = false;
            calling_ = false;
            return Status::Done;
        }
        if (cpu_.pc == 0xFD21 && !keyLatched()) {       // KEYIN: BIT $C000 with no key: wait here
            waiting_ = true;
            return Status::Waiting;
        }
        cpu_.step();
        if (cpu_.failed()) {
            calling_ = false;
            return Status::Failed;
        }
    }
}

Monitor::Status Monitor::call(std::uint16_t address) {
    const std::uint16_t ret = static_cast<std::uint16_t>(kSentinel - 1);
    cpu_.push(static_cast<std::uint8_t>(ret >> 8));
    cpu_.push(static_cast<std::uint8_t>(ret & 0xFF));
    cpu_.pc = address;
    calling_ = true;
    return run();
}

Monitor::Status Monitor::resume() {
    waiting_ = false;
    return run();
}

void Monitor::spin(std::int64_t n) {
    const std::int64_t target = cpu_.cycles + n - spinDebt_;
    while (cpu_.cycles < target) {
        cpu_.step();
        if (cpu_.failed()) return;
    }
    spinDebt_ = cpu_.cycles - target;
}

// ---- the game's own text routines -------------------------------------------------------------------------------------------------------------------------------------------

void Monitor::gameSetTextMode() {
    cpu_.cycles += 4 + 4 + 4 + 6;            // LDA $C051, LDA $C055, LDA $C052, RTS
    textPage2_ = true;
}

void Monitor::gameClearPage() {
    // $4868: LDA #0, STA $25, STA $24, LDX #4, then 4 x (LDY #0, LDA #$A0, 256 x (STA $0800+,Y; INY; BNE), INC the high byte of the operand, DEX, BNE), LDA #8, STA operand, RTS
    cpu_.cycles += 2 + 3 + 3 + 2;
    for (int block = 0; block < 4; ++block) {
        cpu_.cycles += (block == 0 ? 2 : 0) + 2 + 256 * (5 + 2) + 255 * 3 + 2 + 6 + 2 + (block < 3 ? 3 : 2);   // (LDY #0 only once: the outer loop jumps back to LDA #$A0)
        for (int i = 0; i < 256; ++i) mem_[static_cast<std::size_t>(0x800 + block * 256 + i)] = 0xA0;
    }
    cpu_.cycles += 2 + 4 + 6;
    mem_[0x25] = 0;
    mem_[0x24] = 0;
    cpu_.x = 0;
    cpu_.y = 0;
    cpu_.a = 8;
}

Monitor::Status Monitor::gameVtab() {
    cpu_.cycles += 6;                        // JSR $FC22
    const Status s = call(0xFC22);
    if (s != Status::Done) return s;
    cpu_.cycles += 3 + 2 + 2 + 3 + 6;        // LDA $29, AND #3, ORA #8, STA $29, RTS
    mem_[0x29] = static_cast<std::uint8_t>((mem_[0x29] & 3) | 8);
    cpu_.a = mem_[0x29];
    return Status::Done;
}

Monitor::Status Monitor::gameCout(std::uint8_t c) {
    cpu_.a = c;
    cpu_.cycles += 2;                        // CMP #$8D
    if (c != 0x8D) {
        cpu_.cycles += 3 + 3;                // BNE (taken), JMP $FDF0
        return call(0xFDF0);
    }
    cpu_.cycles += 2 + 5 + 2 + 3 + 3;        // BNE (not taken), INC $25, LDA #0, STA $24, JMP $485C
    ++mem_[0x25];
    mem_[0x24] = 0;
    return gameVtab();                       // ($485C itself is entered by a JMP: its own JSR $FC22 is counted inside)
}

Monitor::Status Monitor::gamePrint(std::span<const std::uint8_t> s, std::uint16_t tableAddress) {
    // LDX #0; loop: LDA table,X (one more cycle when the index crosses a page); JSR $4A6A; INX; CPX #n; BCC/BNE loop
    cpu_.cycles += 2;
    cpu_.x = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        cpu_.cycles += 4 + (((tableAddress & 0xFF) + i > 0xFF) ? 1 : 0) + 6;
        const Status st = gameCout(s[i]);
        if (st != Status::Done) return st;
        cpu_.x = static_cast<std::uint8_t>(cpu_.x + 1);
        cpu_.cycles += 2 + 2 + (i + 1 < s.size() ? 3 : 2);
    }
    return Status::Done;
}

}  // namespace pacman::core
