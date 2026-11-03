// TASK-033-6: several players. The original plays the players in turn: when Pac-Man is caught, this player's bytes ($52-$59) and maze are saved ($53BE, $5394), the next player with
// a life left is loaded ($53CF, $5360) and starts his life ($53E6); the game is over when nobody has a life left ($8C20-$8C4D, $6EC3). Checked against the oracle's recordings of two-player
// games (tools/record_ticks.py P, Q, R, S, U): at every tick in which the original restarts a life, the core (driven tick by tick with the recording's keys, pokes and ghosts) must reach
// the oracle's state of the next tick byte for byte (score, level, items left, lives, the maze of the other player, positions). Needs --image and --playerticks <ticks_P.bin,...>.
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "../src/core/gamecore.h"
#include "../src/core/ticktrace.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;
using namespace pacman::core;

namespace {

std::vector<std::uint8_t> readAllP(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

std::vector<std::string> splitP(const std::string& list) {
    std::vector<std::string> out;
    for (std::size_t pos = 0; pos <= list.size();) {
        const std::size_t comma = list.find(',', pos);
        const std::size_t end = comma == std::string::npos ? list.size() : comma;
        if (end > pos) out.push_back(list.substr(pos, end - pos));
        pos = end + 1;
    }
    return out;
}

// the ghosts fed from the recording; at a catch tick the ghost that touches Pac-Man is found as in tests/test_gamecore.cpp
class FedGhostsP : public GhostSource {
public:
    const GameState* next = nullptr;
    const GameState* before = nullptr;
    bool caughtTick = false;
    std::array<bool, 4> step(GameState& s, bool) override {
        std::array<bool, 4> moved{};
        for (std::size_t g = 0; g < 4; ++g) moved[g] = before->ghosts[g].b[0] != next->ghosts[g].b[0] || before->ghosts[g].b[1] != next->ghosts[g].b[1];
        s.ghosts = next->ghosts;
        if (caughtTick) {
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
        for (std::size_t g = 0; g < 4; ++g) {
            const auto& b0 = before->ghosts[g].b;
            auto& b1 = s.ghosts[g].b;
            if (b0[9] == 0 && b1[9] == 1 && b0[8] != 0 && b1[8] == 0) {
                b1[8] = b0[8];
                b1[9] = 0;
            }
        }
        s.mode78 = next->mode78;
        s.mode79 = next->mode79;
        if (next->cornerPhase == 0 && s.cornerPhase != 0) s.zp(0x68) = 2;
        s.cornerPhase = next->cornerPhase;
        return moved;
    }
};

}  // namespace

TEST(players_rotation_equals_the_oracle) {
    if (testfw::config().playerTicks.empty() || testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --playerticks and --image)\n");
        return;
    }
    const auto built = datapack::buildPack(readAllP(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    std::size_t switches = 0, bad = 0, ticksRun = 0, wrongEvents = 0, gameOvers = 0;
    for (const auto& path : splitP(testfw::config().playerTicks)) {
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), path + ": " + why);
        if (!tr.ok()) continue;
        GameCore core;
        CHECK(core.init(loaded.pack));
        CHECK(core.newGame(2));
        const std::uint8_t startX = core.state().pacX(), startY = core.state().pacY();
        const GameState fresh = core.state();
        auto atStart = [&](const GameState& g) {   // Pac-Man and all four ghosts at their start places
            bool same = g.pacX() == startX && g.pacY() == startY;
            for (std::size_t i = 0; i < 4; ++i) same = same && g.ghosts[i].b[0] == fresh.ghosts[i].b[0] && g.ghosts[i].b[1] == fresh.ghosts[i].b[1];
            return same;
        };
        core.state() = fromOracleLayout(tr.state(0));
        std::multimap<std::uint32_t, std::uint32_t> keysAt;
        for (const auto& in : tr.inputs()) keysAt.emplace(in.tick, in.key);
        std::multimap<std::uint32_t, TickPoke> pokesAt;
        for (const auto& p : tr.pokes()) pokesAt.emplace(p.tick, p);
        FedGhostsP feed;
        for (std::size_t k = 0; k + 1 < tr.ticks(); ++k) {
            const auto kr = keysAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = kr.first; it != kr.second; ++it) core.input().pushKey(static_cast<std::uint8_t>(it->second));
            GameState& s = core.state();
            bool pokedLives = false;
            const auto pr = pokesAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = pr.first; it != pr.second; ++it) {
                const auto a = it->second.address;
                if (a >= 0x52 && a <= 0x6F) s.zp(static_cast<std::uint16_t>(a)) = static_cast<std::uint8_t>(it->second.value);
                if (a >= 0x623A && a <= 0x65B9) s.map[a - 0x623A] = static_cast<std::uint8_t>(it->second.value);
                pokedLives = pokedLives || a == 0x58;
            }
            const GameState a = fromOracleLayout(tr.state(k));
            const GameState b = fromOracleLayout(tr.state(k + 1));
            // the original restarts a life in this tick: Pac-Man is at his start place afterwards and was not before
            const bool oracleRestart = atStart(b) && !atStart(a) && b.level() == a.level();   // (a level end also puts Pac-Man at the start, but raises the level)
            feed.next = &b;
            feed.before = &a;
            feed.caughtTick = oracleRestart;
            const TickEvents ev = core.tick(feed);
            ++ticksRun;
            if (ev.caught != oracleRestart) {
                ++wrongEvents;
                if (wrongEvents <= 8) std::printf("    %s tick %zu: caught: oracle %d mine %d\n", path.c_str(), k, oracleRestart, ev.caught);
                core.state() = b;
                continue;
            }
            if (oracleRestart) {
                const auto m2 = toOracleLayout(core.state()), o2 = toOracleLayout(b);
                std::size_t d = 0;
                std::string list;
                for (std::size_t i = 0; i < m2.size(); ++i) {
                    if (i == 30 || i == 31) continue;   // $78 $79: working flags of the ghost set-up
                    if (m2[i] != o2[i]) {
                        ++d;
                        if (list.size() < 300) list += " [" + std::to_string(i) + "] " + std::to_string(m2[i]) + ">" + std::to_string(o2[i]);
                    }
                }
                ++switches;
                std::printf("    %s tick %zu: catch: player now %d, saved lives p1 %02X p2 %02X, state lives %02X, game over %d\n", path.c_str(), k, core.player(), core.savedLives(1), core.savedLives(2), core.state().lives(), ev.gameOver);
                gameOvers += ev.gameOver ? 1 : 0;
                if (d != 0) {
                    ++bad;
                    if (bad <= 6) std::printf("    %s tick %zu: the restarted life (player %d, switched %d) differs in %zu bytes (mine>oracle):%s\n", path.c_str(), k, core.player(), ev.playerSwitched, d, list.c_str());
                }
                core.state() = b;   // and go on from the oracle's state (the ghosts' tables are not part of the comparison)
                continue;
            }
            // between restarts the core is held to the oracle's bytes it owns (the logic of one player is checked in test_gamecore); a difference is copied so that it does not cascade
            const auto mine = toOracleLayout(core.state());
            const auto theirs = toOracleLayout(b);
            bool diff = false;
            for (std::size_t i = 0; i < mine.size(); ++i) {
                if ((i >= 34 && i < 114) || i == 30 || i == 31 || i == 33) continue;
                diff = diff || mine[i] != theirs[i];
            }
            if (diff) {
                core.state() = fromOracleLayout(theirs);
                core.state().ghosts = b.ghosts;
            }
            (void)pokedLives;
        }
        if (path.find("ticks_R") != std::string::npos) {
            // R ends with the death of player 1's last life while player 2 has none either: the recording stops there (the main loop is left), so the last tick is run on its own
            const GameState last = fromOracleLayout(tr.state(tr.ticks() - 1));
            core.state() = last;
            feed.next = &last;
            feed.before = &last;
            feed.caughtTick = true;
            const TickEvents ev = core.tick(feed);
            std::printf("    %s: the last tick: caught %d, game over %d (expected: caught and game over)\n", path.c_str(), ev.caught, ev.gameOver);
            CHECK(ev.caught && ev.gameOver && !ev.playerSwitched);
        }
    }
    std::printf("    %zu ticks run, %zu life restarts compared with the oracle (two players): %zu differ, %zu catch events disagree, %zu ended the game\n", ticksRun, switches, bad, wrongEvents, gameOvers);
    CHECK(wrongEvents == 0);
    CHECK(bad == 0);
    CHECK(switches >= 4);
}
