// Tests of the per-tick drawing (src/core/tickdraw): for each golden scenario whose files are present (convention, in the directory of the --ticks files):
//   ticks_X.bin (states), tick_calls_X.txt (the oracle's drawing calls per tick), pages_X.bin (the oracle's hi-res page every 10th tick),
//   extra_X.bin (6 bytes per tick: $E1 $E2 $E5 $E6 $E7 $E4 at the start of the tick), all from tools/record_tick_calls.py (local, git-ignored),
// 1. the calls derived from the states of the tick (sprites, score display, dot redraw, door, blinks) equal the oracle's logged calls (the door lines and the
//    erase calls of the blinks are not logged by the collector, so those are removed from the comparison), tick by tick, in order;
// 2. executing ALL derived calls on the first-screen page gives the oracle's page at every 10th tick, byte for byte (no area is excluded any more).
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "../src/core/lifestart.h"
#include "../src/core/screen.h"
#include "../src/core/cartoon.h"
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

// Two-player recordings (extra2_X.bin, 2 bytes per tick: $50 = the current player, $51 = the number of players). Without the file: one player.
struct PlayerInfo {
    std::vector<std::uint8_t> bytes;
    int current(std::size_t t) const { return 2 * t + 1 < bytes.size() ? bytes[2 * t] : 1; }
    int count(std::size_t t) const { return 2 * t + 1 < bytes.size() ? bytes[2 * t + 1] : 1; }
};
PlayerInfo loadPlayers(const std::string& extraPath) {
    std::string p = extraPath;
    const auto at = p.rfind("extra_");
    if (at != std::string::npos) p.replace(at, 6, "extra2_");
    return PlayerInfo{readAll(p)};
}
// The score of the player who is NOT current at tick t, as last seen while that player was current (the game keeps it outside the logical state).
std::uint32_t otherScoreAt(const PlayerInfo& pi, const TickTrace& tr, std::size_t t) {
    const int cur = pi.current(t);
    for (std::size_t u = t + 1; u-- > 0;)
        if (pi.current(u) != cur) {
            const GameState g = fromOracleLayout(tr.state(u));
            return g.zp(0x52) | (g.zp(0x53) << 8) | (g.zp(0x54) << 16);
        }
    return 0;
}

// 3-5 players: every player's score as last seen while he was the current player (index = player; a player who has not played yet has 0).
std::array<std::uint32_t, 6> scoresAt(const PlayerInfo& pi, const TickTrace& tr, std::size_t t) {
    // A player's score is final when his turn ends; the state of the tick in which he dies is already the next player's, so a gain in that very tick (a dot eaten) is not in any recorded state
    // before the switch. It is in the state at the START of his next turn (the score does not change between), so that one is used when the player comes round again.
    std::array<std::uint32_t, 6> out{};
    std::array<bool, 6> have{};
    const auto scoreAt = [&](std::size_t u) {
        const GameState g = fromOracleLayout(tr.state(u));
        return static_cast<std::uint32_t>(g.zp(0x52) | (g.zp(0x53) << 8) | (g.zp(0x54) << 16));
    };
    const int now = pi.current(t);
    for (std::size_t u = t + 1; u < tr.ticks(); ++u) {
        const int cur = pi.current(u);
        if (cur < 1 || cur > 5 || cur == now || have[static_cast<std::size_t>(cur)]) continue;
        out[static_cast<std::size_t>(cur)] = scoreAt(u);
        have[static_cast<std::size_t>(cur)] = true;
    }
    for (std::size_t u = t + 1; u-- > 0;) {
        const int cur = pi.current(u);
        if (cur < 1 || cur > 5 || have[static_cast<std::size_t>(cur)]) continue;
        out[static_cast<std::size_t>(cur)] = scoreAt(u);
        have[static_cast<std::size_t>(cur)] = true;
    }
    return out;
}

