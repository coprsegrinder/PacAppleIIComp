#include "ghostmode.h"

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

void modeTimerStep(GameState& s, GhostShared& sh, GhostQueues& queues) {
    if (sh.ea == 0) sh.eb = static_cast<std::uint8_t>(sh.eb - 1);
    sh.ea = static_cast<std::uint8_t>(sh.ea - 1);
    if (static_cast<unsigned>(sh.ea) + sh.eb != 0) return;            // only an exact zero (no carry out, no remainder) flips the phase
    if (s.cornerPhase == 0) {                                          // chase -> corner
        reverseGhosts(s, queues);
        s.cornerPhase = 1;
        sh.ea = 0x8C;
        sh.eb = 0;
    } else {                                                           // corner -> chase
        reverseGhosts(s, queues);
        s.cornerPhase = 0;
        sh.eb = 0x02;
        sh.ea = 0x58;
    }
}

void energizerGhostEffects(GameState& s, GhostShared& sh, GhostQueues& queues, const GhostModeTables& tables) {
    if (s.level() >= 0x15) return;
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
