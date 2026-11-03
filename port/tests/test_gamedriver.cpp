// TASK-029h end to end: GameDriver (core + drawing) against the oracle's hi-res pages. One-player recordings that have pages_X.bin and extra_X.bin next to
// ticks_X.bin (tools/record_tick_calls.py). The ghosts are FED from the oracle (ghost logic is M5) together with the frightened timers $E1/$E2/$E4; everything
// else - Pac-Man, eating, scores, lives, fruit, blinks, the whole picture - comes from the C++ code. Catches and level ends replace the page and the state by the
// oracle's (the next level's set-up is M6); the page is compared at every 10th tick.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <map>
#include <string>
#include <vector>

#include "../src/core/deathtiming.h"
#include "../src/core/gamedriver.h"
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

class FedGhosts : public FrightenAware {
public:
    const GameState* before = nullptr;
    const GameState* next = nullptr;
    bool caughtTick = false;
    std::uint8_t f1 = 0, f2 = 0, f4 = 0;
    std::array<bool, 4> step(GameState& s, bool) override {
        std::array<bool, 4> moved{};
        for (std::size_t g = 0; g < 4; ++g) moved[g] = before->ghosts[g].b[0] != next->ghosts[g].b[0] || before->ghosts[g].b[1] != next->ghosts[g].b[1];
        s.ghosts = next->ghosts;
        for (std::size_t g = 0; g < 4; ++g) {   // a ghost turning into eyes is eaten by the core's contact test: hand it over frightened
            const auto& b0 = before->ghosts[g].b;
            auto& b1 = s.ghosts[g].b;
            if (b0[9] == 0 && b1[9] == 1 && b0[8] != 0 && b1[8] == 0) {
                b1[8] = b0[8];
                b1[9] = 0;
            }
        }
        if (caughtTick) {   // positions at the moment of the catch are not in the recording: offer the start-of-tick place and one step further
            for (std::size_t g = 0; g < 4; ++g) {
                GhostRecord cand = before->ghosts[g], moved1 = cand;
                static constexpr int dx[4] = {0, 0, 2, -2}, dy[4] = {2, -2, 0, 0};
                moved1.b[0] = static_cast<std::uint8_t>(cand.b[0] + dx[cand.b[2] & 3]);
                moved1.b[1] = static_cast<std::uint8_t>(cand.b[1] + dy[cand.b[2] & 3]);
                s.ghosts[g] = cand;
                if (ghostContact(s, cand) != Contact::Touch && ghostContact(s, moved1) == Contact::Touch) s.ghosts[g] = moved1;
            }
            return {true, true, true, true};
        }
        s.mode78 = next->mode78;
        s.mode79 = next->mode79;
        if (next->cornerPhase == 0 && s.cornerPhase != 0) s.zp(0x68) = 2;
        s.cornerPhase = next->cornerPhase;
        return moved;
    }
    void frightenState(std::uint8_t& e1, std::uint8_t& e2, std::uint8_t& e4) const override {
        e1 = f1;
        e2 = f2;
        e4 = f4;
    }
};

}  // namespace

