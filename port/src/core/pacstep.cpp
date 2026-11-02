#include "pacstep.h"

namespace pacman::core {

namespace {

// $79FD-$7A5D: the cell counters follow the step. Directions 2 (right) and 3 (left) use $6C/$6A, 0 (down) and 1 (up) use $6D/$6B.
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
    if (!negative) {                                             // right / down
        std::uint8_t v = static_cast<std::uint8_t>(sub + 2);
        if (v == 6) {
            v = 0;
            cell = static_cast<std::uint8_t>(cell + 1);
        }
        sub = v;
        if (v == 2) test = at(1);                                // the point where the next cell is entered
    } else {                                                     // left / up
        std::uint8_t v = static_cast<std::uint8_t>(sub - 2);
        if (v == 0xFE) {
            v = 4;
            cell = static_cast<std::uint8_t>(cell - 1);
        }
        sub = v;
        if (v == 4) test = at(0);
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
        if (!atLeftEnd) {                                           // at 29: right edge
            if (odd) {
                x = static_cast<std::uint8_t>(x - 2);
            } else {
                x = static_cast<std::uint8_t>(x + 2);
                if (x == 0xDD) {                                    // out of the right edge: reappear at the left
                    s.pacJunction() = s.pacNextJunction();
                    s.pacNextJunction() = 0x1D;
                    x = 0x3D;
                    s.zp(0x6A) = 0;
                    s.zp(0x6C) = 0;
                }
            }
        } else {                                                    // at 26: left edge
            if (!odd) {
                x = static_cast<std::uint8_t>(x + 2);
            } else {
                x = static_cast<std::uint8_t>(x - 2);
                if (x == 0x3B) {                                    // out of the left edge: reappear at the right
                    s.pacJunction() = s.pacNextJunction();
                    s.pacNextJunction() = 0x1A;
                    x = 0xDB;
                    s.zp(0x6A) = 0x1C;
                    s.zp(0x6C) = 0;
                }
            }
        }
    } else {                                                        // $7968-$7988: 2 units along the direction
        const bool horizontal = (dir >> 1) != 0;
        const bool negative = (dir & 1) != 0;
        std::uint8_t& axis = horizontal ? x : y;
        axis = static_cast<std::uint8_t>(negative ? axis - 2 : axis + 2);
    }
    s.zp(0x66) = static_cast<std::uint8_t>((s.zp(0x66) + 1) & 0x0F);   // INC $66, then the draw routine $79A4 keeps only the low four bits
    return advanceCellCounters(s);
}

}  // namespace pacman::core
