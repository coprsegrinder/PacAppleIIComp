// Tests of the sprite calls of a tick (src/core/tickdraw): the comparison with the oracle's call log of the first ticks of scenario A
// (needs --image, --ticks scratch/ticks_A.bin and --tickcalls scratch/tick_calls_A.txt from tools/record_tick_calls.py): for every tick the sprite calls
// (types 3 and 1) derived from the states at the start and at the end of the tick must equal the logged ones, in order. A second test executes those calls on a
// page that starts as the first play screen and compares the page with the oracle's every 10th tick (--pages scratch/pages_A.bin).
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "../src/core/lifestart.h"
#include "../src/core/screen.h"
#include "../src/core/tickdraw.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;
using namespace pacman::core;

namespace {
std::vector<std::uint8_t> readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
}  // namespace

TEST(tickdraw_synthetic_rules) {
    // no pack: nothing is returned (the tables are missing) and nothing crashes
    datapack::Pack empty;
    GameState a, b;
    b.pacX() = static_cast<std::uint8_t>(a.pacX() + 2);
    CHECK(spriteCalls(a, b, empty).empty());
    int flag = 0;
    CHECK(ghostImageBase(a, 0, 0, empty, flag) == 0);
}

TEST(tickdraw_against_the_oracle_call_log) {
    if (testfw::config().image.empty() || testfw::config().ticks.empty() || testfw::config().tickcalls.empty()) {
        std::printf("    SKIPPED (needs --image, --ticks and --tickcalls)\n");
        return;
    }
    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    std::string why;
    const auto tr = TickTrace::load(testfw::config().ticks.substr(0, testfw::config().ticks.find(',')), why);
    CHECK_MSG(tr.ok(), why);
    if (!tr.ok()) return;
    // parse the log: blocks 'tick N' followed by '  type T p0 .. p4'
    std::vector<std::vector<DrawCall>> logged;
    std::ifstream in(testfw::config().tickcalls);
    std::string line;
    bool started = false;
    while (std::getline(in, line)) {
        if (line.rfind("tick ", 0) == 0) {
            started = true;
            logged.emplace_back();
        } else if (started) {
            std::istringstream ss(line);
            std::string w;
            DrawCall c;
            ss >> w >> c.type;
            for (auto& v : c.p) ss >> v;
            const bool blink = c.type == 2 || (c.type == 6 && (c.p[0] == 25034 || (c.p[0] == 29185 && c.p[1] == 47 && c.p[2] == 6)));   // energizer and 1UP blinks: not rebuilt
            if ((c.type == 1 || c.type == 3 || c.type == 4 || c.type == 5 || c.type == 6) && !blink) logged.back().push_back(c);
        }
    }
    CHECK(logged.size() > 500);
    std::size_t ticks = 0, withCalls = 0, bad = 0, firstBad = 0, calls = 0;
    std::array<std::uint8_t, 3> high{0, 0, 0};
    for (std::size_t k = 0; k + 1 < logged.size() && k + 1 < tr.ticks() && k < 700; ++k) {   // the log run goes on a little after tick 700 without the scenario's keys
        const GameState a = fromOracleLayout(tr.state(k));
        const GameState b = fromOracleLayout(tr.state(k + 1));
        auto mine = tickCalls(a, b, loaded.pack, tr.at(k, 0x59) == 1, high);
        mine.erase(std::remove_if(mine.begin(), mine.end(), [](const DrawCall& c) { return c.type == 8; }), mine.end());   // door lines: not in the collector's log
        ++ticks;
        calls += logged[k].size();
        withCalls += !logged[k].empty();
        if (!(mine == logged[k])) {
            if (bad++ == 0) firstBad = k;
            if (bad <= 5) {
                std::printf("    tick %zu: mine", k);
                for (const auto& c : mine) std::printf(" [%d %d %d %d]", c.type, c.p[0], c.p[1], c.p[2]);
                std::printf(" | oracle");
                for (const auto& c : logged[k]) std::printf(" [%d %d %d %d]", c.type, c.p[0], c.p[1], c.p[2]);
                std::printf("\n");
            }
        }
    }
    std::printf("    %zu ticks compared, %zu with sprite calls (%zu calls), %zu differ%s\n", ticks, withCalls, calls, bad, bad ? (" (first at tick " + std::to_string(firstBad) + ")").c_str() : "");
    CHECK(bad == 0);
    CHECK(withCalls > 100);
}