TEST(gamedriver_pages_equal_the_oracle_end_to_end) {
    const std::string& list = testfw::config().drawticks.empty() ? testfw::config().ticks : testfw::config().drawticks;
    if (list.empty() || testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image and --ticks / --drawticks)\n");
        return;
    }
    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    std::size_t scenarios = 0;
    for (const auto& path : split(list)) {
        const std::size_t slash = path.find_last_of("/\\");
        const std::string dir = slash == std::string::npos ? "" : path.substr(0, slash + 1);
        const std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
        if (name.size() < 11 || name.compare(0, 6, "ticks_") != 0) continue;
        const std::string letter = name.substr(6, name.size() - 10);
        const auto pagesFile = readAll(dir + "pages_" + letter + ".bin");
        const auto extras = readAll(dir + "extra_" + letter + ".bin");
        const auto players = readAll(dir + "extra2_" + letter + ".bin");
        if (pagesFile.size() < 8 || extras.empty()) continue;
        bool onePlayer = true;
        for (std::size_t i = 1; i < players.size(); i += 2) onePlayer = onePlayer && players[i] <= 1;
        if (!onePlayer) continue;
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), why);
        if (!tr.ok()) continue;
        std::uint32_t count = 0;
        std::memcpy(&count, pagesFile.data() + 4, 4);
        const std::size_t rec = 4 + hires::kPageBytes;
        CHECK(pagesFile.size() == 8 + count * rec);
        GameDriver drv;
        CHECK(drv.init(loaded.pack));
        CHECK(drv.newGame());
        drv.core().state() = fromOracleLayout(tr.state(0));
        drv.rebuildScreen();
        std::multimap<std::uint32_t, std::uint32_t> keysAt;
        for (const auto& in : tr.inputs()) keysAt.emplace(in.tick, in.key);
        std::multimap<std::uint32_t, TickPoke> pokesAt;
        for (const auto& p : tr.pokes()) pokesAt.emplace(p.tick, p);
        const auto logged = parseLog(dir + "tick_calls_" + letter + ".txt");
        // the call logs, pages and extras cover only the first ticks of a recording (A 2,090, D 1,600, ...)
        // the recorded range of each scenario, as in the drawing test (A 2,090, D 1,093 here - from tick 1094 the poked position of Pac-Man leaves the map -, G 1,399, H 499, the others 700; beyond it the call logs and pages are not usable)
        const std::size_t recorded = letter == "D" ? 1093 : letter == "A" ? 2090 : letter == "G" ? 1399 : letter == "H" ? 499 : 700;
        const std::size_t limit = std::min<std::size_t>({tr.ticks() - 1, extras.size() / 6 - 1, logged.empty() ? tr.ticks() : logged.size() - 1, recorded});
        FedGhosts feed;
        std::size_t badCalls = 0;
        std::size_t deathSeqChecked = 0, deathSeqBad = 0;
        std::size_t nextPage = 0, compared = 0, badPages = 0, resyncs = 0, shown = 0;
        for (std::size_t k = 0; k < limit; ++k) {
            // the page at the START of tick k
            while (nextPage < count) {
                const std::uint8_t* rp = pagesFile.data() + 8 + nextPage * rec;
                std::uint32_t tick = 0;
                std::memcpy(&tick, rp, 4);
                if (tick > k) break;
                if (tick == k) {
                    std::size_t diff = 0;
                    for (int y = 0; y < 192; ++y)
                        for (int c = 0; c < 40; ++c) {
                            const std::size_t off = hires::rowOffset(y) + static_cast<std::size_t>(c);
                            if (drv.page().byte(off) != rp[4 + off]) {
                                if (shown++ < 4) std::printf("    %s page at tick %zu: row %d column %d mine %02X oracle %02X\n", letter.c_str(), k, y, c, drv.page().byte(off), rp[4 + off]);
                                ++diff;
                            }
                        }
                    ++compared;
                    badPages += diff != 0;
                }
                ++nextPage;
            }
            const auto kr = keysAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = kr.first; it != kr.second; ++it) drv.core().input().pushKey(static_cast<std::uint8_t>(it->second));
            GameState& s = drv.core().state();
            const auto pr = pokesAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = pr.first; it != pr.second; ++it) {
                const auto a = it->second.address;
                if (a >= 0x52 && a <= 0x6F) s.zp(static_cast<std::uint16_t>(a)) = static_cast<std::uint8_t>(it->second.value);
                if (a >= 0x623A && a <= 0x65B9) s.map[a - 0x623A] = static_cast<std::uint8_t>(it->second.value);
                if (a >= 0x84 && a <= 0xD3) s.ghosts[(a - 0x84) / 20].b[(a - 0x84) % 20] = static_cast<std::uint8_t>(it->second.value);   // ghost records (the timer reads their flags)
                if (a == 0x78) s.mode78 = static_cast<std::uint8_t>(it->second.value);
                if (a == 0x79) s.mode79 = static_cast<std::uint8_t>(it->second.value);
                if (a == 0xE9) s.cornerPhase = static_cast<std::uint8_t>(it->second.value);
            }
            const GameState a = fromOracleLayout(tr.state(k));
            const GameState b = fromOracleLayout(tr.state(k + 1));
            feed.before = &a;
            feed.next = &b;
            feed.caughtTick = (b.lives() & 0x7F) < (a.lives() & 0x7F) && a.lives() != 0;
            drv.setPreviousFrighten(extras[6 * k], extras[6 * k + 1], extras[6 * k + 5]);
            feed.f1 = extras[6 * (k + 1)];
            feed.f2 = extras[6 * (k + 1) + 1];
            feed.f4 = extras[6 * (k + 1) + 5];
            if (feed.caughtTick && (feed.next->lives() & 0x7F) > 0 && !logged.empty() && k < logged.size()) {   // TASK-033-3: the death animation's calls (from the state BEFORE the catch) equal the oracle's log
                GameDriver copy = drv;
                copy.setPlayDeath(true);
                copy.tick(feed);
                const auto& block = logged[k];
                std::size_t start = 0;
                for (std::size_t i = 0; i + 1 < block.size() && start == 0; ++i)
                    if (block[i].type == 2 && block[i].p[0] == 0x55F7 && block[i + 1].type == 6 && block[i + 1].p[0] == 0x7201 && block[i + 1].p[1] == 47) start = i + 2;
                std::vector<DrawCall> mine;
                std::int64_t last = -1;
                bool ascending = true;
                for (const auto& st : copy.sequence()) {
                    for (const auto& c : st.calls) mine.push_back(c);
                    ascending = ascending && st.at > last;
                    last = st.at;
                }
                ++deathSeqChecked;
                if (start == 0 || mine.size() != 19 || !ascending || copy.sequence().size() != 22) {
                    ++deathSeqBad;
                    std::printf("    %s tick %zu: death sequence has %zu calls in %zu steps (ascending %d), tail found %d\n", letter.c_str(), k, mine.size(), copy.sequence().size(), ascending ? 1 : 0, start != 0 ? 1 : 0);
                } else {
                    std::size_t diff = 0;
                    for (std::size_t i = 0; i < 19; ++i)
                        // types and addresses only: this test feeds recorded ghosts that do not move inside the tick, so the POSITIONS of the clears are checked in the free-running
                        // test (test_ghostworld: ghosts and Pac-Man at the catch equal the oracle's, exactly); the frame addresses and the structure are checked here
                        if (mine[i].type != block[start + i].type || mine[i].p[0] != block[start + i].p[0]) {
                            if (diff++ == 0) std::printf("    %s tick %zu: death call %zu mine [%d %d %d %d] oracle [%d %d %d %d]\n", letter.c_str(), k, i, mine[i].type, mine[i].p[0], mine[i].p[1], mine[i].p[2], block[start + i].type, block[start + i].p[0], block[start + i].p[1], block[start + i].p[2]);
                        }
                    deathSeqBad += diff != 0;
                }
            }
            const TickEvents ev = drv.tick(feed);
            if (!logged.empty() && k < logged.size() && !ev.caught && !ev.levelComplete) {   // the call lists, as in the drawing test (types the collector does not log are left out)
                std::vector<DrawCall> mine;
                for (const auto& c : drv.lastCalls())
                    if (c.type != 8 && c.type != 10 && c.type != 11 && c.type != 12) mine.push_back(c);
                const bool restartTick = (b.lives() & 0x7F) < (a.lives() & 0x7F) || (b.level() > a.level() && a.dotsLeft() == 1);
                if (!restartTick && !(mine == logged[k])) {
                    if (badCalls++ < 3) {
                        std::printf("    %s tick %zu: pac before %02X dir %u req %u, mine after %02X dir %u, oracle after %02X dir %u; mine", letter.c_str(), k, a.pacX(), a.pacDirection(), a.zp(0x63), drv.core().state().pacX(), drv.core().state().pacDirection(), b.pacX(), b.pacDirection());
                        for (const auto& c : mine) std::printf(" [%d %d %d %d]", c.type, c.p[0], c.p[1], c.p[2]);
                        std::printf(" | oracle");
                        for (const auto& c : logged[k]) std::printf(" [%d %d %d %d]", c.type, c.p[0], c.p[1], c.p[2]);
                        std::printf("\n");
                    }
                }
            }
            bool pokedLives = false;
            for (auto it = pr.first; it != pr.second; ++it) pokedLives = pokedLives || it->second.address == 0x58 || it->second.address == 0x55;
            const bool oracleRestart = !pokedLives && ((b.lives() & 0x7F) < (a.lives() & 0x7F) || (b.level() > a.level() && a.dotsLeft() == 1));
            const bool restart = ev.caught || ev.levelComplete || ev.outsideMap || oracleRestart;
            if (restart) {
                const bool outside = ev.outsideMap && b.dotsLeft() < a.dotsLeft();
                drv.core().state() = b;
                if (outside) drv.core().itemEatenOutsideTheMap();
                drv.rebuildScreen();
                ++resyncs;
                if (false) std::printf("    %s resync at tick %zu: caught %d level %d outside %d oracle %d\n", letter.c_str(), k, ev.caught, ev.levelComplete, ev.outsideMap, oracleRestart);
            }
        }
        std::printf("    scenario %s: %zu ticks, %zu pages compared, %zu differ (%zu catches / level ends / poke artefacts resynchronised)\n", letter.c_str(), limit, compared, badPages, resyncs);
        CHECK(compared >= 20);
        CHECK(badPages == 0);
        std::printf("    death animation calls checked at %zu catches, %zu differ\n", deathSeqChecked, deathSeqBad);
        CHECK(deathSeqBad == 0);
        ++scenarios;
    }
    CHECK(scenarios >= 1);
}

