#include "eating.h"

#include "cycles.h"

#include "lives.h"

namespace pacman::core {

namespace {
// One ADC in decimal mode: returns the BCD sum of a + b + carryIn and the carry out.
std::uint8_t bcdAdd(std::uint8_t a, std::uint8_t b, bool& carry) {
    unsigned lo = (a & 0x0Fu) + (b & 0x0Fu) + (carry ? 1u : 0u);
    unsigned hi = (a >> 4) + (b >> 4);
    if (lo > 9) {
        lo -= 10;
        ++hi;
    }
    carry = hi > 9;
    if (carry) hi -= 10;
    return static_cast<std::uint8_t>((hi << 4) | lo);
}
}  // namespace

void addToScore(GameState& s, std::uint8_t bcdLow) {
    bool carry = false;                          // CLC
    s.zp(0x52) = bcdAdd(s.zp(0x52), bcdLow, carry);
    s.zp(0x53) = bcdAdd(s.zp(0x53), 0, carry);
    s.zp(0x54) = bcdAdd(s.zp(0x54), 0, carry);
}

EatResult eatAt(GameState& s, const ItemTest& at, StepTimer& timer, const StepTables& tables, int player) {
    EatResult r;
    const std::size_t index = static_cast<std::size_t>(at.column) * 32 + at.row;   // $7A6B: $623A + 32 * column + row
    if (index >= s.map.size()) {                                                   // outside the map: the original would read the master copy behind it
        r.outsideMap = true;
        return r;
    }
    const std::uint8_t cell = s.map[index];
    // $7A5F: JSR $7A6B (the cell's address), LDY #0, LDA ($16),Y, BEQ; an empty cell ends with the RTS
    unsigned address;
    cy(6 + costCellAddress(at.column, at.row, address) + 2 + 5);
    if (cell == 0) {
        cy(3 + 6);                                                                 // BEQ taken, RTS
        return r;                                                                  // $7A64-$7A66: nothing here
    }
    cy(2 + 3);                                                                     // BEQ not taken, JMP $7AAC
    r.ate = true;
    timer.itemEaten(s, tables);                                                    // $7AAC-$7ABB
    cy(2 + 6 + (s.level() < 15 ? 14 : 15) + 2 + 4 + 3 + 3 + 4 + 3 + 5 + 3);         // CLC, JSR $6D1E (+ body), TAX, LDA $7A8C,X, ADC $59, STA $59, LDA $7A9C,X, STA $F3, LDA ($16),Y, LDX $E8
    if (s.demo != 0) {                                                             // $7B1F: the attract demo eats without score
        cy(4 + 2 + 2);                                                             // BNE taken across the page, CLC, ROR
        r.energizer = (cell >> 1) != 0;
        if (!r.energizer) cy(3);                                                   // BEQ taken
        else cy(2 + 6);                                                            // BEQ, JSR $6D60 (its body: energizerGhostEffects)
        cy(2 + 2 + 6 + 5 + 6);                                                     // $7B26: LDA #0, LDY #0, STA ($16),Y, DEC $56, RTS
        s.map[index] = 0;
        s.dotsLeft() = static_cast<std::uint8_t>(s.dotsLeft() - 1);                // $7B2C
        return r;
    }
    cy(2 + 2 + 2);                                                                 // BNE not taken, CLC, ROR
    if ((cell >> 1) == 0) {                                                        // $7AC3: ROR A, BEQ -> a dot
        cy(3);                                                                     // BEQ taken
        cy(30 + 6);                                                                // $7AF4: the decimal addition of 10 points, JSR $7BDC (the sound itself is counted apart)
        addToScore(s, 0x10);
    } else {                                                                       // an energizer
        cy(2 + kEnergizerTickCycles);
        r.energizer = true;
        addToScore(s, 0x50);
    }
    s.map[index] = 0;                                                              // $7B0C-$7B10
    cy(2 + 2 + 6 + 6);                                                             // LDA #0, LDY #0, STA ($16),Y, JSR $7091 (+ scoreRoutineCycles)
    awardBonusLife(s, player);                                                     // $7B12: JSR $7091 (the score routine contains the extra-life test)
    s.fruitCounter() = static_cast<std::uint8_t>(s.fruitCounter() - 1);            // $7B15
    s.dotsLeft() = static_cast<std::uint8_t>(s.dotsLeft() - 1);                    // $7B17
    r.levelComplete = s.dotsLeft() == 0;                                           // $7B19: BNE, else JMP $6CF4
    cy(5 + 5);                                                                     // DEC $57, DEC $56
    cy(r.levelComplete ? 2 + 3 : 3);                                               // BNE
    if (!r.levelComplete) cy(6);                                                   // RTS
    return r;
}

}  // namespace pacman::core