// The tile that marks a player's label (the digit 1..5: $7201 + $70 x (player - 1)) at the column of the left (x 47: players 1, 3, 5) or the right (x 271: players 2, 4) block.
bool isPlayerMark(const DrawCall& c) {
    if (c.type != 6 || (c.p[1] != 47 && c.p[1] != 271)) return false;
    for (int i = 0; i < 5; ++i)
        if (c.p[0] == 0x7201 + 0x70 * i) return true;
    return false;
}

// Pac-Man was caught: the lives went down. A poke of the lives byte ($58) in the recording (scenario Q) is not a catch.
bool livesDropped(const TickTrace& tr, std::size_t k, const GameState& a, const GameState& b) {
    for (const auto& pk : tr.pokes())
        if (pk.address == 0x58 && (pk.tick == k || pk.tick == k + 1)) return false;
    return (b.lives() & 0x7F) < (a.lives() & 0x7F) && a.lives() != 0;
}

struct Scenario {
    std::string letter, ticksPath, callsPath, pagesPath, extraPath;
};

// ticks_X.bin -> the sibling files of scenario X (only A and D have been recorded)
std::vector<Scenario> scenarios() {
    std::vector<Scenario> out;
    for (const auto& p : split(testfw::config().drawticks.empty() ? testfw::config().ticks : testfw::config().drawticks)) {
        const std::size_t slash = p.find_last_of("/\\");
        const std::string dir = slash == std::string::npos ? std::string() : p.substr(0, slash + 1);
        const std::string name = p.substr(slash == std::string::npos ? 0 : slash + 1);
        if (name.size() < 11 || name.rfind("ticks_", 0) != 0 || name.compare(name.size() - 4, 4, ".bin") != 0) continue;   // ticks_X.bin (X: one letter, or a name such as Z08 or T3)
        const std::string id = name.substr(6, name.size() - 10);
        Scenario s{id, p, dir + "tick_calls_" + id + ".txt", dir + "pages_" + id + ".bin", dir + "extra_" + id + ".bin"};
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
    if (testfw::config().image.empty() || (testfw::config().ticks.empty() && testfw::config().drawticks.empty())) {
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
        const PlayerInfo pinfo = loadPlayers(sc.extraPath);
        const auto pagesFile = readAll(sc.pagesPath);
        CHECK(pagesFile.size() > 8 && std::memcmp(pagesFile.data(), "PAGS", 4) == 0);
        if (pagesFile.size() < 8) continue;
        std::uint32_t count = 0;
        std::memcpy(&count, pagesFile.data() + 4, 4);
        const std::size_t rec = 4 + hires::kPageBytes;
        CHECK(pagesFile.size() == 8 + count * rec);

        hires::Page page;
        CHECK(drawPlayfield(page, first, pack));
        CHECK(drawHud(page, first, pack, {0, 0, 0}, pinfo.count(0), 0, 1));
        CHECK(drawPacman(page, first, pack, false));
        DisplayState display;
        std::size_t ticks = 0, withCalls = 0, calls = 0, badTicks = 0, pagesCompared = 0, badPages = 0, shown = 0, doorCalls = 0, doorLowBit = 0;
        std::uint32_t nextPage = 0;
        // the recorder replays N ticks with the scenario's keys and pokes and runs on a little without them: only the first N ticks are valid (A, B, C: 700; D: 1600)
        const std::size_t scenarioLimit = sc.letter == "D" ? 1600 : sc.letter == "A" ? 2090 : sc.letter == "G" ? 1399 : sc.letter == "H" ? 499 : sc.letter == "P" ? 2299 : sc.letter == "Q" ? 4699 : sc.letter == "R" ? 5574 : sc.letter == "S" ? 6999 : sc.letter == "U" ? 2599 : sc.letter.size() > 1 ? 100000 : 700;   // (the recordings with a longer name - Z08.., T3, T5 - are recorded to their end)
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
            TickExtras ea = extrasAt(extras, k), eb = extrasAt(extras, k + 1);
            ea.player = pinfo.current(k);
            eb.player = pinfo.current(k + 1);
            const auto mine = tickCalls(a, b, pack, tr.at(k, 0x59) == 1, display, ea, eb);
            std::vector<DrawCall> compare;
            for (const auto& c : mine)
                if (c.type != 8 && c.type != 11 && c.type != 10 && c.type != 12) compare.push_back(c);
            const auto& theirs = logged[k];
            ++ticks;
            calls += theirs.size();
            withCalls += !theirs.empty();
            for (const auto& c : mine)   // coverage of the door redraw (type 8) and of its low-bit OR ($1E with bits 0-6 set)
                if (c.type == 8) {
                    ++doorCalls;
                    doorLowBit += (c.p[4] & 0x7F) != 0;
                }
            const bool switched = pinfo.current(k + 1) != pinfo.current(k);
            const bool caughtTick = livesDropped(tr, k, a, b) || switched || (b.level() > a.level() && a.dotsLeft() == 1);   // the death animation's own calls are not rebuilt (its result is, below)
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
            if (livesDropped(tr, k, a, b) || switched || levelUp) {
                // Pac-Man was caught: the original plays the death animation and then builds the first screen again from the state (walls, door, dots of the CURRENT
                // map, labels, the score and high score, lives icons, Pac-Man at his start); the ghosts appear with the steps of the next ticks. The animation itself is
                // inside this tick and not rebuilt, only its result: the page is replaced.
                {   // the high score shown is the best of all the players' scores (the dying player's last gain is in his next turn's first state)
                    std::uint32_t best = b.zp(0x52) | (b.zp(0x53) << 8) | (b.zp(0x54) << 16);
                    if (pinfo.count(k + 1) > 2) {
                        for (const std::uint32_t sc2 : scoresAt(pinfo, tr, k + 1)) best = std::max(best, sc2);
                    } else {
                        best = std::max(best, otherScoreAt(pinfo, tr, k + 1));
                    }
                    const std::uint32_t old = display.high[0] | (display.high[1] << 8) | (display.high[2] << 16);
                    best = std::max(best, old);
                    display.high = {static_cast<std::uint8_t>(best), static_cast<std::uint8_t>(best >> 8), static_cast<std::uint8_t>(best >> 16)};
                }
                hires::Page fresh;
                CHECK(drawPlayfield(fresh, b, pack));
                if (pinfo.count(k + 1) > 2)   // 3-5 players: all the blocks, with every player's score
                    CHECK(drawHudPlayers(fresh, b, pack, display.high, pinfo.count(k + 1), pinfo.current(k + 1), scoresAt(pinfo, tr, k + 1)));
                else
                    CHECK(drawHud(fresh, b, pack, display.high, pinfo.count(k + 1), otherScoreAt(pinfo, tr, k + 1), pinfo.current(k + 1)));
                CHECK(drawPacman(fresh, b, pack, false));
                page = fresh;
                display.lastFlag1E = 0;
                continue;
            }
            for (const auto& c : mine) executeCall(page, c, pack);
        }
        std::printf("    scenario %s: %zu ticks, %zu with calls (%zu logged calls), %zu ticks differ; %zu pages compared, %zu differ; door redraws %zu (%zu with the low-bit OR)\n", sc.letter.c_str(), ticks, withCalls, calls, badTicks,
                    pagesCompared, badPages, doorCalls, doorLowBit);
        CHECK(badTicks == 0);
        CHECK(pagesCompared >= (sc.letter.size() > 1 ? 10u : 50u) && badPages == 0);
        if (sc.letter == "K") CHECK_MSG(doorLowBit >= 1, "scenario K must redraw the door with the previous draw's low palette bit ($1E = 1): checked by changing the code (the page then differs at tick 210)");
    }
}

