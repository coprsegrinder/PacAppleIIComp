#include "pacstep.h"

#include "cycles.h"

namespace pacman::core {

namespace {

// $79FD-$7A5D: the cell counters follow the step. Directions 2 (right) and 3 (left) use $6C/$6A, 0 (down) and 1 (up) use $6D/$6B.
// The cycles: the routine ends with an RTS (counted here); at the point where the next cell is entered it asks the item routine $7A5F (counted by eatAt, which is called when this
// returns a test): from right / down by INC, JSR, DEC, RTS around it, from left / up by a branch.
std::optional<ItemTest> advanceCellCounters(GameState& s) {
    const std::uint8_t dir = s.pacDirection();
    const bool horizontal = (dir >> 1) != 0;                     // ROR A: A = dir >> 1, carry = bit 0
    const bool negative = (dir & 1) != 0;                        // odd directions (1 up, 3 left) count down
    std::uint8_t& sub = horizontal ? s.zp(0x6C) : s.zp(0x6D);
    std::uint8_t& cell = horizontal ? s.zp(0x6A) : s.zp(0x6B);
    std::optional<ItemTest> test;
    auto at = [&](int extra) {
        ItemTest t;
        t.column = static_cast<std::uint8_t>(horizontal ? s.zp(0x6A) + extra : s.zp(0x6A));
        t.row = static_cast<std::uint8_t>(horizontal ? s.zp(0x6B) : s.zp(0x6B) + extra);
        return t;
    };
    cy(2 + 3 + 2);                                               // $79FD: CLC, LDA $62, ROR
    cy(horizontal ? 2 : 3);                                      // BEQ (taken for the vertical directions: $7A31)
    cy(3);                                                       // LDA $6C / $6D
    if (!negative) {                                             // right / down
        cy(2 + 2 + 2);                                           // BCS not taken, ADC #2, CMP #6
        std::uint8_t v = static_cast<std::uint8_t>(sub + 2);
        if (v == 6) {
            cy(2 + 2 + 5);                                       // BNE, LDA #0, INC cell
            v = 0;
            cell = static_cast<std::uint8_t>(cell + 1);
        } else {
            cy(3);                                               // BNE taken
        }
        sub = v;
        cy(3 + 2);                                               // STA sub, CMP #2
        if (v == 2) {
            cy(2 + 5 + 6 + 5 + 6);                               // BNE, INC cell, JSR $7A5F, DEC cell, RTS (the routine itself: eatAt)
            test = at(1);                                        // the point where the next cell is entered
        } else {
            cy(3 + 6);                                           // BNE taken, RTS
        }
    } else {                                                     // left / up
        cy(3 + 2 + 2);                                           // BCS taken, SBC #2, CMP #$FE
        std::uint8_t v = static_cast<std::uint8_t>(sub - 2);
        if (v == 0xFE) {
            cy(2 + 2 + 5);                                       // BNE, LDA #4, DEC cell
            v = 4;
            cell = static_cast<std::uint8_t>(cell - 1);
        } else {
            cy(3);                                               // BNE taken
        }
        sub = v;
        cy(3 + 2);                                               // STA sub, CMP #4
        if (v == 4) {
            cy(2 + 3);                                           // BNE, BEQ taken to $7A5F (whose RTS ends the routine)
            test = at(0);
        } else {
            cy(3 + 6);                                           // BNE taken, RTS
        }
    }
    return test;
}

}  // namespace

std::optional<ItemTest> stepPacman(GameState& s, JunctionOutcome outcome) {
    if (outcome == JunctionOutcome::Blocked) return std::nullopt;   // $7905-$7912: only redrawn standing; the counters do not move
    const std::uint8_t dir = s.pacDirection();
    std::uint8_t& x = s.pacX();
    std::uint8_t& y = s.pacY();

    if (outcome == JunctionOutcome::Tunnel) {                       // $7B2F-$7BAA: the tunnel is a straight run along the row with a jump at the edge
        const bool atLeftEnd = s.pacJunction() == 0x1A;
        const bool odd = (dir & 1) != 0;
        cy(6 + 23 + 3 + 2);                                         // JSR $7D80 (+ body), LDA $64, CMP #$1A (the JSR $699E is a drawing call)
        if (!atLeftEnd) {                                           // at 29: right edge
            cy(2 + 3 + 2);                                          // BEQ not taken, LDA $62, ROR
            if (odd) {
                cy(2 + 3 + 2 + 3 + 3);                              // BCC not taken, LDA $60, SBC #2, STA $60, STA $0C
                x = static_cast<std::uint8_t>(x - 2);
            } else {
                cy(3 + 2 + 3 + 2 + 3 + 3 + 2);                      // BCC taken, CLC, LDA $60, ADC #2, STA, STA, CMP #$DD
                x = static_cast<std::uint8_t>(x + 2);
                if (x == 0xDD) {                                    // out of the right edge: reappear at the left
                    cy(2 + 29 + 3);                                 // BNE not taken, $7B61-$7B75, BEQ taken
                    s.pacJunction() = s.pacNextJunction();
                    s.pacNextJunction() = 0x1D;
                    x = 0x3D;
                    s.zp(0x6A) = 0;
                    s.zp(0x6C) = 0;
                } else {
                    cy(3);                                          // BNE taken
                }
            }
        } else {                                                    // at 26: left edge
            cy(3 + 3 + 2);                                          // BEQ taken, LDA $62, ROR
            if (!odd) {
                cy(2 + 3 + 2 + 3 + 3 + 3);                          // BCS not taken, LDA $60, ADC #2, STA, STA, BNE taken
                x = static_cast<std::uint8_t>(x + 2);
            } else {
                cy(3 + 3 + 2 + 3 + 3 + 2);                          // BCS taken, LDA $60, SBC #2, STA, STA, CMP #$3B
                x = static_cast<std::uint8_t>(x - 2);
                if (x == 0x3B) {                                    // out of the left edge: reappear at the right
                    cy(2 + 29 + 3);                                 // BNE not taken, $7B94-$7BA8, BEQ taken
                    s.pacJunction() = s.pacNextJunction();
                    s.pacNextJunction() = 0x1A;
                    x = 0xDB;
                    s.zp(0x6A) = 0x1C;
                    s.zp(0x6C) = 0;
                } else {
                    cy(3);                                          // BNE taken
                }
            }
        }
        s.zp(0x66) = static_cast<std::uint8_t>((s.zp(0x66) + 1) & 0x0F);   // INC $66, then the draw routine $79A4 keeps only the low four bits
        cy(3 + 3 + 5 + 6 + cost79A4(s.zp(0x66)) + 3);               // $7B48: LDA $61, STA $10, INC $66, JSR $79A4 (+ body), JMP $79FD
        return advanceCellCounters(s);
    }
    // $7962-$7988: 2 units along the direction
    const bool horizontal = (dir >> 1) != 0;
    const bool negative = (dir & 1) != 0;
    std::uint8_t& axis = horizontal ? x : y;
    axis = static_cast<std::uint8_t>(negative ? axis - 2 : axis + 2);
    cy(dir == 0 ? 48 : dir == 1 ? 49 : dir == 2 ? 55 : 53);          // JSR $7D80 (+ body), CLC, LDA $62, ROR, the direction's branches and the load / store
    cy(27);                                                         // $7988-$7998: the stores, INC $66, INC $1F, LDA $1F, CMP #$FB
    if (++g_stepCounter1F == 0xFB) {
        g_stepCounter1F = 0;
        cy(2 + 2 + 3);                                              // BNE not taken, LDA #0, STA $1F
    } else {
        cy(3);                                                      // BNE taken
    }
    s.zp(0x66) = static_cast<std::uint8_t>((s.zp(0x66) + 1) & 0x0F);   // INC $66, then the draw routine $79A4 keeps only the low four bits
    cy(6 + cost79A4(s.zp(0x66)) + 3);                               // JSR $79A4 (+ body up to the JMP $6A20), JMP $79CD
    return advanceCellCounters(s);
}

}  // namespace pacman::core
