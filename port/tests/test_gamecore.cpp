// TASK-029h: GameCore::tick against the oracle. The state is carried from tick to tick (no re-start from the oracle's state), with the ghosts FED from the
// oracle (ghost logic is M5) and every byte that is outside what the core owns reported. Pokes and keys of the recording are applied at the start of each tick.
// After a tick in which the oracle restarts a life, completes a level or changes player, our state is replaced by the oracle's (those set-ups are tested
// elsewhere: lifestart, M6). Mismatching bytes are counted per state offset and then copied from the oracle so that one difference does not cascade.
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../src/core/gamecore.h"
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

class FedGhosts : public GhostSource {
public:
    const GameState* next = nullptr;
    const GameState* before = nullptr;
    bool caughtTick = false;
    std::array<bool, 4> step(GameState& s, bool) override {
        std::array<bool, 4> moved{};
        for (std::size_t g = 0; g < 4; ++g) moved[g] = before->ghosts[g].b[0] != next->ghosts[g].b[0] || before->ghosts[g].b[1] != next->ghosts[g].b[1];
        s.ghosts = next->ghosts;
        if (caughtTick) {
            // The recording's next state is already the RESTARTED life, so the ghosts' positions at the moment of the catch are not in it. The feed offers each ghost
            // where it was at the start of the tick and one step further along its direction; the core's contact test has to find the catch at one of them.
            for (std::size_t g = 0; g < 4; ++g) {
                GhostRecord cand = before->ghosts[g];
                static constexpr int dx[4] = {0, 0, 2, -2}, dy[4] = {2, -2, 0, 0};
                GhostRecord moved1 = cand;
                moved1.b[0] = static_cast<std::uint8_t>(cand.b[0] + dx[cand.b[2] & 3]);
                moved1.b[1] = static_cast<std::uint8_t>(cand.b[1] + dy[cand.b[2] & 3]);
                s.ghosts[g] = cand;
                if (ghostContact(s, cand) != Contact::Touch && ghostContact(s, moved1) == Contact::Touch) s.ghosts[g] = moved1;
            }
            return {true, true, true, true};
        }
        for (std::size_t g = 0; g < 4; ++g) {   // a ghost that turns into eyes in this tick is eaten by the CONTACT test of the core: the ghost logic hands it over frightened
            const auto& b0 = before->ghosts[g].b;
            auto& b1 = s.ghosts[g].b;
            if (b0[9] == 0 && b1[9] == 1 && b0[8] != 0 && b1[8] == 0) {
                b1[8] = b0[8];
                b1[9] = 0;
            }
        }
        s.mode78 = next->mode78;
        s.mode79 = next->mode79;
        if (next->cornerPhase == 0 && s.cornerPhase != 0) s.zp(0x68) = 2;   // the end of the frightened period resets the ghost score step (ghost logic, M5)
        s.cornerPhase = next->cornerPhase;
        return moved;
    }
};

std::string offsetName(std::size_t i) {
    char buf[40];
    if (i < 30) std::snprintf(buf, sizeof buf, "$%02zX", 0x52 + i);
    else if (i < 34) std::snprintf(buf, sizeof buf, "$%02X", i == 30 ? 0x78 : i == 31 ? 0x79 : i == 32 ? 0xE8 : 0xE9);
    else if (i < 114) std::snprintf(buf, sizeof buf, "ghost%zu.b%zu", (i - 34) / 20 + 1, (i - 34) % 20);
    else std::snprintf(buf, sizeof buf, "map[%zu]", i - 114);
    return buf;
}

}  // namespace

