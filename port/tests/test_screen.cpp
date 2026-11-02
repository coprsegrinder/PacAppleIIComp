// Tests of the play screen drawing (src/core/screen): Pac-Man's frame choice, and the first-tick page against the oracle's (needs --image and
// --page0 scratch/page_tick0.bin made by tools/record_page0.py). The page drawn from the NEW-GAME STATE (walls, door, dots, Pac-Man) must equal the
// oracle's hi-res page at the first tick of a game everywhere except in the areas that later parts draw (score and labels, the lives and fruit icons); those areas are listed here, and the test also checks that the differences stay inside them.
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/core/lifestart.h"
#include "../src/core/screen.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;
using namespace pacman::core;

namespace {

struct Rect {
    int y0, y1, c0, c1;   // rows (inclusive) and byte columns (inclusive)
    const char* what;
};

// Areas that the first tick shows but this part does not draw yet.
constexpr Rect kNotYet[] = {
    {6, 22, 0, 7, "score and its label (top left)"},
    {84, 100, 0, 7, "second text block (left)"},
    {174, 187, 0, 35, "lives and fruit icons (bottom)"},
};

std::vector<std::uint8_t> readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

}  // namespace

TEST(screen_pacman_frame_choice) {
    GameState s;
    s.pacDirection() = 3;
    s.zp(0x66) = 0;
    CHECK(pacmanFrame(s) == 0);         // phase 0: closed mouth, whatever the direction
    s.zp(0x66) = 1;
    CHECK(pacmanFrame(s) == 4);         // direction + 1
    s.zp(0x66) = 2;
    CHECK(pacmanFrame(s) == 8);         // direction + 5
    s.zp(0x66) = 3;
    CHECK(pacmanFrame(s) == 4);
    s.zp(0x66) = 0x0E;                  // only the low two bits matter (the counter is kept to four bits)
    CHECK(pacmanFrame(s) == 8);
    s.pacDirection() = 0;
    s.zp(0x66) = 1;
    CHECK(pacmanFrame(s) == 1);
}

TEST(screen_first_tick_page_equals_the_oracle_outside_the_known_gaps) {
    if (testfw::config().image.empty() || testfw::config().page0.empty()) {
        std::printf("    SKIPPED (needs --image and --page0)\n");
        return;
    }
    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    const auto oracle = readAll(testfw::config().page0);
    CHECK(oracle.size() == hires::kPageBytes);
    if (oracle.size() != hires::kPageBytes) return;

    GameState state;
    CHECK(newGameState(loaded.pack, state));
    hires::Page page;
    CHECK(drawPlayfield(page, state, loaded.pack));
    CHECK(drawPacman(page, state, loaded.pack, false));   // the first draw of a life ($5839) clears the palette bit

    std::size_t differing = 0, outside = 0;
    std::array<std::size_t, 3> perArea{};
    for (int y = 0; y < 192; ++y) {
        for (int c = 0; c < 40; ++c) {
            const std::size_t off = hires::rowOffset(y) + static_cast<std::size_t>(c);
            if (page.byte(off) == oracle[off]) continue;
            ++differing;
            bool inside = false;
            for (std::size_t r = 0; r < 3; ++r) {
                if (y >= kNotYet[r].y0 && y <= kNotYet[r].y1 && c >= kNotYet[r].c0 && c <= kNotYet[r].c1) {
                    inside = true;
                    ++perArea[r];
                }
            }
            if (!inside) {
                ++outside;
                if (outside <= 5) std::printf("    differs outside the known gaps: row %d column %d mine %02X oracle %02X\n", y, c, page.byte(off), oracle[off]);
            }
        }
    }
    std::printf("    %zu bytes differ from the oracle's page, %zu of them outside the known gaps (score %zu, text %zu, icons %zu)\n", differing, outside, perArea[0],
                perArea[1], perArea[2]);
    CHECK(outside == 0);
    CHECK(differing > 50);   // the gaps are really there: the test is not comparing an empty thing
}
