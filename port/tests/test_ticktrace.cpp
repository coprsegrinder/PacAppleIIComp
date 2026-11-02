// Tests of the per-tick trace reader. The synthetic test always runs (builds a small file); the real-trace test needs --ticks a.bin,b.bin,...
// (recorded by tools/record_ticks.py from the golden scenarios) and checks that the C++ chain hash over all recorded states equals the golden hash.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "../src/core/ticktrace.h"
#include "../src/verify/sha256.h"
#include "test_framework.h"

using namespace pacman;
using core::TickTrace;

namespace {

std::string tempPath(const char* name) {
    char dir[MAX_PATH + 2] = {};
    const DWORD n = GetTempPathA(MAX_PATH, dir);
    return std::string(n > 0 && n < MAX_PATH ? dir : ".\\") + name;  // GetTempPath ends with a backslash
}

void put32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<std::uint8_t>(x >> (8 * i)));
}

std::vector<std::uint8_t> synthetic(std::uint32_t count, bool goodTail) {
    std::vector<std::uint8_t> f{'T', 'I', 'C', 'K'};
    put32(f, count);
    put32(f, core::kStateBytes);
    f.insert(f.end(), 32, 0xAB);
    for (std::uint32_t t = 0; t < count; ++t)
        for (std::size_t i = 0; i < core::kStateBytes; ++i) f.push_back(static_cast<std::uint8_t>(t * 7 + i));
    put32(f, 2);  // inputs
    put32(f, 5); put32(f, 0xC1);
    put32(f, 9); put32(f, 0x88);
    put32(f, 1);  // pokes
    put32(f, 3); put32(f, 0x56); put32(f, 3);
    if (!goodTail) f.push_back(0);
    return f;
}

void write(const std::string& path, const std::vector<std::uint8_t>& f) {
    std::ofstream o(path, std::ios::binary | std::ios::trunc);
    o.write(reinterpret_cast<const char*>(f.data()), static_cast<std::streamsize>(f.size()));
}

}  // namespace

TEST(ticktrace_offsets_and_synthetic_file) {
    CHECK(core::stateOffsetOf(0x52) == 0 && core::stateOffsetOf(0x6F) == 29);
    CHECK(core::stateOffsetOf(0x78) == 30 && core::stateOffsetOf(0x79) == 31 && core::stateOffsetOf(0xE8) == 32 && core::stateOffsetOf(0xE9) == 33);
    CHECK(core::stateOffsetOf(0x84) == 34 && core::stateOffsetOf(0xD3) == 113);
    CHECK(core::stateOffsetOf(0x623A) == 114 && core::stateOffsetOf(0x65B9) == 1009);
    CHECK(core::stateOffsetOf(0x70) == -1 && core::stateOffsetOf(0x77) == -1 && core::stateOffsetOf(0x6239) == -1 && core::stateOffsetOf(0x65BA) == -1);

    const std::string path = tempPath("pacman_test_ticks.bin");
    write(path, synthetic(3, true));
    std::string why;
    const auto t = TickTrace::load(path, why);
    CHECK_MSG(t.ok(), why);
    if (!t.ok()) return;
    CHECK(t.ticks() == 3);
    CHECK(t.state(1)[0] == static_cast<std::uint8_t>(7) && t.state(2)[5] == static_cast<std::uint8_t>(2 * 7 + 5));
    CHECK(t.at(1, 0x52) == 7 && t.at(1, 0x56) == static_cast<std::uint8_t>(7 + 4));
    CHECK(t.at(1, 0x70) == 0 && t.at(99, 0x52) == 0);   // outside the state / the trace: 0
    CHECK(t.state(3).empty());
    CHECK(t.inputs().size() == 2 && t.inputs()[1].tick == 9 && t.inputs()[1].key == 0x88);
    CHECK(t.pokes().size() == 1 && t.pokes()[0].address == 0x56 && t.pokes()[0].value == 3);
    CHECK(t.goldenFinalHash()[0] == 0xAB);
    // the chain hash: h0 = SHA256(s0), h1 = SHA256(h0 + s1), ...
    std::array<std::uint8_t, 32> h{};
    for (std::size_t k = 0; k < 3; ++k) {
        std::vector<std::uint8_t> buf;
        if (k) buf.insert(buf.end(), h.begin(), h.end());
        const auto s = t.state(k);
        buf.insert(buf.end(), s.begin(), s.end());
        h = verify::sha256(buf);
    }
    CHECK(t.chainHash() == h);
    // damaged files are refused
    write(path, synthetic(3, false));
    CHECK(!TickTrace::load(path, why).ok());
    auto cut = synthetic(3, true);
    cut.resize(cut.size() - 7);
    write(path, cut);
    CHECK(!TickTrace::load(path, why).ok());
    auto wrong = synthetic(3, true);
    wrong[0] = 'X';
    write(path, wrong);
    CHECK(!TickTrace::load(path, why).ok());
    CHECK(!TickTrace::load(tempPath("pacman_no_such_ticks.bin"), why).ok());
    std::remove(path.c_str());
}

TEST(ticktrace_real_golden_recordings_have_the_golden_chain_hash) {
    if (testfw::config().ticks.empty()) {
        std::printf("    SKIPPED (needs --ticks)\n");
        return;
    }
    const std::string& list = testfw::config().ticks;
    int files = 0;
    for (std::size_t pos = 0; pos <= list.size();) {
        const std::size_t comma = list.find(',', pos);
        const std::size_t end = comma == std::string::npos ? list.size() : comma;
        const std::string path = list.substr(pos, end - pos);
        pos = end + 1;
        std::string why;
        const auto t = TickTrace::load(path, why);
        CHECK_MSG(t.ok(), path + ": " + why);
        if (!t.ok()) continue;
        ++files;
        CHECK(t.chainHash() == t.goldenFinalHash());  // the recording IS the golden run
        CHECK(t.ticks() > 1000);
        CHECK(!t.inputs().empty());
        // plausibility from the analysis: three lives at the start ($58 = $83 per PACMAN_RULES.md), level 1, 244+4 items left (or fewer later)
        CHECK(t.at(0, 0x58) == 0x83);
        CHECK(t.at(0, 0x55) == 1);
        std::printf("    %s: %zu ticks, %zu inputs, %zu pokes, chain hash equals the golden one\n", path.c_str(), t.ticks(), t.inputs().size(), t.pokes().size());
    }
    CHECK(files >= 1);
}
