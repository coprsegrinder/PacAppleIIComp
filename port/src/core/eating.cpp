#include "eating.h"

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

EatResult eatAt(GameState& s, const ItemTest& at, StepTimer& timer, const StepTables& tables) {
    EatResult r;
    const std::size_t index = static_cast<std::size_t>(at.column) * 32 + at.row;   // $7A6B: $623A + 32 * column + row
    if (index >= s.map.size()) {                                                   // outside the map: the original would read the master copy behind it
        r.outsideMap = true;
        return r;
    }
    const std::uint8_t cell = s.map[index];
    if (cell == 0) return r;                                                       // $7A64-$7A66: nothing here
    r.ate = true;
    timer.itemEaten(s, tables);                                                    // $7AAC-$7ABB
    if (s.demo != 0) {                                                             // $7B1F: the attract demo eats without score
        r.energizer = (cell >> 1) != 0;
        s.map[index] = 0;
        s.dotsLeft() = static_cast<std::uint8_t>(s.dotsLeft() - 1);                // $7B2C
        return r;
    }
    if ((cell >> 1) == 0) {                                                        // $7AC3: ROR A, BEQ -> a dot
        addToScore(s, 0x10);
    } else {                                                                       // an energizer
        r.energizer = true;
        addToScore(s, 0x50);
    }
    s.map[index] = 0;                                                              // $7B0C-$7B10
    s.fruitCounter() = static_cast<std::uint8_t>(s.fruitCounter() - 1);            // $7B15
    s.dotsLeft() = static_cast<std::uint8_t>(s.dotsLeft() - 1);                    // $7B17
    r.levelComplete = s.dotsLeft() == 0;                                           // $7B19: BNE, else JMP $6CF4
    return r;
}

}  // namespace pacman::core
