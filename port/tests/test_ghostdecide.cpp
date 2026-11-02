// TASK-030e4: the C++ transliteration of the 48 ghost decision routines against the original run on the oracle (tools/record_decide_vectors.py, --decide <file>).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "../src/core/ghostdecide.h"
#include "test_framework.h"

using namespace pacman::core;

TEST(ghostdecide_synthetic) {
    DecisionEnv env;
    CHECK(ghostDecisionRun(0x8E1E, 1, 0, 0, env) == 2);   // routine 0: incoming direction 1 -> 2, anything else -> 0
    CHECK(ghostDecisionRun(0x8E1E, 3, 0, 0, env) == 0);
    CHECK(ghostDecisionRun(0x8E28, 1, 0, 0, env) == 3);   // routine 1
    CHECK(ghostDecisionRun(0x8E32, 0, 0, 0, env) == 2 && ghostDecisionRun(0x8E32, 2, 0, 0, env) == 1);   // routine 2
}

TEST(ghostdecide_against_the_oracle_vectors) {
    if (testfw::config().decide.empty()) {
        std::printf("    SKIPPED (needs --decide scratch/decide_vectors.bin)\n");
        return;
    }
    std::ifstream f(testfw::config().decide, std::ios::binary);
    const std::vector<std::uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(d.size() >= 8 && std::memcmp(d.data(), "DECV", 4) == 0);
    if (d.size() < 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, d.data() + 4, 4);
    CHECK(d.size() == 8 + static_cast<std::size_t>(count) * 15);
    std::size_t bad = 0, changedDir = 0, entries = 0;
    std::uint16_t last = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint8_t* r = d.data() + 8 + static_cast<std::size_t>(i) * 15;
        const std::uint16_t entry = static_cast<std::uint16_t>(r[0] | (r[1] << 8));
        if (entry != last) {
            ++entries;
            last = entry;
        }
        DecisionEnv env;
        env.ghostX = r[5];
        env.ghostY = r[6];
        env.ghostDir = r[7];
        env.pacDir = r[8];
        env.pacTunnel = r[9];
        const std::uint8_t a = ghostDecisionRun(entry, r[2], r[3], r[4], env);
        const bool ok = r[14] == 1 && a == r[10] && env.ghostX == r[11] && env.ghostY == r[12] && env.ghostDir == r[13];
        changedDir += env.ghostDir != r[7];
        if (!ok && bad++ < 5) std::printf("    entry %04X A=%u X=%u Y=%u: mine %u, oracle %u\n", entry, r[2], r[3], r[4], a, r[10]);
    }
    std::printf("    %u vectors over %zu routine entries (%zu change the ghost's own direction register): %zu differ\n", count, entries, changedDir, bad);
    CHECK(bad == 0 && count >= 50000 && entries >= 48);
}
