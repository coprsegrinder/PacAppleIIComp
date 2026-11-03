// Shared helpers of the flow tests (TASK-033-4e3).
#pragma once

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/core/attractflow.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

namespace flowtest {

inline const char* romPath() { return "Apple II roms/Apple II plus ROM Pages F8-FF - 341-0020 - Autostart Monitor.bin"; }

inline std::vector<std::uint8_t> readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

inline bool loadPack(pacman::datapack::Pack& pack) {
    using namespace pacman;
    const auto built = datapack::buildPack(readAll(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    if (!built.ok) return false;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    if (!loaded.ok) return false;
    pack = loaded.pack;
    return true;
}

// Lets the code that makes the current prompt run out; a key in the latch is taken as soon as the prompt is complete (advance(0)), and the code it starts runs out too.
inline void settle(pacman::core::AttractFlow& flow) {
    flow.advance(0);
    for (int guard = 0; guard < 100 && flow.busyCycles() > 0; ++guard) {
        flow.advance(flow.busyCycles());
        flow.advance(0);
    }
}

// the bytes of the monitor's zero page and the random counter that the tests compare (the window, the cursor, the base address of the line, the flags, the vectors, $4E/$4F)
// `rndShift`: the random counter ($4E/$4F) of the port may be ahead of the oracle's by the loops it ran while it waited for the first key (the oracle's keys come at once)
inline std::size_t zeroPageDiff(const std::array<std::uint8_t, 256>& zp, const std::uint8_t* oracleZp, int rndShift = 0) {
    std::size_t d = 0;
    for (unsigned i = 0x20; i < 0x40; ++i) {
        if (zp[i] != oracleZp[i]) {
            ++d;
            std::printf("      zero page $%02X: mine %02X oracle %02X\n", i, zp[i], oracleZp[i]);
        }
    }
    const int mine = zp[0x4E] | (zp[0x4F] << 8), theirs = oracleZp[0x4E] | (oracleZp[0x4F] << 8);
    d += (((mine - theirs - rndShift) & 0xFFFF) != 0) ? 1 : 0;
    return d;
}

inline int rndOf(const pacman::core::Monitor& m) { return m.peek(0x4E) | (m.peek(0x4F) << 8); }

}  // namespace flowtest
