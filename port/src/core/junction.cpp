#include "junction.h"

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

    if (atJunction(s, *here)) {                      // $78A5-$78B8 -> $78D1
        s.pacNextJunction() = 0xFF;
    } else if (!isExit(s.pacNextJunction())) {       // $78BA: BMI $78D7, nothing to arrive at
    } else {
        const std::uint8_t candidate = s.pacNextJunction();
        here = &graph.at(candidate);                 // $78BE: JSR $75F1 for the next junction
        if (atJunction(s, *here)) {
            s.pacJunction() = candidate;             // $78D1: STX $64
            s.pacNextJunction() = 0xFF;
        } else {
            betweenJunctions = true;                 // $78CF: BNE $794B
        }
    }

    if (betweenJunctions) {
        // $794B-$7959: between two junctions Pac-Man may only reverse (directions 0<->1 or 2<->3: the codes differ in bit 0 only)
        if ((wanted & 0x80) == 0 && (wanted ^ dir) == 1) dir = wanted;
    } else {
        if (s.demo != 0 && stream) wanted = stream();   // $78D7-$78E5
        std::optional<std::uint8_t> neighbour;       // the junction Pac-Man will head for
        if ((wanted & 0x80) == 0) {                  // $78E7: a requested direction exists
            const std::uint8_t wantedNext = here->next[wanted & 3];     // INY INY / LDA ($16),Y: byte 2 + direction
            if (isExit(wantedNext)) {
                dir = wanted;                        // $78F9
            } else {
                const std::uint8_t currentNext = here->next[dir & 3];   // $78F1
                if (isExit(currentNext)) neighbour = currentNext;       // $78F7: BPL $7915, keep the current direction
                else dir = wanted;                                      // fall into $78F9; $78FD will find the way blocked
            }
        }
        if (!neighbour) {
            const std::uint8_t n = here->next[dir & 3];                 // $78FD
            if (!isExit(n)) {
                s.zp(0x66) = 2;                      // $7905-$790D: Pac-Man is redrawn standing still, nothing moves
                return JunctionOutcome::Blocked;
            }
            neighbour = n;
        }
        s.pacNextJunction() = *neighbour;            // $7915
        const std::uint8_t at = s.pacJunction();
        if (at == 0x1A || at == 0x1D) {              // the two tunnel ends: 26 (left) and 29 (right)
            s.zp(0x6A) = at == 0x1A ? 6 : 0x15;
            s.zp(0x6C) = 0;
            const std::uint8_t entering = at == 0x1A ? 3 : 2;   // heading out through the tunnel
            s.zp(0x69) = dir == entering ? 1 : 0;
        }
    }
    return s.zp(0x69) != 0 ? JunctionOutcome::Tunnel : JunctionOutcome::Move;   // $795B-$795F
}

}  // namespace pacman::core