TEST(gamedriver_free_running_pages_equal_the_oracle_with_its_own_ghosts) {
    const std::string& list = testfw::config().drawticks.empty() ? testfw::config().ticks : testfw::config().drawticks;
    if (list.empty() || testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image and --ticks / --drawticks)\n");
        return;
    }
    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    std::size_t scenarios = 0;
    for (const auto& path : split(list)) {
        const std::size_t slash = path.find_last_of("/\\");
        const std::string dir = slash == std::string::npos ? "" : path.substr(0, slash + 1);
        const std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
        if (name.size() < 11 || name.compare(0, 6, "ticks_") != 0) continue;
        const std::string letter = name.substr(6, name.size() - 10);
        const auto pagesFile = readAll(dir + "pages_" + letter + ".bin");
        const auto extras = readAll(dir + "extra_" + letter + ".bin");
        const auto players = readAll(dir + "extra2_" + letter + ".bin");
        if (pagesFile.size() < 8 || extras.empty()) continue;
        const auto ram = readAll(dir + "ghostram_" + letter + ".bin");
        if (ram.size() < 12 + 1280 || std::memcmp(ram.data(), "GRAM", 4) != 0) continue;
        bool onePlayer = true;
        for (std::size_t i = 1; i < players.size(); i += 2) onePlayer = onePlayer && players[i] <= 1;
        if (!onePlayer) continue;
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), why);
        if (!tr.ok()) continue;
        std::uint32_t count = 0;
        std::memcpy(&count, pagesFile.data() + 4, 4);
        const std::size_t rec = 4 + hires::kPageBytes;
        CHECK(pagesFile.size() == 8 + count * rec);
        GameDriver drv;
        CHECK(drv.init(loaded.pack));
        CHECK(drv.newGame());
        drv.core().state() = fromOracleLayout(tr.state(0));
        auto loadShared = [&](const std::uint8_t* z) {
            GhostShared& sh = drv.core().ghostShared();
            sh.e1 = z[0xE1];
            sh.e2 = z[0xE2];
            sh.e4 = z[0xE4];
            sh.ea = z[0xEA];
            sh.eb = z[0xEB];
            for (std::size_t g = 0; g < 4; ++g) std::copy(z + 256 + 256 * g, z + 512 + 256 * g, drv.core().ghostQueues()[g].begin());
            drv.syncExtras();
        };
        loadShared(ram.data() + 12);
        drv.rebuildScreen();
        std::multimap<std::uint32_t, std::uint32_t> keysAt;
        for (const auto& in : tr.inputs()) keysAt.emplace(in.tick, in.key);
        std::multimap<std::uint32_t, TickPoke> pokesAt;
        for (const auto& p : tr.pokes()) pokesAt.emplace(p.tick, p);
        const auto logged = parseLog(dir + "tick_calls_" + letter + ".txt");
        // the call logs, pages and extras cover only the first ticks of a recording (A 2,090, D 1,600, ...)
        // the recorded range of each scenario, as in the drawing test (A 2,090, D 1,093 here - from tick 1094 the poked position of Pac-Man leaves the map -, G 1,399, H 499, the others 700; beyond it the call logs and pages are not usable)
        const std::size_t recorded = letter == "D" ? 1093 : letter == "A" ? 2090 : letter == "G" ? 1399 : letter == "H" ? 499 : 700;
        const std::size_t limit = std::min<std::size_t>({tr.ticks() - 1, extras.size() / 6 - 1, logged.empty() ? tr.ticks() : logged.size() - 1, recorded});
        std::size_t badCalls = 0;
        std::size_t nextPage = 0, compared = 0, badPages = 0, resyncs = 0, shown = 0;
        for (std::size_t k = 0; k < limit; ++k) {
            // the page at the START of tick k
            while (nextPage < count) {
                const std::uint8_t* rp = pagesFile.data() + 8 + nextPage * rec;
                std::uint32_t tick = 0;
                std::memcpy(&tick, rp, 4);
                if (tick > k) break;
                if (tick == k) {
                    std::size_t diff = 0;
                    for (int y = 0; y < 192; ++y)
                        for (int c = 0; c < 40; ++c) {
                            const std::size_t off = hires::rowOffset(y) + static_cast<std::size_t>(c);
                            if (drv.page().byte(off) != rp[4 + off]) {
                                if (shown++ < 4) std::printf("    %s page at tick %zu: row %d column %d mine %02X oracle %02X\n", letter.c_str(), k, y, c, drv.page().byte(off), rp[4 + off]);
                                ++diff;
                            }
                        }
                    ++compared;
                    badPages += diff != 0;
                }
                ++nextPage;
            }
            const auto kr = keysAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = kr.first; it != kr.second; ++it) drv.core().input().pushKey(static_cast<std::uint8_t>(it->second));
            GameState& s = drv.core().state();
            const auto pr = pokesAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = pr.first; it != pr.second; ++it) {
                const auto a = it->second.address;
                if (a >= 0x52 && a <= 0x6F) s.zp(static_cast<std::uint16_t>(a)) = static_cast<std::uint8_t>(it->second.value);
                if (a >= 0x623A && a <= 0x65B9) s.map[a - 0x623A] = static_cast<std::uint8_t>(it->second.value);
                if (a == 0xE1) drv.core().ghostShared().e1 = static_cast<std::uint8_t>(it->second.value);
                if (a == 0xE2) drv.core().ghostShared().e2 = static_cast<std::uint8_t>(it->second.value);
                if (a == 0xE4) drv.core().ghostShared().e4 = static_cast<std::uint8_t>(it->second.value);
                if (a == 0xEA) drv.core().ghostShared().ea = static_cast<std::uint8_t>(it->second.value);
                if (a == 0xEB) drv.core().ghostShared().eb = static_cast<std::uint8_t>(it->second.value);
                if (a >= 0x84 && a <= 0xD3) s.ghosts[(a - 0x84) / 20].b[(a - 0x84) % 20] = static_cast<std::uint8_t>(it->second.value);   // ghost records (the timer reads their flags)
                if (a == 0x78) s.mode78 = static_cast<std::uint8_t>(it->second.value);
                if (a == 0x79) s.mode79 = static_cast<std::uint8_t>(it->second.value);
                if (a == 0xE9) s.cornerPhase = static_cast<std::uint8_t>(it->second.value);
            }
            const GameState a = fromOracleLayout(tr.state(k));
            const GameState b = fromOracleLayout(tr.state(k + 1));
            drv.syncExtras();
            const TickEvents ev = drv.tick();
            if (!logged.empty() && k < logged.size() && !ev.caught && !ev.levelComplete) {   // the call lists, as in the drawing test (types the collector does not log are left out)
                std::vector<DrawCall> mine;
                for (const auto& c : drv.lastCalls())
                    if (c.type != 8 && c.type != 10 && c.type != 11 && c.type != 12) mine.push_back(c);
                const bool restartTick = (b.lives() & 0x7F) < (a.lives() & 0x7F) || (b.level() > a.level() && a.dotsLeft() == 1);
                if (!restartTick && !(mine == logged[k])) {
                    if (badCalls++ < 3) {
                        std::printf("    %s tick %zu: pac before %02X dir %u req %u, mine after %02X dir %u, oracle after %02X dir %u; mine", letter.c_str(), k, a.pacX(), a.pacDirection(), a.zp(0x63), drv.core().state().pacX(), drv.core().state().pacDirection(), b.pacX(), b.pacDirection());
                        for (const auto& c : mine) std::printf(" [%d %d %d %d]", c.type, c.p[0], c.p[1], c.p[2]);
                        std::printf(" | oracle");
                        for (const auto& c : logged[k]) std::printf(" [%d %d %d %d]", c.type, c.p[0], c.p[1], c.p[2]);
                        std::printf("\n");
                    }
                }
            }
            bool pokedLives = false;
            for (auto it = pr.first; it != pr.second; ++it) pokedLives = pokedLives || it->second.address == 0x58 || it->second.address == 0x55;
            const bool oracleRestart = !pokedLives && ((b.lives() & 0x7F) < (a.lives() & 0x7F) || (b.level() > a.level() && a.dotsLeft() == 1));
            const bool restart = ev.caught || ev.levelComplete || ev.outsideMap || oracleRestart;
            if (restart) {
                const bool outside = ev.outsideMap && b.dotsLeft() < a.dotsLeft();
                drv.core().state() = b;
                loadShared(ram.data() + 12 + (k + 1) * 1280);
                if (outside) drv.core().itemEatenOutsideTheMap();
                drv.rebuildScreen();
                ++resyncs;
                if (false) std::printf("    %s resync at tick %zu: caught %d level %d outside %d oracle %d\n", letter.c_str(), k, ev.caught, ev.levelComplete, ev.outsideMap, oracleRestart);
            }
        }
        std::printf("    scenario %s: %zu ticks, %zu pages compared, %zu differ (%zu catches / level ends / poke artefacts resynchronised)\n", letter.c_str(), limit, compared, badPages, resyncs);
        CHECK(compared >= 20);
        CHECK(badPages == 0);
        ++scenarios;
    }
    CHECK(scenarios >= 1);
}

