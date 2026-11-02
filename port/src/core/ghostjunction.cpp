#include "ghostjunction.h"

#include <algorithm>

namespace pacman::core {

bool GhostDecisionData::load(const datapack::Pack& pack) {
    const auto idx = pack.find(0x8CBE, 256), addr = pack.find(0x8DBE, 96), cor = pack.find(0x8CB4, 10), route = pack.find(0xA360, 64);
    if (idx.empty() || addr.empty() || cor.empty() || route.empty()) return false;
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
    return r;
}

}  // namespace pacman::core
