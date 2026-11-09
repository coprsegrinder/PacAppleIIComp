#include "ghostjunction.h"

#include "cycles.h"

#include <algorithm>

namespace pacman::core {

bool GhostDecisionData::load(const datapack::Pack& pack) {
    const auto idx = pack.find(0x8CBE, 256), addr = pack.find(0x8DBE, 96), cor = pack.find(0x8CB4, 10), route = pack.find(0xA360, 64), start = pack.find(0x6D3F, 10);
    if (idx.empty() || addr.empty() || cor.empty() || route.empty() || start.empty()) return false;
    std::copy(start.begin(), start.end(), startPlaces.begin());
    const auto ptrs = pack.find(0x8A3C, 8);
    if (ptrs.empty()) return false;
    for (std::size_t n = 1; n <= 4; ++n) {
        const std::uint16_t p = static_cast<std::uint16_t>(ptrs[2 * (n - 1)] | (ptrs[2 * (n - 1) + 1] << 8));
        // the original copies 16 bytes from the pointer; the script ends with $FF, anything after it is never read (the last script's tail lies on code that the pack does not hold: 0)
        for (std::size_t j = 0; j < 16; ++j) {
            const auto one = pack.find(static_cast<std::uint16_t>(p + j), 1);
            houseScripts[n][j] = one.empty() ? std::uint8_t{0} : one[0];
        }
    }
    std::copy(idx.begin(), idx.end(), routineIndex.begin());
    for (std::size_t i = 0; i < 48; ++i) routineAddress[i] = static_cast<std::uint16_t>(addr[2 * i] | (addr[2 * i + 1] << 8));
    std::copy(cor.begin(), cor.end(), corners.begin());
    std::copy(route.begin(), route.end(), returnRoute.begin());
    return true;
}

namespace {

// $75F1: the body (not the JSR) of the routine that makes the address of the record of junction `j` ($7600 + 8 j + 10): 17 + the INC that a carry out of the third ASL asks for + 7 + the INC of the
// carry out of the ADC + RTS
int cost75F1(unsigned j) noexcept {
    const unsigned a = (j << 3) & 255;
    const bool c1 = (j & 0x20) != 0, c2 = a + 10 > 255;
    return 17 + (c1 ? 7 : 4) + 7 + (c2 ? 7 : 3) + 6;
}

}  // namespace

JunctionStepResult ghostJunction(GameState& s, int ghostNumber, GhostQueues& queues, const JunctionGraph& graph, const GhostDecisionData& data, const GhostTables& tables) {
    JunctionStepResult r;
    GhostRecord& g = s.ghosts[static_cast<std::size_t>(ghostNumber - 1) & 3];
    auto& queue = queues[static_cast<std::size_t>(ghostNumber - 1) & 3];
    const std::uint8_t x = g.b[0], y = g.b[1];
    // $87C0-$87EA: the position equals the current junction's, or else the one it is heading to
    std::uint8_t found;
    cy(3 + 2 + 6 + cost75F1(g.b[3]) + 2 + 5 + 3);               // LDA $73, TAX, JSR $75F1, LDY #0, LDA ($16),Y, CMP $70
    const bool xHere = graph.at(g.b[3]).x == x;
    if (!xHere) {
        cy(3);                                                  // BNE taken
    } else {
        cy(2 + 2 + 5 + 3);                                      // BNE, INY, LDA ($16),Y, CMP $71
        cy(graph.at(g.b[3]).y == y ? 3 : 2);                    // BEQ
    }
    if (xHere && graph.at(g.b[3]).y == y) {
        found = g.b[3];
    } else {
        cy(3 + 2 + 6 + cost75F1(g.b[4]) + 2 + 5 + 3);           // LDA $74, TAX, JSR $75F1, LDY #0, LDA ($16),Y, CMP $70
        const bool xNext = graph.at(g.b[4]).x == x;
        if (!xNext) {
            cy(3 + 3);                                          // BNE taken, JMP $8866
            return r;
        }
        cy(2 + 2 + 5 + 3);                                      // BNE, INY, LDA ($16),Y, CMP $71
        if (graph.at(g.b[4]).y != y) {
            cy(2 + 3);                                          // BEQ, JMP $8866
            return r;
        }
        cy(3);                                                  // BEQ taken
        found = g.b[4];
    }
    r.onJunction = true;
    r.junction = found;
    g.b[3] = found;                                           // $87ED: STX $73
    cy(3 + 6);                                                // STX $73, JSR $8C50
    // $8C50: the decision, unless the queue still holds a move
    std::uint8_t qi = g.b[7];                                 // $77
    cy(3);                                                    // LDA $79
    if (g.b[9] != 0) {                                        // eyes: the fixed route home
        cy(2 + 34);                                           // BEQ, $8C54-$8C63
        queue[static_cast<std::uint8_t>(qi + 1)] = data.returnRoute[found & 63];
        queue[static_cast<std::uint8_t>(qi + 2)] = 0xFF;
        r.decided = true;
    } else if ((queue[static_cast<std::uint8_t>(qi + 1)] & 0x80) == 0) {
        cy(3 + 3 + 2 + 5 + 2 + 6);                            // BEQ taken, LDY $77, INY, LDA ($75),Y, BMI not taken, RTS
    } else {
        cy(3 + 3 + 2 + 5 + 3);                                // BEQ taken, LDY $77, INY, LDA ($75),Y, BMI taken
        const std::uint8_t index = static_cast<std::uint8_t>(found * 4 + ghostNumber - 1);
        const std::uint8_t routine = data.routineIndex[index];
        const std::uint16_t entry = data.routineAddress[routine % 48];
        // $8C6C-$8C89: 43 cycles and the carries of the three table reads
        cy(43 + cyCross(0x8CBE, index) + cyCross(0x8DBE, 2 * routine) + cyCross(0x8DBF, 2 * routine));
        const bool corner = g.b[8] != 0 || s.cornerPhase != 0;
        if (g.b[8] != 0) cy(3 + 25);                          // BNE taken, $8C8F-$8C9D
        else if (s.cornerPhase != 0) cy(2 + 3 + 2 + 25);      // BNE, LDA $E9, BEQ not taken, $8C8F-$8C9D
        else cy(2 + 3 + 3 + 9);                               // BNE, LDA $E9, BEQ taken, $8CA0-$8CA4
        std::uint8_t tx, ty;
        if (corner) {
            ty = data.corners[static_cast<std::size_t>(2 * ghostNumber)];
            tx = data.corners[static_cast<std::size_t>(2 * ghostNumber + 1)];
        } else {
            tx = s.pacX();
            ty = s.pacY();
        }
        DecisionEnv env;
        env.ghostX = x;
        env.ghostY = y;
        env.ghostDir = g.b[2];
        env.pacDir = s.pacDirection();
        env.pacTunnel = s.zp(0x69);
        const std::uint8_t answer = ghostDecisionRun(entry, g.b[2], tx, ty, env);
        cy(6 + env.cycles + 27);                              // JSR to the routine, the routine, $8CA9-$8CB3
        g.b[2] = env.ghostDir;                                // some routines write $72
        queue[static_cast<std::uint8_t>(qi + 1)] = answer;
        queue[static_cast<std::uint8_t>(qi + 2)] = 0xFF;
        r.decided = true;
    }
    // $87F2-$8811: take the next queued move
    qi = static_cast<std::uint8_t>(qi + 1);
    g.b[7] = qi;
    const std::uint8_t next = queue[qi];
    const bool same = next == g.b[2];
    g.b[2] = next;
    cy(5 + 3 + 5 + 3 + 3);                                    // INC $77, LDY $77, LDA ($75),Y, CMP $72, STA $72
    if (same) {
        cy(4);                                                // BEQ taken across the page
    } else {
        cy(2 + 3);                                            // BEQ, LDA $79
        if (g.b[9] != 0) {
            cy(3);                                            // BNE taken
        } else {
            cy(2 + 6 + (s.level() < 15 ? 14 : 15) + 2 + 4 + 3);   // BNE, JSR $6D1E, TAX, LDA $8733,X, STA $80
            g.b[16] = tables.turn[std::min<std::size_t>(s.level(), 15)];   // $8802-$8809
        }
    }
    g.b[4] = graph.at(found).next[g.b[2] & 3];                                               // $880B-$8811
    cy(3 + 2 + 2 + 5 + 3 + 3 + 2);                            // LDY $72, INY, INY, LDA ($16),Y, STA $74, LDA $73, CMP #$17
    // $8813-$8864: the special junctions
    if (found == 0x17 || found == 0x18) {                                                    // the house door: only an eaten ghost does anything here
        cy(found == 0x17 ? 3 : 2 + 2 + 3);                    // BEQ (and CMP #$18, BEQ)
        cy(3);                                                // LDA $79
        if (g.b[9] != 0) {
            cy(2 + 10 + 6 + 23 + 6 + 27 + 6 + (ghostNumber == 1 ? 94 : ghostNumber == 3 ? 98 : ghostNumber == 2 ? 102 : 104) + 314 + 3);   // BEQ, the flags, JSR $7D8D (+ body), JSR $6D30 (+ body), JSR $6DA1 (+ body, with the JSR $8712), JMP $8866 (the JSR $699E is a drawing call)
            g.b[9] = 0;                                                                      // $79: the eyes are gone
            g.b[17] = 1;                                                                     // $81: in the house
            g.b[0] = data.startPlaces[static_cast<std::size_t>(2 * ghostNumber)];            // $6D30: back to the start place
            g.b[1] = data.startPlaces[static_cast<std::size_t>(2 * ghostNumber + 1)];
            // $6DA1: the exit script, written behind the queue index
            std::uint8_t y2 = static_cast<std::uint8_t>(g.b[7] + 1);
            auto put = [&](std::uint8_t v) { queue[y2++] = v; };
            if (ghostNumber == 1 || ghostNumber == 3) {
                put(0);
                put(1);
                put(0);
            } else {
                put(1);
                put(0);
                put(ghostNumber == 2 ? 2 : 3);
            }
            put(1);
            put(1);
            put(3);
            put(0xFF);
            r.arrivedAtHouse = true;
        } else {
            cy(3);                                            // BEQ taken
        }
    } else if (found == 0x1A) {                                                              // the left end of the tunnel row
        cy(2 + 2 + 2 + 2 + 3);                                // BEQ, CMP #$18, BEQ, CMP #$1A, BEQ taken
        cy(2 + 3 + 2 + 3 + 3 + 2);                            // LDA #6, STA $7B, LDA #0, STA $7D, LDA $72, CMP #3
        g.b[11] = 6;                                                                         // $7B
        g.b[13] = 0;                                                                         // $7D
        g.b[10] = g.b[2] == 3 ? 1 : 0;                                                       // $7A: slow while heading into the tunnel
        cy(g.b[2] == 3 ? 3 + 2 + 3 + 3 : 2 + 2 + 3);          // BEQ taken, LDA #1, STA $7A, BNE taken / BEQ, LDA #0, STA $7A
    } else if (found == 0x1D) {                                                              // the right end
        cy(2 + 2 + 2 + 2 + 2 + 2 + 2);                        // BEQ, CMP #$18, BEQ, CMP #$1A, BEQ, CMP #$1D, BNE not taken
        cy(2 + 3 + 2 + 3 + 3 + 2);                            // LDA #$15, STA $7B, LDA #0, STA $7D, LDA $72, CMP #2
        g.b[11] = 0x15;
        g.b[13] = 0;
        g.b[10] = g.b[2] == 2 ? 1 : 0;
        cy(g.b[2] == 2 ? 2 + 2 + 3 + 3 : 3 + 2 + 3);          // BNE, LDA #1, STA $7A, BNE taken / BNE taken, LDA #0, STA $7A
    } else {
        cy(2 + 2 + 2 + 2 + 2 + 2 + 3);                        // the four tests fail; BNE taken to $8866
    }
    return r;
}

namespace {

// $895F: the dot restore after a ghost left a cell: JSR $8B13 (the address of the cell in the maze map), the test of the dot bit, and for a dot the pixel position and the call of $6172
// (that drawing call is counted with the drawing). `cellX`, `cellY` = $7B / $7C as the routine sees them.
void dotRestoreCost(const GameState& s, unsigned cellX, unsigned cellY) {
    cy(6 + 6);                                                 // JSR $895F, JSR $8B13
    unsigned address;
    cy(costCellAddress(cellX, cellY, address));
    const unsigned off = address - 0x623A;
    const bool dot = address >= 0x623A && off < s.map.size() && (s.map[off] & 1) != 0;
    cy(2 + 5 + 2);                                             // LDY #0, LDA ($16),Y, ROR
    if (!dot) {
        cy(3 + 6);                                             // BCC taken, RTS
    } else {
        cy(2 + 3 + 6 + 16 + 2 + 2 + 3 + 3 + 6 + 16 + 2 + 2 + 3 + 6);   // BCC, LDA $7B, JSR $8981, CLC, ADC, STA $0C, LDA $7C, JSR $8981, CLC, ADC, STA $10, (JSR $6172 is a drawing call), RTS
    }
}

// $88ED-$8957: the cell counters (3 sub-steps of 2 units per cell); the cycles of the way through them, with the call of $895F when a cell has been left (the RTS is counted by ghostMove)
void counters(const GameState& s, GhostRecord& g) {
    cy(2 + 3 + 2);                                             // CLC, LDA $72, ROR
    switch (g.b[2] & 3) {
        case 2:   // right: $7D counts 0, 2, 4
            cyBranch(false, 0x88F3, 0x8929);                   // BEQ
            cyBranch(false, 0x88F5, 0x890F);                   // BCS
            cy(2 + 3 + 2);                                     // LDA #2, ADC $7D, CMP #6
            g.b[13] = static_cast<std::uint8_t>(g.b[13] + 2);
            if (g.b[13] == 6) {
                cy(2 + 2 + 5);                                 // BNE, LDA #0, INC $7B
                g.b[13] = 0;
                ++g.b[11];
            } else {
                cyBranch(true, 0x88FD, 0x8901);
            }
            cy(3 + 2);                                         // STA $7D, CMP #0
            if (g.b[13] != 0) {
                cyBranch(true, 0x8907, 0x890E);
            } else {
                cy(2 + 5);                                     // BNE, DEC $7B
                dotRestoreCost(s, static_cast<unsigned>(g.b[11] - 1) & 255, g.b[12]);
                cy(5);                                         // INC $7B
            }
            break;
        case 3:   // left
            cyBranch(false, 0x88F3, 0x8929);
            cyBranch(true, 0x88F5, 0x890F);                    // BCS taken
            cy(3 + 2 + 2);                                     // LDA $7D, SBC #2, CMP #$FE
            g.b[13] = static_cast<std::uint8_t>(g.b[13] - 2);
            if (g.b[13] == 0xFE) {
                cy(2 + 2 + 5);                                 // BNE, LDA #4, DEC $7B
                g.b[13] = 4;
                --g.b[11];
            } else {
                cyBranch(true, 0x8917, 0x891B);
            }
            cy(3 + 2);                                         // STA $7D, CMP #0
            if (g.b[13] != 0) {
                cyBranch(true, 0x8921, 0x890E);
            } else {
                cy(2 + 5);                                     // BNE, INC $7B
                dotRestoreCost(s, static_cast<unsigned>(g.b[11] + 1) & 255, g.b[12]);
                cy(5);                                         // DEC $7B
            }
            break;
        case 0:   // down: $7E / $7C
            cyBranch(true, 0x88F3, 0x8929);                    // BEQ taken
            cyBranch(false, 0x892B, 0x8945);                   // BCS
            cy(3 + 2 + 2);                                     // LDA $7E, ADC #2, CMP #6
            g.b[14] = static_cast<std::uint8_t>(g.b[14] + 2);
            if (g.b[14] == 6) {
                cy(2 + 2 + 5);
                g.b[14] = 0;
                ++g.b[12];
            } else {
                cyBranch(true, 0x8933, 0x8937);
            }
            cy(3 + 2);
            if (g.b[14] != 0) {
                cyBranch(true, 0x893D, 0x890E);
            } else {
                cy(2 + 5);
                dotRestoreCost(s, g.b[11], static_cast<unsigned>(g.b[12] - 1) & 255);
                cy(5);
            }
            break;
        default:  // up
            cyBranch(true, 0x88F3, 0x8929);
            cyBranch(true, 0x892B, 0x8945);                    // BCS taken
            cy(3 + 2 + 2);
            g.b[14] = static_cast<std::uint8_t>(g.b[14] - 2);
            if (g.b[14] == 0xFE) {
                cy(2 + 2 + 5);
                g.b[14] = 4;
                --g.b[12];
            } else {
                cyBranch(true, 0x894D, 0x8951);
            }
            cy(3 + 2);
            if (g.b[14] != 0) {
                cyBranch(true, 0x8957, 0x890E);
            } else {
                cy(2 + 5);
                dotRestoreCost(s, g.b[11], static_cast<unsigned>(g.b[12] + 1) & 255);
                cy(5);
            }
            break;
    }
}

// $88A2-$88DE (the end of every move): the choice of the colour for the ghost image (blue and white while frightened), the loads of the image address and the test of the house flag; the
// drawing call itself (JSR $6A20) is counted with the drawing. $1D is 1 in a game.
void drawSetupCost(const GhostRecord& g, const GhostShared* sh) {
    const unsigned e1 = sh ? sh->e1 : 0, e2 = sh ? sh->e2 : 0;
    cy(3 + 2 + 2 + 3);                                         // LDA $E0, ASL, TAX, LDA $78
    if (g.b[8] != 0) {
        cy(2 + 3);                                             // BEQ, LDA $E2
        bool blue = true;
        if (e2 != 0) {
            cy(3);                                             // BNE taken
        } else {
            cy(2 + 3 + 2);                                     // BNE, LDA $E1, CMP #$80
            if (e1 >= 0x80) {
                cy(3);                                         // BCS taken
            } else {
                cy(2 + 2 + 2);                                 // BCS, LDX #6, AND #$10
                if ((e1 & 0x10) == 0) {
                    cy(3);                                     // BEQ taken
                    blue = false;
                } else {
                    cy(2);
                }
            }
        }
        if (blue) {
            cy(2);                                             // LDX #0
            cy(3 + (g.b[9] == 0 ? 3 : 2 + 2));                 // LDA $79, BEQ (taken) / BEQ, LDX #$0A
        }
    } else {
        cy(3 + 3 + 3);                                         // BEQ taken, LDA $79 ($88BC), BEQ taken or not
        if (g.b[9] != 0) cy(-1 + 2);                           // (not taken: 2, then LDX #$0A)
    }
    cy(3 + 2 + 21);                                            // LDA $1D, BEQ not taken (it is 1), the palette lines
    cy(4 + 4);                                                 // LDA $9E5C,X, LDY $9E5D,X
    cy(3);                                                     // LDA $81
}

// $887A-$889A: two units along the direction of travel (with $8874-$889A: JSR $7D8D, the direction test and the stores)
void plainMove(GhostRecord& g) {
    switch (g.b[2] & 3) {
        case 0: g.b[1] = static_cast<std::uint8_t>(g.b[1] + 2); cy(48); break;   // down
        case 1: g.b[1] = static_cast<std::uint8_t>(g.b[1] - 2); cy(49); break;   // up
        case 2: g.b[0] = static_cast<std::uint8_t>(g.b[0] + 2); cy(55); break;   // right
        default: g.b[0] = static_cast<std::uint8_t>(g.b[0] - 2); cy(53); break;  // left
    }
    cy(12);                                                    // $889A-$88A0
}

// $8A9C-$8B12: the tunnel move. Right half (x >= $60) or left half; the wrap exchanges the junction numbers and moves the ghost to the other end.
void tunnelMove(GhostRecord& g) {
    const bool oddDir = (g.b[2] & 1) != 0;
    cy(6 + 23 + 3 + 2);                                        // JSR $7D8D, LDA $70, CMP #$60 (the JSR $699E is a drawing call)
    if (g.b[0] >= 0x60) {
        cy(2 + 3 + 2);                                         // BCC, LDA $72, ROR
        if (!oddDir) {
            cy(3 + 3 + 2 + 3 + 3 + 2);                         // BCC taken, LDA $70, ADC #2, STA $70, STA $0C, CMP #$DD
            g.b[0] = static_cast<std::uint8_t>(g.b[0] + 2);
            if (g.b[0] == 0xDD) {
                cy(2 + 29 + 3);                                // BNE, $8AC8-$8ADC, BEQ taken
                g.b[3] = g.b[4];
                g.b[4] = 0x1D;
                g.b[0] = 0x3D;
                g.b[11] = 0;
                g.b[13] = 0;
            } else {
                cy(3);                                         // BNE taken
            }
        } else {
            cy(2 + 3 + 2 + 3 + 3);                             // BCC not taken, LDA $70, SBC #2, STA $70, STA $0C
            g.b[0] = static_cast<std::uint8_t>(g.b[0] - 2);
        }
    } else {
        cy(3 + 3 + 2);                                         // BCC taken, LDA $72, ROR
        if (!oddDir) {
            cy(2 + 3 + 2 + 3 + 3 + 3);                         // BCS not taken, LDA $70, ADC #2, STA, STA, BNE taken
            g.b[0] = static_cast<std::uint8_t>(g.b[0] + 2);
        } else {
            cy(3 + 3 + 2 + 3 + 3 + 2);                         // BCS taken, LDA $70, SBC #2, STA, STA, CMP #$3B
            g.b[0] = static_cast<std::uint8_t>(g.b[0] - 2);
            if (g.b[0] == 0x3B) {
                cy(2 + 29 + 4);                                // BNE, $8AFB-$8B0F, BEQ taken across the page
                g.b[3] = g.b[4];
                g.b[4] = 0x1A;
                g.b[0] = 0xDB;
                g.b[11] = 0x1C;
                g.b[13] = 0;
            } else {
                cy(3);                                         // BNE taken
            }
        }
    }
    cy(3 + 3 + 3);                                             // $8AB5: LDA $71, STA $10, JMP $88A2
}

// $8988-$8A38: the house. The release timer $82 counts down; when it reaches zero the exit moves are queued (and, in the corner phase, the script of the ghost).
// At the ends of its way inside the box (and at the door row) the ghost takes the next queued move.
void houseStep(GameState& s, GhostRecord& g, int ghostNumber, GhostQueues& queues, const GhostDecisionData& data) {
    auto& queue = queues[static_cast<std::size_t>(ghostNumber - 1) & 3];
    g.b[18] = static_cast<std::uint8_t>(g.b[18] - 1);
    cy(5);                                                     // DEC $82
    if (g.b[18] == 0) {
        cy(2);                                                 // BNE not taken
        cy(3 + 3 + 2);                                         // LDY $77, LDA $E0, ROR
        std::uint8_t y = g.b[7];
        auto push = [&](std::uint8_t v) { queue[++y] = v; };
        if ((ghostNumber & 1) != 0) {                            // ghosts 1 and 3
            cy(2 + 3 + 2);                                     // BCC not taken, LDA $72, CMP #0
            if (g.b[2] == 0) {
                cy(2 + 2 + 2 + 6);                             // BNE, LDA #1, INY, STA
                push(1);
            } else {
                cy(3);                                         // BNE taken
            }
            push(1);
            cy(2 + 2 + 6 + 3);                                 // $899E: LDA #1, INY, STA, JMP $89C7
        } else {                                                 // ghosts 2 and 4
            cy(3 + 3 + 2);                                     // BCC taken, LDA $72, CMP #1
            if (g.b[2] == 1) {
                cy(2 + 2 + 2 + 6);                             // BNE, INY, LDA #0, STA
                push(0);
            } else {
                cy(3);
            }
            cy(3 + 2);                                         // LDA $E0, CMP #2
            if (ghostNumber == 2) {
                cy(2 + 2 + 2 + 6 + 3);                         // BNE, LDA #2, INY, STA, JMP $8999
            } else {
                cy(3 + 2 + 2 + 6 + 3);                         // BNE taken, LDA #3, INY, STA, JMP $8999
            }
            push(ghostNumber == 2 ? 2 : 3);
            push(1);
            push(1);
            cy(2 + 2 + 6 + 2 + 2 + 6 + 3);                     // $8999: LDA #1, INY, STA; $899E: LDA #1, INY, STA, JMP $89C7
        }
        ++y;                                                     // $89C7
        cy(2 + 3);                                             // INY, LDA $E9
        if (s.cornerPhase == 0) {
            cy(2 + 2 + 6 + 2 + 2 + 6 + 3);                     // BNE, LDA #3, STA, INY, LDA #$FF, STA, BNE taken
            queue[y] = 3;
            queue[static_cast<std::uint8_t>(y + 1)] = 0xFF;
        } else {                                                 // $6EA5: the 16-byte script of this ghost
            cy(3 + 6 + 334);                                   // BNE taken, JSR $6EA5 and its body
            for (std::size_t j = 0; j < 16; ++j) queue[static_cast<std::uint8_t>(y + j)] = data.houseScripts[static_cast<std::size_t>(ghostNumber)][j];
        }
    } else {
        cy(3);                                                 // BNE taken
    }
    cy(3 + 2);                                                 // $89DA: LDA $E0, ROR
    bool advance = false;
    bool centre = true;                                          // look at the middle column ($8A05)
    if ((ghostNumber & 1) == 0) {                                // ghosts 2 and 4 have their own column inside the box
        cy(3);                                                 // BCC taken
        cy(2);                                                 // $89E1: ROR
        if (ghostNumber == 2) cy(2 + 2 + 3);                   // BCC, LDA #$7F, BNE taken
        else cy(3 + 2);                                        // BCC taken, LDA #$99
        const std::uint8_t column = ghostNumber == 2 ? 0x7F : 0x99;
        cy(3);                                                 // CMP $70
        if (g.b[0] == column) {
            cy(2 + 3 + 2);                                     // BNE not taken, LDA $71, CMP #$55
            if (g.b[1] == 0x55) {
                cy(2);                                         // BNE not taken
                advance = true;
            } else {
                cy(3 + 2);                                     // BNE taken, CMP #$5D
                if (g.b[1] == 0x5D) {
                    cy(4);                                     // BEQ taken
                    advance = true;
                } else {
                    cy(2 + 4);                                 // BEQ, BNE taken to $89FC
                }
            }
            centre = false;
        } else {
            cy(4);                                             // BNE taken to $8A05
        }
    } else {
        cy(2 + 4);                                             // BCC not taken, BCS taken to $8A05
    }
    if (centre) {
        cy(3 + 2);                                             // $8A05: LDA $70, CMP #$8B
        if (g.b[0] != 0x8B) {
            cy(4);                                             // BNE taken
        } else {
            cy(2 + 3 + 2);                                     // BNE, LDA $71, CMP #$5D
            if (g.b[1] == 0x5D) {
                cy(4);
                advance = true;
            } else {
                cy(2 + 2);                                     // BEQ, CMP #$55
                if (g.b[1] == 0x55) {
                    cy(4);
                    advance = true;
                } else {
                    cy(2 + 2);                                 // BEQ, CMP #$47
                    if (g.b[1] == 0x47) {                        // the door row: out of the house
                        cy(2 + 42 + 35 + 4);                  // BNE, JSR $8A7D without its two drawing calls (JSR $5CF0 is counted with the drawing), the stores, BEQ taken
                        g.b[3] = 0x17;
                        g.b[4] = 0x18;
                        g.b[11] = 0x0D;
                        g.b[12] = 0x0B;
                        g.b[13] = 2;
                        g.b[14] = 0;
                        g.b[17] = 0;
                        advance = true;
                    } else {
                        cy(4);                                 // BNE taken
                    }
                }
            }
        }
    }
    if (advance) {
        cy(5 + 3 + 5 + 3 + 3);                                 // $89F4: INC $77, LDY $77, LDA ($75),Y, STA $72, JMP $8874
        ++g.b[7];
        g.b[2] = queue[g.b[7]];
    } else {
        cy(3);                                                 // $89FC: JMP $8874
    }
}

}  // namespace

void ghostMove(GameState& s, int ghostNumber, GhostQueues& queues, const GhostDecisionData& data, const GhostShared* shared) {
    GhostRecord& g = s.ghosts[static_cast<std::size_t>(ghostNumber - 1) & 3];
    cy(3);                                                     // $8866: LDA $7A
    if (g.b[10] != 0) {
        cy(2 + 3);                                             // BEQ not taken, JMP $8A9C
        tunnelMove(g);                                           // $8A9C (the slow flag is tested before the house flag)
        drawSetupCost(g, shared);
    } else {
        cy(3 + 3);                                             // BEQ taken, $886D: LDA $81
        const bool inHouse = g.b[17] != 0;
        if (inHouse) {
            cy(2 + 3);                                         // BEQ not taken, JMP $8988
            houseStep(s, g, ghostNumber, queues, data);
        } else {
            cy(3);                                             // BEQ taken
        }
        plainMove(g);
        drawSetupCost(g, shared);
        if (g.b[17] != 0) {                                      // $88DC: a ghost that is (still) in the house returns here: no contact test, no cell counters
            cy(2 + 6);                                         // BEQ not taken, RTS
            return;
        }
    }
    cy(3 + 3);                                                 // BEQ taken ($88DE), $88E6: LDA $79
    cy(g.b[9] != 0 ? 3 : 2);                                   // BNE taken (eyes: no contact test) / not taken (the contact test adds its own cycles, JSR $8B34 included)
    counters(s, g);
    cy(6);                                                     // RTS
}

}  // namespace pacman::core