TEST(gamecore_against_the_oracle_recordings) {
    if (testfw::config().ticks.empty() || testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --ticks and --image)\n");
        return;
    }
    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    std::map<std::size_t, std::pair<std::size_t, std::size_t>> diffs;   // offset -> (count, first tick)
    std::map<std::size_t, std::string> firstFile;
    std::size_t ticksRun = 0, resyncs = 0, wrongEvents = 0, caughtChecked = 0, badRestart = 0, levelChangesChecked = 0, badLevelChange = 0;
    std::vector<std::string> files = split(testfw::config().ticks);
    for (const auto& p : split(testfw::config().startTicks)) files.push_back(p);   // games started at a higher level through the hidden menu: replayed like the others
    if (!testfw::config().joyTicks.empty()) files.push_back(testfw::config().joyTicks);   // a game in joystick mode: the original's paddle routines steer
    if (!testfw::config().joyDemo.empty()) files.push_back(testfw::config().joyDemo);   // the attract game in joystick mode
    if (!testfw::config().demo.empty()) files.push_back(testfw::config().demo);   // the attract demo: Pac-Man steered by the original's own move stream
    std::size_t nearTotal = 0, nearFiles = 0;
    for (const auto& path : files) {
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), path + ": " + why);
        if (!tr.ok()) continue;
        GameCore core;
        CHECK(core.init(loaded.pack));
        core.state() = fromOracleLayout(tr.state(0));
        const std::vector<std::uint8_t> pad = readAll(path + ".pad");   // a game played in joystick mode (tools/record_joystick_ticks.py): the paddle positions of every tick
        if (!pad.empty()) core.port().joystickMode = true;
        std::multimap<std::uint32_t, std::uint32_t> keysAt;
        for (const auto& in : tr.inputs()) keysAt.emplace(in.tick, in.key);
        std::multimap<std::uint32_t, TickPoke> pokesAt;
        for (const auto& p : tr.pokes()) pokesAt.emplace(p.tick, p);
        FedGhosts feed;
        for (std::size_t k = 0; k + 1 < tr.ticks(); ++k) {
            if (!pad.empty() && 2 * k + 1 < pad.size()) {
                core.port().paddleX = pad[2 * k];
                core.port().paddleY = pad[2 * k + 1];
            }
            const auto kr = keysAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = kr.first; it != kr.second; ++it) core.input().pushKey(static_cast<std::uint8_t>(it->second));
            GameState& s = core.state();
            const auto pr = pokesAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = pr.first; it != pr.second; ++it) {
                const auto a = it->second.address;
                if (a >= 0x52 && a <= 0x6F) s.zp(static_cast<std::uint16_t>(a)) = static_cast<std::uint8_t>(it->second.value);
                if (a >= 0x623A && a <= 0x65B9) s.map[a - 0x623A] = static_cast<std::uint8_t>(it->second.value);
            }
            GameState a = fromOracleLayout(tr.state(k));
            for (auto it = pr.first; it != pr.second; ++it) {   // a poke that puts a ghost somewhere (tools/record_nearghost.py) moves it BEFORE the tick: it is not a step of the ghost
                const auto addr = it->second.address;
                if (addr >= 0x84 && addr < 0x84 + 80) a.ghosts[(addr - 0x84) / 20].b[(addr - 0x84) % 20] = static_cast<std::uint8_t>(it->second.value);
            }
            core.state().ghosts = a.ghosts;
            const GameState b = fromOracleLayout(tr.state(k + 1));
            feed.next = &b;
            feed.before = &a;
            feed.caughtTick = (b.lives() & 0x7F) < (a.lives() & 0x7F) && a.lives() != 0;
            const TickEvents ev = core.tick(feed);
            ++ticksRun;
            const bool oracleCaught = (b.lives() & 0x7F) < (a.lives() & 0x7F) && a.lives() != 0;
            const bool oracleLevelEnd = b.level() > a.level() && a.dotsLeft() == 1;
            bool pokedLevel = false;
            for (auto it = pr.first; it != pr.second; ++it) pokedLevel = pokedLevel || it->second.address == 0x55 || it->second.address == 0x58;
            if (!pokedLevel) {
                if (oracleCaught != ev.caught) { ++wrongEvents; if (wrongEvents <= 12) std::printf("    %s tick %zu: caught: oracle %d mine %d\n", path.c_str(), k, oracleCaught, ev.caught); }
                if (oracleLevelEnd != ev.levelComplete) { ++wrongEvents; if (wrongEvents <= 12) std::printf("    %s tick %zu: level end: oracle %d mine %d\n", path.c_str(), k, oracleLevelEnd, ev.levelComplete); }
            }
            if (ev.caught && !ev.gameOver && !pokedLevel) {   // the restarted life must equal the oracle's state (except the working-slot flags $78 $79)
                const auto m2 = toOracleLayout(core.state()), o2 = toOracleLayout(b);
                std::size_t d = 0;
                std::string which;
                for (std::size_t i = 0; i < m2.size(); ++i) {
                    if (i != 30 && i != 31 && m2[i] != o2[i]) {
                        ++d;
                        which += " " + offsetName(i) + " mine " + std::to_string(m2[i]) + " oracle " + std::to_string(o2[i]);
                    }
                }
                ++caughtChecked;
                if (d != 0) { ++badRestart; if (badRestart <= 3) std::printf("    %s tick %zu: restarted life differs in %zu bytes:%s\n", path.c_str(), k, d, which.c_str()); }
            }
            if (ev.levelComplete && !pokedLevel) {   // TASK-031-3: the level change must equal the oracle's first state of the new level
                core.startNextLevel();
                const auto m2 = toOracleLayout(core.state()), o2 = toOracleLayout(b);
                std::string list;
                std::size_t d = 0;
                for (std::size_t i = 0; i < m2.size(); ++i) {
                    if (i == 30 || i == 31 || m2[i] == o2[i]) continue;
                    ++d;
                    if (list.size() < 400) list += " [" + std::to_string(i) + "] " + std::to_string(m2[i]) + ">" + std::to_string(o2[i]);
                }
                ++levelChangesChecked;
                if (d != 0) {
                    ++badLevelChange;
                    std::printf("    %s tick %zu: level change differs in %zu bytes (mine>oracle, decimal):%s\n", path.c_str(), k, d, list.c_str());
                }
            }
            const bool restart = ev.caught || ev.levelComplete || ev.outsideMap || b.level() != a.level() || (b.lives() & 0x7F) < (a.lives() & 0x7F);
            if (restart) {
                const bool outside = ev.outsideMap && b.dotsLeft() < a.dotsLeft();
                core.state() = b;
                if (outside) core.itemEatenOutsideTheMap();
                ++resyncs;
                continue;
            }
            const auto mine = toOracleLayout(core.state());
            const auto theirs = toOracleLayout(b);
            for (std::size_t i = 0; i < mine.size(); ++i) {
                if (i >= 34 && i < 114) continue;   // fed ghosts
                if (i == 30 || i == 31 || i == 33) continue;   // fed $78 $79 $E9
                if (mine[i] == theirs[i]) continue;
                auto& d = diffs[i];
                if (d.first++ == 0) {
                    d.second = k;
                    char buf[160];
                    std::snprintf(buf, sizeof buf, "%s (mine %02X oracle %02X; before %02X; acted %d ate %d energizer %d eaten %d%d%d%d)", path.c_str(), mine[i], theirs[i],
                                  toOracleLayout(a)[i], ev.acted, ev.ate, ev.energizer, ev.ghostEaten[0], ev.ghostEaten[1], ev.ghostEaten[2], ev.ghostEaten[3]);
                    firstFile[i] = buf;
                }
                core.state() = fromOracleLayout(theirs);   // copy the oracle's byte (all of them, cheap) so that one difference does not cascade
                core.state().ghosts = b.ghosts;
            }
        }
        if (path.find("ticks_Y") != std::string::npos) {   // the recordings with a ghost put near Pac-Man (tools/record_nearghost.py)
            nearTotal += core.nearGhostDelayCount();
            ++nearFiles;
        }
    }
    if (nearFiles != 0) {
        std::printf("    near-ghost delay: added a tick %zu times in %zu recordings, and Pac-Man's step countdown equals the oracle's in every tick\n", nearTotal, nearFiles);
        CHECK(nearTotal >= 10 * nearFiles);
    }
    std::printf("    %zu ticks run, %zu resynchronised after a catch / level end\n", ticksRun, resyncs);
    for (const auto& [off, d] : diffs) std::printf("    %-12s differs in %zu ticks (first: tick %zu of %s)\n", offsetName(off).c_str(), d.first, d.second, firstFile[off].c_str());
    std::printf("    %zu catches compared with the oracle's restarted life, %zu differ; %zu catch / level-end events disagree\n", caughtChecked, badRestart, wrongEvents);
    CHECK(diffs.empty());
    std::printf("    %zu level changes compared with the oracle, %zu differ\n", levelChangesChecked, badLevelChange);
    CHECK(wrongEvents == 0 && badRestart == 0 && caughtChecked >= 8);
    CHECK(badLevelChange == 0 && levelChangesChecked >= 7);
    CHECK(ticksRun > 10000);
}

