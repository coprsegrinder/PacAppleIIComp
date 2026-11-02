// Tests of the per-tick drawing (src/core/tickdraw): for each golden scenario whose files are present (convention, in the directory of the --ticks files):
//   ticks_X.bin (states), tick_calls_X.txt (the oracle's drawing calls per tick), pages_X.bin (the oracle's hi-res page every 10th tick),
//   extra_X.bin (6 bytes per tick: $E1 $E2 $E5 $E6 $E7 $E4 at the start of the tick), all from tools/record_tick_calls.py (local, git-ignored),
// 1. the calls derived from the states of the tick (sprites, score display, dot redraw, door, blinks) equal the oracle's logged calls (the door lines and the
//    erase calls of the blinks are not logged by the collector, so those are removed from the comparison), tick by tick, in order;
// 2. executing ALL derived calls on the first-screen page gives the oracle's page at every 10th tick, byte for byte (no area is excluded any more).
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

std::vector<std::string> split(const std::string& list) {
    std::vector<std::string> out;
    for (std::size_t pos = 0; pos <= list.size();) {
        const std::size_t comma = list.find(',', pos);
        const std::size_t end = comma == std::string::npos ? list.size() : comma;
        if (end > pos) out.push_back(list.substr(pos, end - pos));
        pos = end + 1;
    }
    return out;
}

struct Scenario {
    std::string letter, ticksPath, callsPath, pagesPath, extraPath;
};

// ticks_X.bin -> the sibling files of scenario X (only A and D have been recorded)
std::vector<Scenario> scenarios() {
    std::vector<Scenario> out;
    for (const auto& p : split(testfw::config().ticks)) {
        const std::size_t slash = p.find_last_of("/\\");
        const std::string dir = slash == std::string::npos ? std::string() : p.substr(0, slash + 1);
        const std::string name = p.substr(slash == std::string::npos ? 0 : slash + 1);
        if (name.size() != 11 || name.rfind("ticks_", 0) != 0 || name.substr(7) != ".bin") continue;   // ticks_X.bin
        Scenario s{std::string(1, name[6]), p, dir + "tick_calls_" + name[6] + ".txt", dir + "pages_" + name[6] + ".bin", dir + "extra_" + name[6] + ".bin"};
        if (readAll(s.callsPath).empty() || readAll(s.pagesPath).empty() || readAll(s.extraPath).empty()) continue;
        out.push_back(s);
    }
    return out;
}

bool loadPack(datapack::Pack& pack) {
    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    if (!built.ok) return false;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    if (!loaded.ok) return false;
    pack = loaded.pack;
    return true;
}

std::vector<std::vector<DrawCall>> parseLog(const std::string& path) {
    std::vector<std::vector<DrawCall>> logged;
    std::ifstream in(path);
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
            logged.back().push_back(c);
        }
    }
    return logged;
}

TickExtras extrasAt(const std::vector<std::uint8_t>& e, std::size_t tick) {
    TickExtras x;
    if ((tick + 1) * 6 <= e.size()) {
        const std::uint8_t* p = e.data() + tick * 6;
        x.e1 = p[0];
        x.e2 = p[1];
        x.e5 = p[2];
        x.e6 = p[3];
        x.e7 = p[4];
        x.e4 = p[5];
    }
    return x;
}

}  // namespace

TEST(tickdraw_synthetic_rules) {
    datapack::Pack empty;
    GameState a, b;
    b.pacX() = static_cast<std::uint8_t>(a.pacX() + 2);
    CHECK(spriteCalls(a, b, empty).empty());
    int flag = 0;
    CHECK(ghostImageBase(a, 0, 0, empty, flag) == 0);
    hires::Page page;
    CHECK(!executeCall(page, DrawCall{3, {0x69CD, 10, 10, 0, 0}}, empty));   // no data: refused, nothing drawn
    CHECK(!executeCall(page, DrawCall{99, {}}, empty));
}

