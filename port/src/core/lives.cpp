#include "lives.h"

#include "eating.h"

namespace pacman::core {

Contact ghostContact(const GameState& s, const GhostRecord& g) noexcept {
    const unsigned px = s.pacX(), py = s.pacY(), gx = g.b[0], gy = g.b[1];
    unsigned d;
    if (px == gx) d = py > gy ? py - gy : gy - py;
    else if (py == gy) d = px > gx ? px - gx : gx - px;
    else return Contact::None;
    if (d >= 10) return Contact::None;
    return d >= 6 ? Contact::Near : Contact::Touch;
}

void ghostEaten(GameState& s, std::size_t i, int player) {
    GhostRecord& g = s.ghosts[i & 3];
    g.b[9] = 1;   // $79 := 1 (eyes)
    g.b[8] = 0;   // $78 := 0
    if (s.demo != 0) return;
    // $8BC3-$8BD6: decimal mode; the step $68 is added to the HUNDREDS byte $53, then doubled
    bool carry = false;
    const unsigned step = s.zp(0x68);
    unsigned lo = (s.zp(0x53) & 0x0Fu) + (step & 0x0Fu);
    unsigned hi = (s.zp(0x53) >> 4) + (step >> 4);
    if (lo > 9) { lo -= 10; ++hi; }
    carry = hi > 9;
    if (carry) hi -= 10;
    s.zp(0x53) = static_cast<std::uint8_t>((hi << 4) | lo);
    unsigned high = (s.zp(0x54) & 0x0Fu) + (carry ? 1u : 0u);
    unsigned highHi = s.zp(0x54) >> 4;
    if (high > 9) { high -= 10; ++highHi; }
    s.zp(0x54) = static_cast<std::uint8_t>(((highHi > 9 ? highHi - 10 : highHi) << 4) | high);
    unsigned dlo = (step & 0x0Fu) * 2, dhi = (step >> 4) * 2;
    if (dlo > 9) { dlo -= 10; ++dhi; }
    if (dhi > 9) dhi -= 10;
    s.zp(0x68) = static_cast<std::uint8_t>((dhi << 4) | dlo);
    awardBonusLife(s, player);
}

void pacmanCaught(GameState& s) noexcept {
    std::uint8_t lives = static_cast<std::uint8_t>(s.lives() - 1);   // DEC $58
    if ((lives & 0x7F) == 0) lives = 0;                               // $8C12-$8C18
    s.zp(0x58) = lives;
}

void awardBonusLife(GameState& s, int player) noexcept {
    if (player == 0) return;                      // $7124-$7126
    if ((s.zp(0x58) & 0x80) == 0) return;         // $7128-$712A: BIT $58, BPL
    if (s.zp(0x54) < 1) return;                   // $712C-$7130: 10,000 points or more
    s.zp(0x58) = static_cast<std::uint8_t>(((s.zp(0x58) + 1) & 0x7F));   // INC $58, AND #$7F
}

}  // namespace pacman::core