TEST(new_game_at_a_start_level_equals_the_first_tick_of_the_real_game) {
    if (testfw::config().startTicks.empty() || testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --startticks and --image)\n");
        return;
    }
    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    std::size_t files = 0, bad = 0;
    for (const auto& path : split(testfw::config().startTicks)) {
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), path + ": " + why);
        if (!tr.ok()) continue;
        const int rank = tr.at(0, 0x55);
        GameCore core;
        CHECK(core.init(loaded.pack));
        CHECK(core.newGame(1, rank));
        const auto mine = toOracleLayout(core.state());
        const auto theirs = tr.state(0);
        std::string which;
        std::size_t d = 0;
        for (std::size_t i = 0; i < mine.size(); ++i) {
            if (mine[i] == theirs[i] || i == 30 || i == 31) continue;   // ($78 $79 are leftovers of the previous screen's last ghost pass: 1 after the demo of the golden games, 0 after the hidden menu; the first ghost pass overwrites them before they are read)
            ++d;
            if (which.size() < 300) which += " " + offsetName(i) + " mine " + std::to_string(mine[i]) + " oracle " + std::to_string(theirs[i]);
        }
        ++files;
        std::printf("    rank %d (%s): first tick differs in %zu bytes%s\n", rank, path.c_str(), d, which.c_str());
        bad += d != 0;
    }
    CHECK(files >= 1 && bad == 0);
}

TEST(esc_pauses_the_tick_in_both_input_modes_and_the_jingle_poll_cost_follows_the_mode) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    for (const bool joystick : {false, true}) {
        GameCore core;
        CHECK(core.init(loaded.pack));
        CHECK(core.newGame(1, 1));
        core.port().joystickMode = joystick;
        CHECK(core.jinglePollExtra() == 0 || joystick);
        if (joystick) CHECK(core.jinglePollExtra() > 2500 && core.jinglePollExtra() < 2800);
        core.state().zp(0x59) = 1;              // Pac-Man's step timer runs out in this tick: the joystick poll happens
        core.input().pushKey(0x1B);             // Esc with no key after it: the poll waits (the tick is paused)
        const TickEvents ev = core.tick();
        CHECK(ev.paused);
        core.input().pushKey(0x41);             // any key ends the pause
        const TickEvents again = core.tick();
        CHECK(!again.paused);
    }
}
