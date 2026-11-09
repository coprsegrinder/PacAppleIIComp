#include "junction.h"

#include "cycles.h"

namespace pacman::core {

bool JunctionGraph::load(const datapack::Pack& pack) {
    const auto bytes = pack.find(0x760A, kJunctions * 8);
    if (bytes.empty()) return false;
    for (std::size_t i = 0; i < kJunctions; ++i) {
        Junction& j = junctions_[i];
        j.x = bytes[i * 8];
        j.y = bytes[i * 8 + 1];
        for (std::size_t d = 0; d < 4; ++d) j.next[d] = bytes[i * 8 + 2 + d];
    }
    return true;
}

namespace {
bool atJunction(const GameState& s, const Junction& j) { return j.x == s.pacX() && j.y == s.pacY(); }  // $78AD-$78B8
bool isExit(std::uint8_t v) { return (v & 0x80) == 0; }                                            // BPL

// $75F1: the body (not the JSR) of the routine that makes the address of the record of junction `j` ($7600 + 8 j + 10)
int cost75F1(unsigned j) noexcept {
    const unsigned a = (j << 3) & 255;
    const bool c1 = (j & 0x20) != 0, c2 = a + 10 > 255;
    return 17 + (c1 ? 7 : 4) + 7 + (c2 ? 7 : 3) + 6;
}
}  // namespace

JunctionOutcome junctionStep(GameState& s, const JunctionGraph& graph, std::optional<std::uint8_t> demoStreamByte) {
    StreamByteSource source;
    if (demoStreamByte) {
        const std::uint8_t b = *demoStreamByte;
        source = [b] { return b; };
    }
    return junctionStep(s, graph, source);
}

JunctionOutcome junctionStep(GameState& s, const JunctionGraph& graph, const StreamByteSource& stream) {
    std::uint8_t& dir = s.pacDirection();            // $62
    std::uint8_t& wanted = s.requestedDirection();   // $63
    const Junction* here = &graph.at(s.pacJunction());   // the record the original has in $16/$17 after its first JSR $75F1
    bool betweenJunctions = false;

    cy(3 + 2 + 6 + cost75F1(s.pacJunction()) + 2 + 5 + 3);   // $78A5: LDA $64, TAX, JSR $75F1, LDY #0, LDA ($16),Y, CMP $60
    const bool hereX = here->x == s.pacX();
    if (!hereX) {
        cy(3);                                       // BNE taken
    } else {
        cy(2 + 2 + 5 + 3);                           // BNE, INY, LDA ($16),Y, CMP $61
        cy(here->y == s.pacY() ? 3 : 2);             // BEQ
    }
    if (atJunction(s, *here)) {                      // $78A5-$78B8 -> $78D1
        s.pacNextJunction() = 0xFF;
        cy(3 + 2 + 3);                               // STX $64, LDA #$FF, STA $65
    } else {
        cy(3);                                       // $78BA: LDA $65
        if (!isExit(s.pacNextJunction())) {          // $78BA: BMI $78D7, nothing to arrive at
            cy(3);                                   // BMI taken
        } else {
            cy(2 + 2 + 6);                           // BMI, TAX, JSR $75F1
            const std::uint8_t candidate = s.pacNextJunction();
            cy(cost75F1(candidate) + 2 + 5 + 3);     // the routine, LDY #0, LDA ($16),Y, CMP $60
            here = &graph.at(candidate);             // $78BE: JSR $75F1 for the next junction
            if (here->x != s.pacX()) {
                cy(3 + 4);                           // BNE taken, BNE $794B taken across the page
                betweenJunctions = true;             // $78CF: BNE $794B
            } else {
                cy(2 + 2 + 5 + 3);                   // BNE, INY, LDA ($16),Y, CMP $61
                if (here->y != s.pacY()) {
                    cy(4);                           // BNE taken across the page
                    betweenJunctions = true;
                } else {
                    cy(2 + 3 + 2 + 3);               // BNE not taken, STX $64, LDA #$FF, STA $65
                    s.pacJunction() = candidate;     // $78D1: STX $64
                    s.pacNextJunction() = 0xFF;
                }
            }
        }
    }

    if (betweenJunctions) {
        // $794B-$7959: between two junctions Pac-Man may only reverse (directions 0<->1 or 2<->3: the codes differ in bit 0 only)
        cy(3);                                       // LDA $63
        if ((wanted & 0x80) != 0) {
            cy(3);                                   // BMI taken
        } else {
            cy(2 + 3 + 2 + 2);                       // BMI, EOR $62, CLC, ROR
            if (((wanted ^ dir) >> 1) != 0) {
                cy(3);                               // BNE taken
            } else {
                cy(2);                               // BNE
                if (((wanted ^ dir) & 1) == 0) {
                    cy(3);                           // BCC taken
                } else {
                    cy(2 + 3 + 3);                   // BCC, LDA $63, STA $62
                }
            }
        }
        if ((wanted & 0x80) == 0 && (wanted ^ dir) == 1) dir = wanted;
    } else {
        cy(3);                                       // $78D7: LDA $E8
        if (s.demo != 0 && stream) {
            cy(2 + 5 + 3 + 2 + 5 + 3);               // BEQ, INC $1B, BNE taken, LDY #0, LDA ($1B),Y, STA $63
            wanted = stream();   // $78D7-$78E5
        } else {
            cy(3);                                   // BEQ taken
        }
        std::optional<std::uint8_t> neighbour;       // the junction Pac-Man will head for
        cy(3);                                       // $78E7: LDY $63
        if ((wanted & 0x80) == 0) {                  // $78E7: a requested direction exists
            cy(2 + 2 + 2 + 5);                       // BMI not taken, INY, INY, LDA ($16),Y
            const std::uint8_t wantedNext = here->next[wanted & 3];     // INY INY / LDA ($16),Y: byte 2 + direction
            if (isExit(wantedNext)) {
                cy(3 + 3 + 3);                       // BPL taken, $78F9: LDA $63, STA $62
                dir = wanted;                        // $78F9
            } else {
                cy(2 + 3 + 2 + 2 + 5);               // BPL, LDY $62, INY, INY, LDA ($16),Y
                const std::uint8_t currentNext = here->next[dir & 3];   // $78F1
                if (isExit(currentNext)) {
                    cy(4);                           // BPL taken across the page
                    neighbour = currentNext;         // $78F7: BPL $7915, keep the current direction
                } else {
                    cy(2 + 3 + 3);                   // BPL, LDA $63, STA $62
                    dir = wanted;                    // fall into $78F9; $78FD will find the way blocked
                }
            }
        } else {
            cy(3);                                   // BMI taken
        }
        if (!neighbour) {
            cy(3 + 2 + 2 + 5);                       // $78FD: LDY $62, INY, INY, LDA ($16),Y
            const std::uint8_t n = here->next[dir & 3];                 // $78FD
            if (!isExit(n)) {
                cy(2);                               // BPL not taken
                // $7905-$7912: JSR $7D80 (+ body), JSR $699E (drawing: its glue is with the erase), LDA #2, STA $66, JSR $7D80 (+ body), JMP $79A4
                cy(6 + 23 + 2 + 3 + 6 + 23 + 3);
                s.zp(0x66) = 2;                      // $7905-$790D: Pac-Man is redrawn standing still, nothing moves
                cy(cost79A4(2));
                return JunctionOutcome::Blocked;
            }
            cy(3);                                   // BPL taken
            neighbour = n;
        }
        s.pacNextJunction() = *neighbour;            // $7915
        const std::uint8_t at = s.pacJunction();
        cy(3 + 3 + 2);                               // STA $65, LDA $64, CMP #$1A
        if (at == 0x1A || at == 0x1D) {              // the two tunnel ends: 26 (left) and 29 (right)
            const std::uint8_t entering = at == 0x1A ? 3 : 2;   // heading out through the tunnel
            if (at == 0x1A) {
                cy(3 + 2 + 3 + 2 + 3 + 3 + 2);       // BEQ taken, LDA #6, STA $6A, LDA #0, STA $6C, LDA $62, CMP #3
                cy(dir == 3 ? 2 + 3 + 2 + 3 + 3 : 3 + 2 + 3 + 3);   // BNE, BEQ taken, LDA #1, STA $69, BNE taken / BNE taken, LDA #0, STA $69, BEQ taken
            } else {
                cy(2 + 2 + 2 + 2 + 3 + 2 + 3 + 3 + 2);   // BEQ, CMP #$1D, BNE not taken, LDA #$15, STA $6A, LDA #0, STA $6C, LDA $62, CMP #2
                cy(dir == 2 ? 2 + 2 + 3 + 3 : 3 + 2 + 3 + 3);   // BNE, LDA #1, STA $69, BNE taken / BNE taken, LDA #0, STA $69, BEQ taken
            }
            s.zp(0x6A) = at == 0x1A ? 6 : 0x15;
            s.zp(0x6C) = 0;
            s.zp(0x69) = dir == entering ? 1 : 0;
        } else {
            cy(2 + 2 + 3);                           // BEQ not taken, CMP #$1D, BNE taken
        }
    }
    cy(3);                                           // $795B: LDA $69
    if (s.zp(0x69) != 0) {
        cy(2 + 3);                                   // BEQ not taken, JMP $7B2F
        return JunctionOutcome::Tunnel;
    }
    cy(3);                                           // BEQ taken
    return JunctionOutcome::Move;                    // $795B-$795F
}

}  // namespace pacman::core
