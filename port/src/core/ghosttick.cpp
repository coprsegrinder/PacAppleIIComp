#include "ghosttick.h"

#include <algorithm>

namespace pacman::core {

bool GhostTables::load(const datapack::Pack& pack) {
    const auto t = pack.find(0x8723, 64);
    if (t.empty()) return false;
    std::copy(t.begin(), t.begin() + 16, normal.begin());
    std::copy(t.begin() + 16, t.begin() + 32, turn.begin());
    std::copy(t.begin() + 32, t.begin() + 48, frightened.begin());
    std::copy(t.begin() + 48, t.begin() + 64, eyes.begin());
    return true;
}

HeadResult ghostHead(GameState& s, int ghostNumber, GhostShared& sh, const GhostTables& tables) {
    HeadResult r;
    GhostRecord& g = s.ghosts[static_cast<std::size_t>(ghostNumber - 1) & 3];
    g.b[16] = static_cast<std::uint8_t>(g.b[16] - 1);          // $8763: DEC $80
    if (g.b[16] != 0) return r;
    r.acted = true;
    // $8768-$877D: while the frightened period runs every acting ghost counts the shared timer down by one
    if (sh.e4 != 0) {
        if (sh.e1 == 0) sh.e2 = static_cast<std::uint8_t>(sh.e2 - 1);
        sh.e1 = static_cast<std::uint8_t>(sh.e1 - 1);
        if (static_cast<unsigned>(sh.e1) + sh.e2 == 0) {      // BNE / BCS skip the call unless the sum is exactly zero
            // $6D49-$6D5F: the end of the frightened period
            for (auto& gh : s.ghosts) gh.b[8] = 0;
            g.b[8] = 0;                                      // $78 (the working copy of the current ghost)
            sh.e4 = 0;
            s.cornerPhase = 0;                               // $E9: chase
            s.zp(0x68) = 2;
            r.frightenedEnded = true;
        }
    }
    // $8780-$879D: the countdown for the next step from the level class and the ghost's state (frightened first, then eyes)
    const std::size_t x = std::min<std::size_t>(s.level(), 15);   // $6D1E
    unsigned reload = g.b[8] != 0 ? tables.frightened[x] : g.b[9] != 0 ? tables.eyes[x] : tables.normal[x];
    // $879F-$87B8: a ghost-number term, added when bit 1 of the looked-at coordinate is clear. CLC, ROR $E0 leaves bit 0 of $E0 in the carry, which ADC adds.
    const unsigned e0 = static_cast<unsigned>(ghostNumber);
    const std::uint8_t term = static_cast<std::uint8_t>((e0 >> 1) - 1);
    const unsigned carry = e0 & 1u;
    const std::uint8_t coordinate = (g.b[2] & 2) != 0 ? (g.b[1] != 0 ? g.b[1] : g.b[0]) : g.b[0];   // direction left/right: Y (X if Y is 0), else X
    if ((coordinate & 2) == 0) reload = static_cast<std::uint8_t>(reload + term + carry);
    if (g.b[10] != 0) reload = static_cast<std::uint8_t>(reload << 1);   // $87BA: ASL $80 when the slow flag $7A is set
    r.reload = static_cast<std::uint8_t>(reload);
    g.b[16] = r.reload;
    return r;
}

}  // namespace pacman::core
