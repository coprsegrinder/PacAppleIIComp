// TASK-034-TIMING: the cycles of the original's game logic, counted where the port runs the same branch.
// The tick of a game costs its idle tick (gamedriver.h: kIdleTickCyclesGame, every routine on its shortest path) plus what the routines spend beyond that: the transliterated routines add
// the cycles of the path they take (cy), taken from the original's code (tools/block_costs.py: every instruction at its base cost, a taken branch one more, two more across a page,
// an indexed read one more across a page). The drawing routines are not counted here (attract.h drawCallCycles).
#pragma once

#include <cstdint>

namespace pacman::core {

class CycleMeter {
public:
    void add(std::int64_t n) noexcept { total_ += n; }
    std::int64_t total() const noexcept { return total_; }
    void reset() noexcept { total_ = 0; }

private:
    std::int64_t total_ = 0;
};

inline thread_local CycleMeter* g_cycleMeter = nullptr;   // set by GameCore while it ticks

inline void cy(std::int64_t n) noexcept {
    if (g_cycleMeter) g_cycleMeter->add(n);
}
// $1F: the original counts Pac-Man's steps in this byte of the zero page and clears it at $FB ($7992-$799C). It is the machine's, not the game's: it goes on counting from the attract game into the games
inline thread_local unsigned g_stepCounter1F = 0;

// a branch at an address whose next instruction is `next`, with its target: 2 not taken, 3 taken, 4 taken across a page
inline void cyBranch(bool taken, int next, int target) noexcept { cy(!taken ? 2 : ((next >> 8) != (target >> 8)) ? 4 : 3); }
// an indexed read: one more when the index carries over a page
inline int cyCross(int base, int index) noexcept { return ((base & 255) + index > 255) ? 1 : 0; }

// $7A6B / $8B13 (the same code with different counters): the body of the routine that makes the address of the maze cell (x, y) in $16/$17, RTS included; `address` = what it makes
inline int costCellAddress(unsigned x, unsigned y, unsigned& address) noexcept {
    int c = 2 + 3 + 3 + 8;                                 // LDA #$62, STA $17, LDA x, ASL x4
    unsigned hi = 0x62;
    unsigned a = (x << 4) & 255;
    if ((x & 0x10) == 0) {
        c += 3;                                            // BCC taken
    } else {
        c += 2 + 5 + 5;                                    // BCC, INC $17 twice
        hi += 2;
    }
    c += 2;                                                // ASL
    const bool c3 = (a & 0x80) != 0;
    a = (a << 1) & 255;
    if (!c3) {
        c += 3;
    } else {
        c += 2 + 5;
        hi += 1;
    }
    const unsigned t1 = a + y;                             // CLC, ADC y: its carry is taken into the next ADC (the routine's quirk)
    const unsigned t2 = (t1 & 255) + 0x3A + (t1 > 255 ? 1 : 0);
    c += 2 + 3 + 2;                                        // CLC, ADC y, ADC #$3A
    if (t2 < 256) {
        c += 3;
    } else {
        c += 2 + 5;
        hi += 1;
    }
    address = (hi << 8) | (t2 & 255);
    return c + 3 + 6;                                      // STA $16, RTS
}

// $79A4: the choice of Pac-Man's image from his step counter $66, up to and including the JMP $6A20 (the dispatcher's own cycles are a drawing call)
inline int cost79A4(unsigned z66) noexcept {
    const unsigned t3 = z66 & 3;
    int c = 10;                                            // LDA $66, AND #$0F, STA $66, AND #3
    if (t3 == 0) return c + 3 + 20;                        // BEQ taken, $79BE-$79CA
    c += 2 + 2;                                            // BEQ, CMP #3
    unsigned a = t3;
    if (t3 == 3) {
        c += 2 + 2;                                        // BNE, LDA #1
        a = 1;
    } else {
        c += 3;                                            // BNE taken
    }
    c += 2 + 3;                                            // ROR A, LDA $62
    c += (a & 1) != 0 ? 3 : 2 + 2 + 2;                     // BCS taken / BCS, ADC #4, SEC
    return c + 2 + 20;                                     // ADC #0, $79BE-$79CA
}

}  // namespace pacman::core
