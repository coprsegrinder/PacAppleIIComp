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

#include "../src/core/attract.h"
#include "../src/core/gamecore.h"
#include "../src/core/gamedriver.h"
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
    std::size_t switches = 0, bad = 0, ticksRun = 0, wrongEvents = 0, gameOvers = 0, levelEnds = 0;
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
            if (b.level() > a.level() && a.dotsLeft() == 1) {   // the level end of whoever plays (U: player 2): the same routine as with one player
                CHECK(ev.levelComplete);
                core.startNextLevel();
                const auto m2 = toOracleLayout(core.state()), o2 = toOracleLayout(b);
                std::size_t d = 0;
                for (std::size_t i = 0; i < m2.size(); ++i) d += i != 30 && i != 31 && m2[i] != o2[i];
                ++levelEnds;
                if (d != 0) {
                    ++bad;
                    std::printf("    %s tick %zu: the level change (player %d) differs in %zu bytes\n", path.c_str(), k, core.player(), d);
                }
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
    std::printf("    %zu ticks run, %zu life restarts and %zu level ends compared with the oracle (two players): %zu differ, %zu catch events disagree, %zu ended the game\n", ticksRun, switches, levelEnds, bad, wrongEvents, gameOvers);
    CHECK(wrongEvents == 0);
    CHECK(bad == 0);
    CHECK(switches >= 4);
    CHECK(levelEnds >= 1);
}

// The first play screen for 1 to 5 players: labels, marks, dotted lines, score boxes and scores of every player at the places of the tables ($55C7, $6992, $7141), as the oracle draws them.
TEST(players_first_screens_equal_the_oracle) {
    if (testfw::config().playerPages.empty() || testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --playerpages and --image)\n");
        return;
    }
    const auto built = datapack::buildPack(readAllP(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    const auto files = splitP(testfw::config().playerPages);
    CHECK(files.size() == 5);
    std::size_t bad = 0;
    for (std::size_t n = 1; n <= files.size(); ++n) {
        const auto oracle = readAllP(files[n - 1]);
        CHECK(oracle.size() == hires::kPageBytes);
        GameDriver drv;
        CHECK(drv.init(loaded.pack));
        CHECK(drv.newGame(static_cast<int>(n)));
        std::size_t d = 0;
        for (std::size_t b = 0; b < hires::kPageBytes && b < oracle.size(); ++b) d += drv.page().byte(b) != oracle[b];
        std::printf("    %zu player%s: the first screen differs from the oracle in %zu of 8,192 bytes\n", n, n == 1 ? "" : "s", d);
        bad += d != 0;
    }
    CHECK(bad == 0);
}

// With several players the rebuild of the screen after a catch draws more (the labels, marks, lines, boxes and scores of the other players). The oracle's recordings of forced catches
// (tools/death_timing.py 31 1500 forced <players>: the first death of each run, player 1 with a two-digit score, the others at 0) took 10,629 (2 players), 19,575 (3) and 37,221 (5)
// cycles longer between the rebuild and READY than the same death with one player. 1,814 of each are not the labels: the new player's fresh maze has one more dot (215) and he has one more spare life (a life icon: 1,599), as the events show; the rest is hudExtraCycles (the cycle count of $550A, summed up from its instructions): 8,815 / 17,761 / 35,407, the time from the HUD's start to the high score's label, which the events measure directly.
TEST(players_rebuild_costs_what_the_oracle_measured) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    const auto built = datapack::buildPack(readAllP(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    struct Row { int players; std::int64_t measured; };
    for (const Row& r : {Row{2, 10'629 - 1'814}, Row{3, 19'575 - 1'814}, Row{5, 37'221 - 1'814}}) {
        std::array<std::uint32_t, 6> scores{};
        std::array<int, 6> lives{};
        scores[1] = 0x90;                                   // two digits
        for (int p = 1; p <= 5; ++p) lives[static_cast<std::size_t>(p)] = 0x83;
        const std::int64_t mine = hudExtraCycles(loaded.pack, r.players, 1, scores, lives, 0x90);
        std::printf("    %d players: the rebuild takes %lld cycles more than with one (oracle %lld)\n", r.players, static_cast<long long>(mine), static_cast<long long>(r.measured));
        CHECK(mine == r.measured);
    }
}

// The live driver with two players: a catch hands the game to player 2 (his fresh maze and score 0 on the screen, the label of PLAYER #2 blinking, his lives), the death sequence is
// longer than with one player by what the rebuild of the other player's label and score costs, and when both players are out of lives the game is over.
TEST(players_driver_switches_players) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    const auto built = datapack::buildPack(readAllP(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    std::int64_t total[2] = {0, 0};
    for (int players = 1; players <= 2; ++players) {
        GameDriver drv;
        CHECK(drv.init(loaded.pack));
        CHECK(drv.newGame(players));
        drv.setPlayDeath(true);
        TickEvents ev;
        for (int i = 0; i < 400 && !ev.caught; ++i) {
            drv.core().state().zp(0x60) = drv.core().state().ghosts[0].b[0];   // Pac-Man is put on ghost 1: he is caught
            drv.core().state().zp(0x61) = drv.core().state().ghosts[0].b[1];
            ev = drv.tick();
        }
        CHECK(ev.caught);
        CHECK(!ev.gameOver);
        CHECK(ev.playerSwitched == (players == 2));
        CHECK(drv.core().player() == (players == 2 ? 2 : 1));
        CHECK(!drv.sequence().empty());
        total[players - 1] = drv.sequence().back().at;
        for (const auto& step : drv.sequence()) drv.applySequenceStep(step);
        // the new player's screen: his lives (3 lives = 2 icons), nothing eaten yet
        CHECK((drv.core().state().lives() & 0x7F) == (players == 2 ? 3 : 2));
        if (players == 2) CHECK(drv.core().state().dotsLeft() == 0xF8);
    }
    std::printf("    the death sequence with two players ends %lld cycles after the one with one player's\n", static_cast<long long>(total[1] - total[0]));
    CHECK(total[1] - total[0] > 8'815);
    // both players out of lives: the game is over
    GameDriver drv;
    CHECK(drv.init(loaded.pack));
    CHECK(drv.newGame(2));
    drv.setPlayDeath(true);
    bool over = false;
    int catches = 0;
    for (int i = 0; i < 4000 && !over; ++i) {
        drv.core().state().zp(0x58) = 0x81;                                  // the player who plays has one life left
        drv.core().state().zp(0x60) = drv.core().state().ghosts[0].b[0];
        drv.core().state().zp(0x61) = drv.core().state().ghosts[0].b[1];
        const TickEvents e = drv.tick();
        if (e.caught) ++catches;
        if (catches == 1) CHECK(e.playerSwitched || !e.caught);
        over = e.gameOver;
    }
    CHECK(over && catches == 2);
}