// TASK-033-4d: a tick without any drawing costs exactly what the attract game's 4,757 idle ticks cost (the model for ticks with drawing is checked in test_attract)
TEST(tick_cost_model_idle_tick) {
    using namespace pacman::core;
    datapack::Pack none;
    CHECK(tickCostCycles({}, none, true, false, false) == 2'927);
    CHECK(tickCostCycles({}, none, false, false, false) == 2'987);
}

// TASK-033-3b: the live death and GAME OVER sequences of the driver (structure and total times; the timing model itself is checked against 220 recorded deaths in test_deathtiming)
TEST(gamedriver_death_and_game_over_sequences_have_the_oracles_shape) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    const auto built = datapack::buildPack(readAll(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    for (int lives = 3; lives >= 1; lives -= 2) {   // three lives: an ordinary death; one life: the last one
        GameDriver drv;
        CHECK(drv.init(loaded.pack));
        CHECK(drv.newGame());
        drv.setPlayDeath(true);
        for (int i = 0; i < 20; ++i) drv.tick();   // let the ghosts leave the house a little
        GameState& s = drv.core().state();
        s.zp(0x58) = static_cast<std::uint8_t>(0x80 | lives);
        for (auto& g : s.ghosts) {   // all four ghosts on Pac-Man
            g.b[0] = s.pacX();
            g.b[1] = s.pacY();
        }
        TickEvents ev;
        for (int i = 0; i < 3 && !ev.caught; ++i) ev = drv.tick();
        CHECK(ev.caught);
        CHECK(ev.gameOver == (lives == 1));
        const auto& seq = drv.sequence();
        CHECK(seq.size() == (lives == 1 ? 21u : 22u));
        bool ascending = true;
        int sounds = 0;
        for (std::size_t i = 1; i < seq.size(); ++i) ascending = ascending && seq[i].at > seq[i - 1].at;
        for (const auto& st : seq) sounds += st.deathSound > 0;
        CHECK(ascending && sounds == 6);
        if (seq.empty()) continue;
        if (lives == 1) {
            // GAME OVER: the long wait ends the sequence (10,453,613 cycles after the text)
            CHECK(seq.back().at - seq[seq.size() - 2].at == kGameOverTextDraw + kGameOverPause);
        } else {
            CHECK(seq.back().at - seq[seq.size() - 2].at > 3'000'000 && seq.back().at > 6'000'000 && seq.back().at < 6'100'000);   // about 6.0 million cycles in all
        }
    }
}