TEST(tickdraw_pages_every_10_ticks_equal_the_oracle) {
    if (testfw::config().image.empty() || testfw::config().ticks.empty() || testfw::config().pages.empty()) {
        std::printf("    SKIPPED (needs --image, --ticks and --pages)\n");
        return;
    }
    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    std::string why;
    const auto tr = TickTrace::load(testfw::config().ticks.substr(0, testfw::config().ticks.find(',')), why);
    CHECK_MSG(tr.ok(), why);
    if (!tr.ok()) return;
    const auto f = readAll(testfw::config().pages);
    CHECK(f.size() > 8 && std::memcmp(f.data(), "PAGS", 4) == 0);
    if (f.size() < 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    const std::size_t rec = 4 + hires::kPageBytes;
    CHECK(f.size() == 8 + count * rec);
    const auto& pack = loaded.pack;
    // the page of the first tick: the new game's screen (verified exact in test_screen)
    GameState first;
    CHECK(newGameState(pack, first));
    hires::Page page;
    CHECK(drawPlayfield(page, first, pack));
    CHECK(drawHud(page, first, pack));
    CHECK(drawPacman(page, first, pack, false));
    const auto mask = pack.find(0x69CD, hires::kMaskTableBytes);
    const auto box = pack.find(0x6992, 12);
    const auto dotTable = pack.find(0x61A8, hires::kDotTableBytes);
    CHECK(!mask.empty() && !box.empty() && !dotTable.empty());
    std::array<std::uint8_t, 3> highScore{0, 0, 0};
    // areas this piece does not draw: the score and high-score digits (rows 6-22 and 84-100, columns 0-7) and the four blinking energizers
    auto excluded = [](int y, int c) {
        if (y >= 6 && y <= 13 && c <= 7) return true;   // the blinking 1UP label
        for (int ex : {64, 214})
            for (int ey : {22, 142})
                if (y >= ey - 1 && y <= ey + 6 && c >= (ex - 1) / 7 && c <= (ex - 1) / 7 + 2) return true;
        return false;
    };
    std::size_t compared = 0, badPages = 0, badBytes = 0, laterPages = 0, laterDiffer = 0, nextPageSkipped = 0;
    std::uint32_t nextPage = 0;
    for (std::size_t k = 0; k < 700 && k + 1 < tr.ticks(); ++k) {
        if (k % 10 == 0 && nextPage < count) {
            const std::uint8_t* rp = f.data() + 8 + nextPage * rec;
            std::uint32_t tick = 0;
            std::memcpy(&tick, rp, 4);
            if (tick == k) {
                ++nextPage;
                std::size_t diff = 0;
                for (int y = 0; y < 192; ++y)
                    for (int c = 0; c < 40; ++c) {
                        const std::size_t off = hires::rowOffset(y) + static_cast<std::size_t>(c);
                        if (excluded(y, c)) continue;
                        if (page.byte(off) != rp[4 + off]) {
                            if (badPages == 0 && diff < 6) std::printf("    first bad page, tick %zu: row %d column %d mine %02X oracle %02X\n", k, y, c, page.byte(off), rp[4 + off]);
                            ++diff;
                        }
                    }
                // The first dot restoration after a ghost step (a small-dot call, not rebuilt yet) happens in tick 357: pages up to the start of tick 357 must be
                // exact; later pages are counted only (they differ by the missing dot redraws, see docs/M4_PLAN.md).
                if (false) {
                    laterDiffer += diff != 0;
                    ++laterPages;
                    ++nextPageSkipped;
                    diff = 0;
                } else {
                    ++compared;
                }
                if (diff != 0) {
                    ++badPages;
                    badBytes += diff;
                    if (badPages <= 3) std::printf("    page at tick %zu: %zu bytes differ\n", k, diff);
                }
            }
        }
        const GameState a = fromOracleLayout(tr.state(k));
        const GameState b = fromOracleLayout(tr.state(k + 1));
        for (const auto& c : tickCalls(a, b, pack, tr.at(k, 0x59) == 1, highScore)) {
            if (c.type == 3) {
                hires::maskedClear(page, c.p[1], c.p[2], mask);
            } else if (c.type == 8) {
                hires::hLine(page, c.p[0], c.p[1], c.p[2], c.p[3], c.p[4] != 0);   // empirical: the oracle shows the palette bit set for a ghost whose flag is 1 (ghost 1) and clear for flag 0 (ghost 3); not understood
            } else if (c.type == 4) {
                hires::boxClear(page, box[static_cast<std::size_t>(c.p[0]) * 2], box[static_cast<std::size_t>(c.p[0]) * 2 + 1]);
            } else if (c.type == 5) {
                hires::smallDot(page, c.p[0], c.p[1], dotTable);
            } else if (c.type == 6) {
                const auto t = pack.find(static_cast<std::uint16_t>(c.p[0]), hires::kTileTableBytes);
                if (!t.empty()) hires::tile(page, c.p[1], c.p[2], t);
            } else {
                const auto image = pack.find(static_cast<std::uint16_t>(c.p[0]), hires::kSevenCopies3x10);
                if (!image.empty()) hires::drawImage(page, c.p[1], c.p[2], image, c.p[3] != 0 && c.p[4] != 0);
            }
        }
    }
    std::printf("    %zu pages compared (outside the score digits and the energizers), %zu differ (%zu bytes)\n", compared, badPages, badBytes);
    CHECK(compared >= 30 && badPages == 0);
}
