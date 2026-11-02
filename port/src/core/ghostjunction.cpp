#include "ghostjunction.h"

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

JunctionStepResult ghostJunction(GameState& s, int ghostNumber, GhostQueues& queues, const JunctionGraph& graph, const GhostDecisionData& data, const GhostTables& tables) {
    JunctionStepResult r;
    GhostRecord& g = s.ghosts[static_cast<std::size_t>(ghostNumber - 1) & 3];
    auto& queue = queues[static_cast<std::size_t>(ghostNumber - 1) & 3];
    const std::uint8_t x = g.b[0], y = g.b[1];
    // $87C0-$87EA: the position equals the current junction's, or else the one it is heading to
    std::uint8_t found;
    if (graph.at(g.b[3]).x == x && graph.at(g.b[3]).y == y) found = g.b[3];
    else if (graph.at(g.b[4]).x == x && graph.at(g.b[4]).y == y) found = g.b[4];
    else return r;
    r.onJunction = true;
    r.junction = found;
    g.b[3] = found;                                           // $87ED: STX $73
    // $8C50: the decision, unless the queue still holds a move
    std::uint8_t qi = g.b[7];                                 // $77
    if (g.b[9] != 0) {                                        // eyes: the fixed route home
        queue[static_cast<std::uint8_t>(qi + 1)] = data.returnRoute[found & 63];
        queue[static_cast<std::uint8_t>(qi + 2)] = 0xFF;
        r.decided = true;
    } else if ((queue[static_cast<std::uint8_t>(qi + 1)] & 0x80) != 0) {
        const std::uint8_t index = static_cast<std::uint8_t>(found * 4 + ghostNumber - 1);
        const std::uint16_t entry = data.routineAddress[data.routineIndex[index] % 48];
        const bool corner = g.b[8] != 0 || s.cornerPhase != 0;
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
    if (!same && g.b[9] == 0) g.b[16] = tables.turn[std::min<std::size_t>(s.level(), 15)];   // $8802-$8809
    g.b[4] = graph.at(found).next[g.b[2] & 3];                                               // $880B-$8811
    // $8813-$8864: the special junctions
    if (found == 0x17 || found == 0x18) {                                                    // the house door: only an eaten ghost does anything here
        if (g.b[9] != 0) {
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
        }
    } else if (found == 0x1A) {                                                              // the left end of the tunnel row
        g.b[11] = 6;                                                                         // $7B
        g.b[13] = 0;                                                                         // $7D
        g.b[10] = g.b[2] == 3 ? 1 : 0;                                                       // $7A: slow while heading into the tunnel
    } else if (found == 0x1D) {                                                              // the right end
        g.b[11] = 0x15;
        g.b[13] = 0;
        g.b[10] = g.b[2] == 2 ? 1 : 0;
    }
    return r;
}

namespace {

// $88ED-$8957: the cell counters (3 sub-steps of 2 units per cell)
void counters(GhostRecord& g) {
    switch (g.b[2] & 3) {
        case 2:   // right: $7D counts 0, 2, 4
            g.b[13] = static_cast<std::uint8_t>(g.b[13] + 2);
            if (g.b[13] == 6) {
                g.b[13] = 0;
                ++g.b[11];
            }
            break;
        case 3:   // left
            g.b[13] = static_cast<std::uint8_t>(g.b[13] - 2);
            if (g.b[13] == 0xFE) {
                g.b[13] = 4;
                --g.b[11];
            }
            break;
        case 0:   // down: $7E / $7C
            g.b[14] = static_cast<std::uint8_t>(g.b[14] + 2);
            if (g.b[14] == 6) {
                g.b[14] = 0;
                ++g.b[12];
            }
            break;
        default:  // up
            g.b[14] = static_cast<std::uint8_t>(g.b[14] - 2);
            if (g.b[14] == 0xFE) {
                g.b[14] = 4;
                --g.b[12];
            }
            break;
    }
}

// $887A-$889A: two units along the direction of travel
void plainMove(GhostRecord& g) {
    switch (g.b[2] & 3) {
        case 0: g.b[1] = static_cast<std::uint8_t>(g.b[1] + 2); break;   // down
        case 1: g.b[1] = static_cast<std::uint8_t>(g.b[1] - 2); break;   // up
        case 2: g.b[0] = static_cast<std::uint8_t>(g.b[0] + 2); break;   // right
        default: g.b[0] = static_cast<std::uint8_t>(g.b[0] - 2); break;  // left
    }
}

// $8A9C-$8B12: the tunnel move. Right half (x >= $60) or left half; the wrap exchanges the junction numbers and moves the ghost to the other end.
void tunnelMove(GhostRecord& g) {
    const bool oddDir = (g.b[2] & 1) != 0;
    if (g.b[0] >= 0x60) {
        if (!oddDir) {
            g.b[0] = static_cast<std::uint8_t>(g.b[0] + 2);
            if (g.b[0] == 0xDD) {
                g.b[3] = g.b[4];
                g.b[4] = 0x1D;
                g.b[0] = 0x3D;
                g.b[11] = 0;
                g.b[13] = 0;
            }
        } else {
            g.b[0] = static_cast<std::uint8_t>(g.b[0] - 2);
        }
    } else if (!oddDir) {
        g.b[0] = static_cast<std::uint8_t>(g.b[0] + 2);
    } else {
        g.b[0] = static_cast<std::uint8_t>(g.b[0] - 2);
        if (g.b[0] == 0x3B) {
            g.b[3] = g.b[4];
            g.b[4] = 0x1A;
            g.b[0] = 0xDB;
            g.b[11] = 0x1C;
            g.b[13] = 0;
        }
    }
}

// $8988-$8A38: the house. The release timer $82 counts down; when it reaches zero the exit moves are queued (and, in the corner phase, the ghost's script).
// At the ends of its way inside the box (and at the door row) the ghost takes the next queued move.
void houseStep(GameState& s, GhostRecord& g, int ghostNumber, GhostQueues& queues, const GhostDecisionData& data) {
    auto& queue = queues[static_cast<std::size_t>(ghostNumber - 1) & 3];
    g.b[18] = static_cast<std::uint8_t>(g.b[18] - 1);
    if (g.b[18] == 0) {
        std::uint8_t y = g.b[7];
        auto push = [&](std::uint8_t v) { queue[++y] = v; };
        if ((ghostNumber & 1) != 0) {                            // ghosts 1 and 3
            if (g.b[2] == 0) push(1);
            push(1);
        } else {                                                 // ghosts 2 and 4
            if (g.b[2] == 1) push(0);
            push(ghostNumber == 2 ? 2 : 3);
            push(1);
            push(1);
        }
        ++y;                                                     // $89C7
        if (s.cornerPhase == 0) {
            queue[y] = 3;
            queue[static_cast<std::uint8_t>(y + 1)] = 0xFF;
        } else {                                                 // $6EA5: the 16-byte script of this ghost
            for (std::size_t j = 0; j < 16; ++j) queue[static_cast<std::uint8_t>(y + j)] = data.houseScripts[static_cast<std::size_t>(ghostNumber)][j];
        }
    }
    bool advance = false;
    bool centre = true;                                          // look at the middle column ($8A05)
    if ((ghostNumber & 1) == 0) {                                // ghosts 2 and 4 have their own column inside the box
        const std::uint8_t column = ghostNumber == 2 ? 0x7F : 0x99;
        if (g.b[0] == column) {
            advance = g.b[1] == 0x55 || g.b[1] == 0x5D;
            centre = false;
        }
    }
    if (centre && g.b[0] == 0x8B) {
        if (g.b[1] == 0x5D || g.b[1] == 0x55) {
            advance = true;
        } else if (g.b[1] == 0x47) {                             // the door row: out of the house
            g.b[3] = 0x17;
            g.b[4] = 0x18;
            g.b[11] = 0x0D;
            g.b[12] = 0x0B;
            g.b[13] = 2;
            g.b[14] = 0;
            g.b[17] = 0;
            advance = true;
        }
    }
    if (advance) {
        ++g.b[7];
        g.b[2] = queue[g.b[7]];
    }
}

}  // namespace

void ghostMove(GameState& s, int ghostNumber, GhostQueues& queues, const GhostDecisionData& data) {
    GhostRecord& g = s.ghosts[static_cast<std::size_t>(ghostNumber - 1) & 3];
    if (g.b[10] != 0) {
        tunnelMove(g);                                           // $8A9C (the slow flag is tested before the house flag)
    } else {
        const bool inHouse = g.b[17] != 0;
        if (inHouse) houseStep(s, g, ghostNumber, queues, data);
        plainMove(g);
        if (g.b[17] != 0) return;                                // $88DC: a ghost that is (still) in the house returns here: no contact test, no cell counters
    }
    counters(g);
}

}  // namespace pacman::core
