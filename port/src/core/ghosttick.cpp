#include "ghosttick.h"

#include "cycles.h"

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
    if (g.b[16] != 0) return r;                                // (13 cycles: DEC 5, BEQ 2, RTS 6; they are in the idle tick)
    r.acted = true;
    cy(-13 + 5 + 3);                                           // the acting ghost's own path instead: DEC $80, BEQ taken (its RTS comes at the end of the step, ghostStepEnd)
    // $8768-$877D: while the frightened period runs every acting ghost counts the shared timer down by one
    cy(3);                                                     // LDA $E4
    if (sh.e4 == 0) {
        cy(3);                                                 // BEQ taken
    } else {
        cy(2 + 3);                                             // BEQ, LDA $E1
        if (sh.e1 == 0) cy(2 + 5);                             // BNE, DEC $E2
        else cy(3);
        if (sh.e1 == 0) sh.e2 = static_cast<std::uint8_t>(sh.e2 - 1);
        sh.e1 = static_cast<std::uint8_t>(sh.e1 - 1);
        cy(5 + 2 + 3 + 3);                                     // DEC $E1, CLC, LDA $E1, ADC $E2
        const unsigned sum = static_cast<unsigned>(sh.e1) + sh.e2;
        if ((sum & 255) != 0) {
            cy(3);                                             // BNE taken
        } else {
            cy(2);
            if (sum > 255) {
                cy(3);                                         // BCS taken
            } else {
                cy(2 + 6 + 40);                                // BCS, JSR $6D49 and the end of the frightened period
            }
        }
        if (sum == 0) {                                        // BNE / BCS skip the call unless the sum is exactly zero
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
    cy(6 + (s.level() < 15 ? 14 : 15) + 2 + 3);                  // JSR $6D1E and its body, TAX, LDA $78
    if (g.b[8] != 0) cy(3 + 4 + 3 + 3);                        // BNE taken; LDA $8743,X, STA $80, BNE taken
    else if (g.b[9] != 0) cy(2 + 3 + 3 + 4 + 3);               // BNE, LDA $79, BNE taken; LDA $8753,X, STA $80
    else cy(2 + 3 + 2 + 4 + 3 + 3);                            // BNE, LDA $79, BNE, LDA $8723,X, STA $80, BNE taken
    unsigned reload = g.b[8] != 0 ? tables.frightened[x] : g.b[9] != 0 ? tables.eyes[x] : tables.normal[x];
    // $879F-$87B8: a ghost-number term, added when bit 1 of the looked-at coordinate is clear. CLC, ROR $E0 leaves bit 0 of $E0 in the carry, which ADC adds.
    const unsigned e0 = static_cast<unsigned>(ghostNumber);
    const std::uint8_t term = static_cast<std::uint8_t>((e0 >> 1) - 1);
    const unsigned carry = e0 & 1u;
    cy(2 + 3 + 2 + 2 + 2 + 3 + 2);                             // CLC, LDA $E0, ROR, TAX, DEX, LDA $72, AND #2
    if ((g.b[2] & 2) == 0) {
        cy(3 + 3);                                             // BEQ taken, LDA $70
    } else if (g.b[1] != 0) {
        cy(2 + 3 + 3);                                         // BEQ, LDA $71, BNE taken
    } else {
        cy(2 + 3 + 2 + 3);                                     // BEQ, LDA $71, BNE, LDA $70
    }
    const std::uint8_t coordinate = (g.b[2] & 2) != 0 ? (g.b[1] != 0 ? g.b[1] : g.b[0]) : g.b[0];   // direction left/right: Y (X if Y is 0), else X
    cy(2);                                                     // AND #2
    if ((coordinate & 2) == 0) {
        cy(2 + 2 + 3 + 3);                                     // BNE, TXA, ADC $80, STA $80
        reload = static_cast<std::uint8_t>(reload + term + carry);
    } else {
        cy(3);                                                 // BNE taken
    }
    cy(3);                                                     // LDA $7A
    if (g.b[10] != 0) {
        cy(2 + 5);                                             // BEQ, ASL $80
        reload = static_cast<std::uint8_t>(reload << 1);   // $87BA: ASL $80 when the slow flag $7A is set
    } else {
        cy(3);                                                 // BEQ taken
    }
    r.reload = static_cast<std::uint8_t>(reload);
    g.b[16] = r.reload;
    return r;
}

}  // namespace pacman::core
