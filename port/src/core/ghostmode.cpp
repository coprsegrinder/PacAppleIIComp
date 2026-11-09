#include "ghostmode.h"

#include "cycles.h"

#include <algorithm>

namespace pacman::core {

bool GhostModeTables::load(const datapack::Pack& pack) {
    const auto a = pack.find(0x6DEF, 44), b = pack.find(0x6E19, 32);
    if (a.empty() || b.empty()) return false;
    for (std::size_t i = 0; i < 22; ++i) frightenedTime[i] = static_cast<std::uint16_t>(a[2 * i] | (a[2 * i + 1] << 8));
    for (std::size_t i = 0; i < 16; ++i) eatenBonus[i] = static_cast<std::uint16_t>(b[2 * i] | (b[2 * i + 1] << 8));
    return true;
}

void reverseGhosts(GameState& s, GhostQueues& queues) {
    for (int n = 4; n >= 1; --n) {
        GhostRecord& g = s.ghosts[static_cast<std::size_t>(n - 1)];
        if (g.b[17] != 0) continue;                                   // $80C8: ghosts in the house do not reverse
        g.b[2] ^= 1;
        auto& queue = queues[static_cast<std::size_t>(n - 1)];
        queue[g.b[7]] = g.b[2];
        queue[static_cast<std::uint8_t>(g.b[7] + 1)] = 0xFF;
    }
}

namespace {

// $80C1: the ghosts turn round: for each of the four, JSR $86FC (copy the record to the working area), the test of the house flag, the reversal into the queue, JSR $8712 (copy back)
void reverseCost(const GameState& s) {
    cy(6 + 2 + 3);                                                     // (the JSR $80C1), LDA #4, STA $E0
    for (int n = 4; n >= 1; --n) {
        cy(6 + 334 + 3);                                               // JSR $86FC (+ body), LDA $81
        if (s.ghosts[static_cast<std::size_t>(n - 1)].b[17] != 0) cy(3);   // BNE taken
        else cy(2 + 27);                                               // BNE, LDA $72, EOR #1, STA $72, LDY $77, STA ($75),Y, LDA #$FF, INY, STA ($75),Y
        cy(6 + 314 + 5 + (n == 1 ? 2 : 3));                            // JSR $8712 (+ body), DEC $E0, BNE
    }
    cy(6);                                                             // RTS
}

}  // namespace

void modeTimerStep(GameState& s, GhostShared& sh, GhostQueues& queues) {
    cy(3);                                                             // $808C: LDA $EA
    if (sh.ea == 0) {
        cy(2 + 5);                                                     // BNE, DEC $EB
        sh.eb = static_cast<std::uint8_t>(sh.eb - 1);
    } else {
        cy(3);                                                         // BNE taken
    }
    sh.ea = static_cast<std::uint8_t>(sh.ea - 1);
    cy(5 + 2 + 3 + 3);                                                 // DEC $EA, CLC, LDA $EA, ADC $EB
    const unsigned sum = static_cast<unsigned>(sh.ea) + sh.eb;
    if (sum > 255) {
        cy(3 + 6);                                                     // BCS taken, RTS
        return;
    }
    if ((sum & 255) != 0) {
        cy(2 + 3 + 6);                                                 // BCS, BNE taken, RTS
        return;
    }
    cy(2 + 2 + 3);                                                     // BCS, BNE, LDA $E9
    if (s.cornerPhase == 0) {                                          // chase -> corner
        cy(2);                                                         // BNE not taken
        reverseCost(s);
        cy(2 + 3 + 2 + 3 + 2 + 3 + 6);                                 // LDA #1, STA $E9, LDA #$8C, STA $EA, LDA #0, STA $EB, RTS
        reverseGhosts(s, queues);
        s.cornerPhase = 1;
        sh.ea = 0x8C;
        sh.eb = 0;
    } else {                                                           // corner -> chase
        cy(3);                                                         // BNE taken
        reverseCost(s);
        cy(2 + 3 + 2 + 3 + 2 + 3 + 6);                                 // LDA #0, STA $E9, LDA #2, STA $EB, LDA #$58, STA $EA, RTS
        reverseGhosts(s, queues);
        s.cornerPhase = 0;
        sh.eb = 0x02;
        sh.ea = 0x58;
    }
}

void energizerGhostEffects(GameState& s, GhostShared& sh, GhostQueues& queues, const GhostModeTables& tables) {
    cy(3 + 2);                                                         // $6D60: LDX $55, CPX #$15
    if (s.level() >= 0x15) {
        cy(3 + 6);                                                     // BCS taken, RTS
        return;
    }
    cy(2 + 2 + 3);                                                     // BCS, LDA #4, STA $E0
    for (int n = 4; n >= 1; --n) {
        cy(6 + 334 + 3);                                               // JSR $86FC (+ body), LDA $79
        cy(s.ghosts[static_cast<std::size_t>(n - 1)].b[9] != 0 ? 3 : 2 + 2 + 3);   // BNE taken / BNE, LDA #1, STA $78
        cy(6 + 314 + 5 + (n == 1 ? 2 : 3));                            // JSR $8712 (+ body), DEC $E0, BNE
    }
    cy(2 + 3 + 2 + 3 + 6 + (s.level() < 0x15 ? 14 : 15) + 2 + 2);      // LDA #2, STA $68, LDA #1, STA $E4, JSR $6D27 (+ body), ASL, TAX
    cy(4 + cyCross(0x6DEF, 2 * static_cast<int>(std::min<std::size_t>(s.level(), 21))) + 3 + 4 + cyCross(0x6DF0, 2 * static_cast<int>(std::min<std::size_t>(s.level(), 21))) + 3);   // LDA $6DEF,X, STA $E1, LDA $6DF0,X, STA $E2
    cy(2 + 3 + 3 + 2 + 3);                                             // LDA #4, STA $EA, STA $EB, LDA #1, STA $E9
    reverseCost(s);
    cy(6);                                                             // RTS
    for (auto& g : s.ghosts)
        if (g.b[9] == 0) g.b[8] = 1;
    s.mode78 = s.ghosts[0].b[8];                                       // the working copies after the last pass of the loop (ghost 1)
    s.mode79 = s.ghosts[0].b[9];
    s.zp(0x68) = 2;
    sh.e4 = 1;
    const std::uint16_t t = tables.frightenedTime[std::min<std::size_t>(s.level(), 21)];
    sh.e1 = static_cast<std::uint8_t>(t & 0xFF);
    sh.e2 = static_cast<std::uint8_t>(t >> 8);
    sh.ea = 4;
    sh.eb = 4;
    s.cornerPhase = 1;
    reverseGhosts(s, queues);
}

void ghostEatenTimer(GameState& s, GhostShared& sh, const GhostModeTables& tables) {
    const std::uint16_t add = tables.eatenBonus[std::min<std::size_t>(s.level(), 15)];
    const unsigned sum = static_cast<unsigned>(sh.e1) + (add & 0xFF);                          // ASL A cleared the carry before the ADC
    sh.e1 = static_cast<std::uint8_t>(sum);
    sh.e2 = static_cast<std::uint8_t>(sh.e2 + (add >> 8) + (sum >> 8));
}

void initGhostQueues(GhostQueues& queues, const GhostDecisionData& data) {
    for (std::size_t i = 0; i < 15; ++i) queues[0][i] = data.houseScripts[1][i];   // $8A44.. the first 15 bytes
    for (std::size_t i = 0; i < 256; ++i) {
        queues[2][i] = (i & 1) != 0 ? 1 : 0;                           // ghost 3: 0, 1, 0, 1 ...
        queues[1][i] = queues[3][i] = (i & 1) != 0 ? 0 : 1;            // ghosts 2 and 4: 1, 0, 1, 0 ... (index 0 is written last by the wrap-around)
    }
}

}  // namespace pacman::core
