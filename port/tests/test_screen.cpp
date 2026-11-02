// Tests of the play screen drawing (src/core/screen): Pac-Man's frame choice, and the first-tick page against the oracle's (needs --image and
// --page0 scratch/page_tick0.bin made by tools/record_page0.py). The page drawn from the NEW-GAME STATE (walls, door, dots, score area, labels, icons,
// oracle's hi-res page at the first tick of a game everywhere except in the areas that later parts draw (score and labels, the lives and fruit icons); those areas are listed here, and the test also checks that the differences stay inside them.
#include <cstdint>
#include <cstdlib>
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
    CHECK(drawHud(page, state, loaded.pack));
    CHECK(drawPacman(page, state, loaded.pack, false));   // the first draw of a life ($5839) clears the palette bit

    std::size_t differing = 0, outside = 0;
    std::array<std::size_t, 3> perArea{};
    for (int y = 0; y < 192; ++y) {
        for (int c = 0; c < 40; ++c) {
            const std::size_t off = hires::rowOffset(y) + static_cast<std::size_t>(c);
            if (page.byte(off) == oracle[off]) continue;
            ++differing;
            if (differing <= 20) std::printf("    row %d column %d mine %02X oracle %02X\n", y, c, page.byte(off), oracle[off]);
            bool inside = false;
            for (std::size_t r = 0; r < 3; ++r) {
                if (y >= kNotYet[r].y0 && y <= kNotYet[r].y1 && c >= kNotYet[r].c0 && c <= kNotYet[r].c1) {
                    inside = true;
                    ++perArea[r];
                }
            }
            if (!inside) {
                ++outside;
                if (outside <= 5 || true) std::printf("    differs outside the known gaps: row %d column %d mine %02X oracle %02X\n", y, c, page.byte(off), oracle[off]);
            }
        }
    }
    std::printf("    %zu bytes differ from the oracle's page, %zu of them outside the known gaps (score %zu, text %zu, icons %zu)\n", differing, outside, perArea[0],
                perArea[1], perArea[2]);
    CHECK(differing == 0);   // the complete first page of a game: all 8,192 bytes
    std::size_t lit = 0;
    for (std::size_t k = 0; k < hires::kPageBytes; ++k) lit += page.byte(k) != 0x80;
    CHECK(lit > 1000);   // not an empty page: maze, dots, labels, Pac-Man
}

// TASK-029g15: the high score survives from one game to the next ($00-$02 are set once at the program's start and never cleared by a new game): the first page
// of a SECOND game shows the first game's best score and a zero score. scratch/page_tick0_game2.bin and scratch/high_game2.txt come from tools/record_second_game.py.
TEST(screen_second_game_first_page_shows_the_high_score_of_the_first_game) {
    if (testfw::config().image.empty() || testfw::config().page0.empty()) {
        std::printf("    SKIPPED (needs --image and --page0)\n");
        return;
    }
    std::string base = testfw::config().page0;
    const std::size_t at = base.rfind("page_tick0.bin");
    if (at == std::string::npos) {
        std::printf("    SKIPPED (--page0 is not named page_tick0.bin)\n");
        return;
    }
    base.resize(at);
    const auto oracle = readAll(base + "page_tick0_game2.bin");
    const auto info = readAll(base + "high_game2.txt");
    if (oracle.size() != hires::kPageBytes || info.empty()) {
        std::printf("    SKIPPED (no page_tick0_game2.bin / high_game2.txt next to --page0)\n");
        return;
    }
    const std::string text(info.begin(), info.end());
    const std::size_t g2 = text.find("\"game2_tick0\"");
    unsigned h0 = 0, h1 = 0, h2 = 0;
    CHECK(g2 != std::string::npos);
    if (g2 == std::string::npos) return;
    const std::size_t hi = text.find("\"hi\": [", g2);
    CHECK(hi != std::string::npos);
    if (hi == std::string::npos) return;
    {
        const char* q = text.c_str() + hi + 7;   // after "hi": [
        char* end = nullptr;
        h0 = static_cast<unsigned>(std::strtoul(q, &end, 10));
        h1 = static_cast<unsigned>(std::strtoul(end + 1, &end, 10));
        h2 = static_cast<unsigned>(std::strtoul(end + 1, &end, 10));
    }
    CHECK(h0 + h1 + h2 != 0);   // game 1 really scored

    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    GameState state;
    CHECK(newGameState(loaded.pack, state));
    hires::Page page;
    CHECK(drawPlayfield(page, state, loaded.pack));
    CHECK(drawHud(page, state, loaded.pack, {static_cast<std::uint8_t>(h0), static_cast<std::uint8_t>(h1), static_cast<std::uint8_t>(h2)}));
    CHECK(drawPacman(page, state, loaded.pack, false));
    std::size_t differing = 0;
    for (std::size_t k = 0; k < hires::kPageBytes; ++k) differing += page.byte(k) != oracle[k];
    std::printf("    second game: high score %02X%02X%02X, %zu bytes differ from the oracle's page\n", h2, h1, h0, differing);
    CHECK(differing == 0);
}