TEST(tickdraw_death_and_level_end_sequences_equal_the_oracle_log) {
    if (testfw::config().image.empty() || (testfw::config().ticks.empty() && testfw::config().drawticks.empty())) {
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
        const PlayerInfo pinfo = loadPlayers(sc.extraPath);
        const std::size_t limit = std::min(logged.size(), tr.ticks()) - 1;
        for (std::size_t k = 0; k < limit; ++k) {
            const GameState a = fromOracleLayout(tr.state(k));
            const GameState b = fromOracleLayout(tr.state(k + 1));
            const bool death = livesDropped(tr, k, a, b) || pinfo.current(k + 1) != pinfo.current(k);
            const bool levelEnd = (b.level() > a.level() && a.dotsLeft() == 1);
            if (!death && !levelEnd) continue;
            const auto& block = logged[k];
            // the tail starts after the first "1UP label + mark" pair (death) or at the Pac-Man clear after it (level end)
            std::size_t start = 0;
            bool found = false;
            for (std::size_t i = 0; i + 1 < block.size(); ++i)
                if (block[i].type == 2 && block[i].p[0] == 0x55F7 && isPlayerMark(block[i + 1])) {
                    start = i + 2;
                    found = true;
                    if (death) break;      // the first pair is the blink; for a level end the LAST pair before the dot pass counts (below)
                }
            if (!found) continue;
            RestartInputs in;
            in.players = pinfo.count(k + 1);
            in.current = pinfo.current(k + 1);
            in.otherScore = otherScoreAt(pinfo, tr, k + 1);
            std::array<std::uint32_t, 6> allScores{};
            if (in.players > 2) {   // 3-5 players: the labels and scores of every player (TASK-033-6b)
                allScores = scoresAt(pinfo, tr, k + 1);
                in.scores = &allScores;
            }
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
                    if (isPlayerMark(block[i]) && block[i + 1].type == 3 && block[i + 2].type == 1) s2 = i + 1;
                if (s2 == 0) continue;
                start = s2;
                in.pacX = block[start].p[1] + 4;
                in.pacY = block[start].p[2] + 4;
                in.pacImage = block[start + 1].p[0];
            }
            DisplayState display;
            if (death) {   // the death animation is drawn with the $1E that the last image draw before it left (a ghost's table flag)
                for (std::size_t i = start; i-- > 0;)
                    if (block[i].type == 1) {
                        display.lastFlag1E = block[i].p[4];
                        break;
                    }
            }
            {   // the high score is the best score of the runs (in two-player games the other player's score may be the higher one)
                const std::uint32_t mineScore = b.zp(0x52) | (b.zp(0x53) << 8) | (b.zp(0x54) << 16);
                std::uint32_t best = std::max(mineScore, in.otherScore);
                if (in.players > 2)
                    for (const std::uint32_t sc2 : allScores) best = std::max(best, sc2);
                display.high = {static_cast<std::uint8_t>(best), static_cast<std::uint8_t>(best >> 8), static_cast<std::uint8_t>(best >> 16)};
            }
            const auto mine = restartCalls(death ? RestartKind::Death : RestartKind::LevelEnd, in, b, pack, display);
            std::vector<DrawCall> theirs;
            for (std::size_t i = start; i < block.size(); ++i) theirs.push_back(block[i]);
            bool same = mine == theirs;
            bool cartoonInside = false;
            if (!same && levelEnd && !death && cartoonAfterLevel(a.level()) != 0 && theirs.size() > mine.size()) {
                // after this level the intermission plays inside the tick: the calls of the level end before it and the rebuilt screen after it are mine, the cartoon between them is checked on its own (test_cartoon)
                std::size_t prefix = 0, suffix = 0;
                while (prefix < mine.size() && mine[prefix] == theirs[prefix]) ++prefix;
                while (suffix < mine.size() - prefix && mine[mine.size() - 1 - suffix] == theirs[theirs.size() - 1 - suffix]) ++suffix;
                same = prefix + suffix >= mine.size();
                cartoonInside = same;
            }
            std::printf("    %s tick %zu (%s): %zu calls logged, %zu derived, %s\n", sc.letter.c_str(), k, death ? "death" : "level end", theirs.size(), mine.size(), same ? (cartoonInside ? "EQUAL around the intermission" : "EQUAL") : "DIFFERENT");
            if (!same) {
                for (std::size_t i = 0; i < std::min(mine.size(), theirs.size()); ++i)
                    if (!(mine[i] == theirs[i])) {
                        std::printf("      first difference at call %zu: mine [%d %d %d %d %d %d] oracle [%d %d %d %d %d %d]\n", i, mine[i].type, mine[i].p[0], mine[i].p[1], mine[i].p[2], mine[i].p[3], mine[i].p[4], theirs[i].type, theirs[i].p[0],
                                    theirs[i].p[1], theirs[i].p[2], theirs[i].p[3], theirs[i].p[4]);
                        for (std::size_t j = i > 4 ? i - 4 : 0; j < std::min(i + 5, std::min(mine.size(), theirs.size())); ++j)
                            std::printf("        call %zu: mine [%d %d %d %d %d %d] oracle [%d %d %d %d %d %d]\n", j, mine[j].type, mine[j].p[0], mine[j].p[1], mine[j].p[2], mine[j].p[3], mine[j].p[4], theirs[j].type, theirs[j].p[0], theirs[j].p[1], theirs[j].p[2], theirs[j].p[3], theirs[j].p[4]);
                        break;
                    }
            }
            CHECK(same);
            ++checked;
        }
        // the last recorded tick may be the death of a player's LAST life: the death head, then the text GAME OVER (no state follows it, so it is checked here)
        if (!logged.empty() && logged.size() == tr.ticks()) {
            const auto& block = logged.back();
            bool hasText = false;
            for (const auto& c : block) hasText = hasText || (c.type == 2 && c.p[0] == 0x811D);
            std::size_t start = 0;
            for (std::size_t i = 0; hasText && i + 1 < block.size(); ++i)
                if (block[i].type == 2 && block[i].p[0] == 0x55F7 && block[i + 1].type == 6) {
                    start = i + 2;
                    break;
                }
            if (hasText && start != 0 && block.size() >= start + 6) {
                RestartInputs in;
                for (int g = 0; g < 4; ++g) {
                    in.ghostXY[static_cast<std::size_t>(3 - g) * 2] = block[start + static_cast<std::size_t>(g)].p[1] + 4;
                    in.ghostXY[static_cast<std::size_t>(3 - g) * 2 + 1] = block[start + static_cast<std::size_t>(g)].p[2] + 4;
                }
                in.pacX = block[start + 4].p[1] + 4;
                in.pacY = block[start + 4].p[2] + 4;
                in.pacImage = block[start + 5].p[0];
                DisplayState display;
                for (std::size_t i = start; i-- > 0;)   // the death is drawn with the $1E that the last image draw of the tick left
                    if (block[i].type == 1) {
                        display.lastFlag1E = block[i].p[4];
                        break;
                    }
                const GameState last = fromOracleLayout(tr.state(tr.ticks() - 1));
                const auto mine = restartCalls(RestartKind::GameOver, in, last, pack, display);
                const std::vector<DrawCall> theirs(block.begin() + static_cast<std::ptrdiff_t>(start), block.end());
                const bool same = mine == theirs;
                std::printf("    %s tick %zu (last life): %zu calls logged, %zu derived, %s\n", sc.letter.c_str(), logged.size() - 1, theirs.size(), mine.size(), same ? "EQUAL" : "DIFFERENT");
                CHECK(same);
                hires::Page probe;
                for (const auto& c : mine) CHECK(executeCall(probe, c, pack));
                ++checked;
            }
        }
    }
    CHECK(checked >= 1);
}
