// The maze dot pass ($6113) against REAL runs captured on the oracle by tools/capture_dots.py. Needs --image and --dots <scratch/dots_vectors.bin>;
// skipped otherwise. The vector file holds pages drawn from original data: local only, never committed.
// Record = 896-byte map, page before, page after; the C++ pass must reproduce the page after exactly (8,192 bytes).
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "../src/datapack/datapack.h"
#include "../src/hires/hires.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;

namespace {
std::vector<std::uint8_t> readAll(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}
}  // namespace

TEST(dots_pass_equals_real_oracle_runs) {
    if (testfw::config().image.empty() || testfw::config().dots.empty()) {
        std::printf("    SKIPPED (needs --image and --dots)\n");
        return;
    }
    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    const auto dot = loaded.pack.find(0x61A8, hires::kDotTableBytes);
    const auto tile = loaded.pack.find(0x61CA, hires::kTileTableBytes);
    CHECK(!dot.empty() && !tile.empty());
    if (dot.empty() || tile.empty()) return;

    const auto f = readAll(testfw::config().dots);
    CHECK_MSG(f.size() >= 8 && std::memcmp(f.data(), "DOTV", 4) == 0, "bad dots vector file");
    if (f.size() < 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    const std::size_t rec = hires::kMazeMapBytes + 2 * hires::kPageBytes;
    CHECK(count >= 1 && f.size() == 8 + count * rec);
    int dotsChanged = 0;
    for (std::uint32_t i = 0; i < count && 8 + (i + 1) * rec <= f.size(); ++i) {
        const std::uint8_t* base = f.data() + 8 + i * rec;
        hires::Page page;
        for (std::size_t k = 0; k < hires::kPageBytes; ++k) page.set(k, base[hires::kMazeMapBytes + k]);
        hires::drawDots(page, std::span<const std::uint8_t>(base, hires::kMazeMapBytes), dot, tile);
        const std::uint8_t* after = base + hires::kMazeMapBytes + hires::kPageBytes;
        bool same = true;
        for (std::size_t k = 0; k < hires::kPageBytes; ++k) same = same && page.byte(k) == after[k];
        CHECK_MSG(same, "record " + std::to_string(i) + " differs");
        if (std::memcmp(base + hires::kMazeMapBytes, after, hires::kPageBytes) != 0) ++dotsChanged;
    }
    CHECK_MSG(dotsChanged >= 1, "no record actually changed the page");
    std::printf("    %u records compared\n", count);
}

TEST(dots_pass_synthetic) {
    // one dot and one energizer, tables of 0xFF so every touched byte is visible
    std::vector<std::uint8_t> map(hires::kMazeMapBytes, 0), dot(hires::kDotTableBytes, 0xFF), tile(hires::kTileTableBytes, 0xFF);
    map[0 * 32 + 1] = 1;  // column 0, row 1: dot at (59, 11)
    map[1 * 32 + 2] = 2;  // column 1, row 2: tile at (65 - 1, 17 - 1)
    hires::Page p;
    p.clear(0);
    hires::drawDots(p, map, dot, tile);
    CHECK(p.byte(hires::rowOffset(11) + 59 / 7) != 0);
    CHECK(p.byte(hires::rowOffset(16) + 64 / 7) != 0);
    CHECK(p.byte(hires::rowOffset(12) + 59 / 7 + 2) == 0);
    hires::Page q;
    q.clear(0);
    hires::drawDots(q, std::span<const std::uint8_t>(map).first(100), dot, tile);  // too short: nothing happens
    for (std::size_t k = 0; k < hires::kPageBytes; ++k) CHECK(q.byte(k) == 0);
}