TEST(tickdraw_calls_and_pages_equal_the_oracle) {
    if (testfw::config().image.empty() || testfw::config().ticks.empty()) {
        std::printf("    SKIPPED (needs --image and --ticks)\n");
        return;
    }
    const auto list = scenarios();
    if (list.empty()) {
        std::printf("    SKIPPED (no ticks_X.bin with tick_calls_X.txt, pages_X.bin and extra_X.bin next to it)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPack(pack));
    GameState first;
    CHECK(newGameState(pack, first));
    for (const auto& sc : list) {
        std::string why;
        const auto tr = TickTrace::load(sc.ticksPath, why);
        CHECK_MSG(tr.ok(), why);
        if (!tr.ok()) continue;
        const auto logged = parseLog(sc.callsPath);
        const auto extras = readAll(sc.extraPath);
        const auto pagesFile = readAll(sc.pagesPath);
        CHECK(pagesFile.size() > 8 && std::memcmp(pagesFile.data(), "PAGS", 4) == 0);
        if (pagesFile.size() < 8) continue;
        std::uint32_t count = 0;
        std::memcpy(&count, pagesFile.data() + 4, 4);
        const std::size_t rec = 4 + hires::kPageBytes;
        CHECK(pagesFile.size() == 8 + count * rec);

        hires::Page page;
        CHECK(drawPlayfield(page, first, pack));
        CHECK(drawHud(page, first, pack));
        CHECK(drawPacman(page, first, pack, false));
        DisplayState display;
        std::size_t ticks = 0, withCalls = 0, calls = 0, badTicks = 0, pagesCompared = 0, badPages = 0, shown = 0;
        std::uint32_t nextPage = 0;
        // the recorder replays N ticks with the scenario's keys and pokes and runs on a little without them: only the first N ticks are valid (A, B, C: 700; D: 1600)
        const std::size_t scenarioLimit = sc.letter == "D" ? 1600 : sc.letter == "A" ? 2090 : sc.letter == "G" ? 1399 : sc.letter == "H" ? 499 : 700;
        const std::size_t limit = std::min<std::size_t>(scenarioLimit, std::min(logged.size(), tr.ticks()) - 1);
        for (std::size_t k = 0; k < limit; ++k) {
            // page comparison at the START of tick k (everything before it has been drawn)
            if (k % 10 == 0 && nextPage < count) {
                const std::uint8_t* rp = pagesFile.data() + 8 + nextPage * rec;
                std::uint32_t tick = 0;
                std::memcpy(&tick, rp, 4);
                if (tick == k) {
                    ++nextPage;
                    std::size_t diff = 0;
                    for (int y = 0; y < 192; ++y)
                        for (int c = 0; c < 40; ++c) {
                            const std::size_t off = hires::rowOffset(y) + static_cast<std::size_t>(c);
                            if (page.byte(off) != rp[4 + off]) {
                                if (shown < 6) {
                                    std::printf("    %s page at tick %zu: row %d column %d mine %02X oracle %02X\n", sc.letter.c_str(), k, y, c, page.byte(off), rp[4 + off]);
                                    ++shown;
                                }
                                ++diff;
                            }
                        }
                    ++pagesCompared;
                    badPages += diff != 0;
                }
            }
            const GameState a = fromOracleLayout(tr.state(k));
            const GameState b = fromOracleLayout(tr.state(k + 1));
            const TickExtras ea = extrasAt(extras, k), eb = extrasAt(extras, k + 1);
            const auto mine = tickCalls(a, b, pack, tr.at(k, 0x59) == 1, display, ea, eb);
            std::vector<DrawCall> compare;
            for (const auto& c : mine)
                if (c.type != 8 && c.type != 11 && c.type != 10 && c.type != 12) compare.push_back(c);
            const auto& theirs = logged[k];
            ++ticks;
            calls += theirs.size();
            withCalls += !theirs.empty();
            const bool caughtTick = ((b.lives() & 0x7F) < (a.lives() & 0x7F) && a.lives() != 0) || (b.level() > a.level() && a.dotsLeft() == 1);   // the death animation's own calls are not rebuilt (its result is, below)
            if (!caughtTick && !(compare == theirs)) {
                if (badTicks++ < 4) {
                    std::printf("    %s tick %zu: mine", sc.letter.c_str(), k);
                    for (const auto& c : compare) std::printf(" [%d %d %d %d]", c.type, c.p[0], c.p[1], c.p[2]);
                    std::printf(" | oracle");
                    for (const auto& c : theirs) std::printf(" [%d %d %d %d]", c.type, c.p[0], c.p[1], c.p[2]);
                    std::printf("\n");
                }
            }
            const bool levelUp = (b.level() > a.level() && a.dotsLeft() == 1);   // the level was completed: the original flashes the maze and builds the next level's first screen
            if (((b.lives() & 0x7F) < (a.lives() & 0x7F) && a.lives() != 0) || levelUp) {
                // Pac-Man was caught: the original plays the death animation and then builds the first screen again from the state (walls, door, dots of the CURRENT
                // map, labels, the score and high score, lives icons, Pac-Man at his start); the ghosts appear with the steps of the next ticks. The animation itself is
                // inside this tick and not rebuilt, only its result: the page is replaced.
                hires::Page fresh;
                CHECK(drawPlayfield(fresh, b, pack));
                CHECK(drawHud(fresh, b, pack, display.high));
                CHECK(drawPacman(fresh, b, pack, false));
                page = fresh;
                display.lastFlag1E = 0;
                continue;
            }
            for (const auto& c : mine) executeCall(page, c, pack);
        }
        std::printf("    scenario %s: %zu ticks, %zu with calls (%zu logged calls), %zu ticks differ; %zu pages compared, %zu differ\n", sc.letter.c_str(), ticks, withCalls, calls, badTicks,
                    pagesCompared, badPages);
        CHECK(badTicks == 0);
        CHECK(pagesCompared >= 50 && badPages == 0);
    }
}

TEST(tickdraw_death_and_level_end_sequences_equal_the_oracle_log) {
    if (testfw::config().image.empty() || testfw::config().ticks.empty()) {
        std::printf("    SKIPPED (needs --image and --ticks)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPack(pack));
    int checked = 0;
    for (const auto& sc : scenarios()) {
        std::string why;
        const auto tr = TickTrace::load(sc.ticksPath, why);
        if (!tr.ok()) continue;
        const auto logged = parseLog(sc.callsPath);
        const std::size_t limit = std::min(logged.size(), tr.ticks()) - 1;
        for (std::size_t k = 0; k < limit; ++k) {
            const GameState a = fromOracleLayout(tr.state(k));
            const GameState b = fromOracleLayout(tr.state(k + 1));
            const bool death = (b.lives() & 0x7F) < (a.lives() & 0x7F) && a.lives() != 0;
            const bool levelEnd = (b.level() > a.level() && a.dotsLeft() == 1);
            if (!death && !levelEnd) continue;
            const auto& block = logged[k];
            // the tail starts after the first "1UP label + mark" pair (death) or at the Pac-Man clear after it (level end)
            std::size_t start = 0;
            bool found = false;
            for (std::size_t i = 0; i + 1 < block.size(); ++i)
                if (block[i].type == 2 && block[i].p[0] == 0x55F7 && block[i + 1].type == 6 && block[i + 1].p[0] == 0x7201 && block[i + 1].p[1] == 47) {
                    start = i + 2;
                    found = true;
                    if (death) break;      // the first pair is the blink; for a level end the LAST pair before the dot pass counts (below)
                }
            if (!found) continue;
            RestartInputs in;
            if (death) {
                // positions and the last Pac-Man image are what the game core knows at that moment; here they are read from the first calls of the tail
                std::vector<DrawCall> tail(block.begin() + static_cast<std::ptrdiff_t>(start), block.end());
                for (int g = 0; g < 4; ++g) {                        // the tail begins with the clears of ghosts 4, 3, 2, 1
                    in.ghostXY[static_cast<std::size_t>(3 - g) * 2] = tail[static_cast<std::size_t>(g)].p[1] + 4;
                    in.ghostXY[static_cast<std::size_t>(3 - g) * 2 + 1] = tail[static_cast<std::size_t>(g)].p[2] + 4;
                }
                in.pacX = tail[4].p[1] + 4;
                in.pacY = tail[4].p[2] + 4;
                in.pacImage = tail[5].p[0];
            } else {
                // level end: the tail starts with the Pac-Man clear that follows the label pair; take the pair that precedes a type 3
                std::size_t s2 = 0;
                for (std::size_t i = 0; i + 2 < block.size(); ++i)
                    if (block[i].type == 6 && block[i].p[0] == 0x7201 && block[i].p[1] == 47 && block[i + 1].type == 3 && block[i + 2].type == 1) s2 = i + 1;
                if (s2 == 0) continue;
                start = s2;
                in.pacX = block[start].p[1] + 4;
                in.pacY = block[start].p[2] + 4;
                in.pacImage = block[start + 1].p[0];
            }
            DisplayState display;
            display.high = {b.zp(0x52), b.zp(0x53), b.zp(0x54)};   // the high score equals the score in these runs
            const auto mine = restartCalls(death ? RestartKind::Death : RestartKind::LevelEnd, in, b, pack, display);
            std::vector<DrawCall> theirs;
            for (std::size_t i = start; i < block.size(); ++i) theirs.push_back(block[i]);
            const bool same = mine == theirs;
            std::printf("    %s tick %zu (%s): %zu calls logged, %zu derived, %s\n", sc.letter.c_str(), k, death ? "death" : "level end", theirs.size(), mine.size(), same ? "EQUAL" : "DIFFERENT");
            if (!same) {
                for (std::size_t i = 0; i < std::min(mine.size(), theirs.size()); ++i)
                    if (!(mine[i] == theirs[i])) {
                        std::printf("      first difference at call %zu: mine [%d %d %d %d] oracle [%d %d %d %d]\n", i, mine[i].type, mine[i].p[0], mine[i].p[1], mine[i].p[2], theirs[i].type, theirs[i].p[0],
                                    theirs[i].p[1], theirs[i].p[2]);
                        break;
                    }
            }
            CHECK(same);
            ++checked;
        }
    }
    CHECK(checked >= 1);
}
