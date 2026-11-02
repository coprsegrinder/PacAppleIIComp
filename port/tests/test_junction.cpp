// Tests of Pac-Man at the junction graph (src/core/junction): synthetic cases for every branch of $78A5-$7961, and a per-tick comparison with the oracle
// recordings (needs --ticks and --image: the graph comes from the data pack). In every tick in which Pac-Man acts ($59 = 1 at the start of the tick)
// the keys of that tick are polled, junctionStep runs on the recorded state, and the direction fields must equal the oracle's at the next tick.
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "../src/core/input.h"
#include "../src/core/junction.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;
using namespace pacman::core;

namespace {

// a tiny graph: junction 0 at (10,10) with exits down -> 2 and right -> 1; junction 1 at (22,10) with the exit left -> 0; junction 2 at (10,22), exit up -> 0
JunctionGraph tiny() {
    JunctionGraph g;
    g.set(0, Junction{10, 10, {2, 0xFF, 1, 0xFF}});
    g.set(1, Junction{22, 10, {0xFF, 0xFF, 0xFF, 0}});
    g.set(2, Junction{10, 22, {0xFF, 0, 0xFF, 0xFF}});
    return g;
}

GameState at(int x, int y, int dir, int wanted, int j, int next) {
    GameState s;
    s.pacX() = static_cast<std::uint8_t>(x);
    s.pacY() = static_cast<std::uint8_t>(y);
    s.pacDirection() = static_cast<std::uint8_t>(dir);
    s.requestedDirection() = static_cast<std::uint8_t>(wanted);
    s.pacJunction() = static_cast<std::uint8_t>(j);
    s.pacNextJunction() = static_cast<std::uint8_t>(next);
    return s;
}

}  // namespace

TEST(junction_accepts_a_possible_turn_at_a_junction) {
    const auto g = tiny();
    GameState s = at(10, 10, 2, 0, 0, 0xFF);  // at junction 0, moving right, wants down (possible)
    CHECK(junctionStep(s, g) == JunctionOutcome::Move);
    CHECK(s.pacDirection() == 0 && s.pacNextJunction() == 2 && s.pacJunction() == 0);
    GameState keep = at(10, 10, 2, 1, 0, 0xFF);  // wants up (not possible), current right is possible: keep going right
    CHECK(junctionStep(keep, g) == JunctionOutcome::Move);
    CHECK(keep.pacDirection() == 2 && keep.pacNextJunction() == 1);
}

TEST(junction_blocked_when_neither_direction_works) {
    const auto g = tiny();
    GameState s = at(22, 10, 2, 1, 1, 0xFF);  // junction 1 has only 'left'; moving right and wanting up: both impossible
    CHECK(junctionStep(s, g) == JunctionOutcome::Blocked);
    CHECK(s.pacDirection() == 1);  // the wish is taken over before the block is found ($78F9)
    CHECK(s.zp(0x66) == 2);        // drawn standing
    GameState s2 = at(22, 10, 3, 1, 1, 0xFF);  // moving left (possible) but wanting up (impossible): keeps left
    CHECK(junctionStep(s2, g) == JunctionOutcome::Move);
    CHECK(s2.pacDirection() == 3 && s2.pacNextJunction() == 0);
}

TEST(junction_arrival_at_the_next_junction_and_between_junctions) {
    const auto g = tiny();
    GameState s = at(22, 10, 2, 3, 0, 1);  // heading from 0 to 1 and standing on junction 1
    CHECK(junctionStep(s, g) == JunctionOutcome::Move);
    CHECK(s.pacJunction() == 1 && s.pacNextJunction() == 0 && s.pacDirection() == 3);  // arrived, turned round (left is possible)
    GameState mid = at(16, 10, 2, 3, 0, 1);  // between 0 and 1: reversing is allowed
    CHECK(junctionStep(mid, g) == JunctionOutcome::Move);
    CHECK(mid.pacDirection() == 3 && mid.pacJunction() == 0 && mid.pacNextJunction() == 1);  // junction numbers unchanged
    GameState turn = at(16, 10, 2, 0, 0, 1);  // between junctions a sideways wish is ignored
    junctionStep(turn, g);
    CHECK(turn.pacDirection() == 2);
    GameState same = at(16, 10, 2, 2, 0, 1);  // wishing the same direction changes nothing
    junctionStep(same, g);
    CHECK(same.pacDirection() == 2);
    GameState none = at(16, 10, 2, 0xFF, 0, 1);  // no wish ($63 negative)
    junctionStep(none, g);
    CHECK(none.pacDirection() == 2);
}

TEST(junction_tunnel_flags_at_the_ends_26_and_29) {
    JunctionGraph g;
    g.set(26, Junction{0x0A, 0x20, {0xFF, 0xFF, 5, 6}});  // both ends have some exits; the numbers 26 and 29 are what matters
    g.set(29, Junction{0xD0, 0x20, {0xFF, 0xFF, 7, 8}});
    GameState left = at(0x0A, 0x20, 3, 3, 26, 0xFF);  // at 26 heading left: the tunnel starts
    CHECK(junctionStep(left, g) == JunctionOutcome::Tunnel);
    CHECK(left.zp(0x69) == 1 && left.zp(0x6A) == 6 && left.zp(0x6C) == 0);
    GameState right = at(0x0A, 0x20, 2, 2, 26, 0xFF);  // at 26 heading right (into the maze): no tunnel
    right.zp(0x69) = 1;
    CHECK(junctionStep(right, g) == JunctionOutcome::Move);
    CHECK(right.zp(0x69) == 0 && right.zp(0x6A) == 6);
    GameState r29 = at(0xD0, 0x20, 2, 2, 29, 0xFF);  // at 29 heading right: tunnel; cell column 0x15
    CHECK(junctionStep(r29, g) == JunctionOutcome::Tunnel);
    CHECK(r29.zp(0x69) == 1 && r29.zp(0x6A) == 0x15);
}

TEST(junction_demo_stream_byte_replaces_the_wish) {
    const auto g = tiny();
    GameState s = at(10, 10, 2, 9, 0, 0xFF);
    s.demo = 1;
    junctionStep(s, g, static_cast<std::uint8_t>(0));  // the demo stream says: down
    CHECK(s.requestedDirection() == 0 && s.pacDirection() == 0);
    GameState game = at(10, 10, 2, 0, 0, 0xFF);  // in a real game the stream byte is ignored
    junctionStep(game, g, static_cast<std::uint8_t>(1));
    CHECK(game.requestedDirection() == 0);
}

TEST(junction_against_the_oracle_recordings) {
    if (testfw::config().ticks.empty() || testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --ticks and --image)\n");
        return;
    }
    std::ifstream f(testfw::config().image, std::ios::binary);
    const std::vector<std::uint8_t> img((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    CHECK_MSG(built.ok, built.reason);
    if (!built.ok) return;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    CHECK_MSG(loaded.ok, loaded.reason);
    if (!loaded.ok) return;
    JunctionGraph graph;
    CHECK(graph.load(loaded.pack));
    CHECK(graph.at(0).x == 0x41 && graph.at(0).y == 0x0B && graph.at(0).next[0] == 6);  // first record as printed from the program body

    std::size_t actingTicks = 0, blocked = 0, tunnels = 0, turns = 0, restarts = 0;
    for (std::size_t pos = 0; pos <= testfw::config().ticks.size();) {
        const std::size_t comma = testfw::config().ticks.find(',', pos);
        const std::size_t end = comma == std::string::npos ? testfw::config().ticks.size() : comma;
        const std::string path = testfw::config().ticks.substr(pos, end - pos);
        pos = end + 1;
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), path + ": " + why);
        if (!tr.ok()) continue;
        std::multimap<std::uint32_t, std::uint32_t> keysAt;
        for (const auto& in : tr.inputs()) keysAt.emplace(in.tick, in.key);
        InputPoller poller;
        KeyConfig keys;
        std::size_t mismatches = 0, firstBad = 0;
        std::multimap<std::uint32_t, TickPoke> pokesAt;
        for (const auto& p : tr.pokes()) pokesAt.emplace(p.tick, p);
        for (std::size_t k = 0; k + 1 < tr.ticks(); ++k) {
            const auto range = keysAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = range.first; it != range.second; ++it) poller.pushKey(static_cast<std::uint8_t>(it->second));
            GameState s = fromOracleLayout(tr.state(k));
            const auto pr = pokesAt.equal_range(static_cast<std::uint32_t>(k));   // the scenario's zero-page pokes of this tick, applied before the tick runs
            for (auto it = pr.first; it != pr.second; ++it)
                if (it->second.address >= 0x52 && it->second.address <= 0x6F) s.zp(static_cast<std::uint16_t>(it->second.address)) = static_cast<std::uint8_t>(it->second.value);
            poller.poll(s, keys, {});
            if (tr.at(k, 0x59) != 1) continue;  // Pac-Man does not act in this tick
            const bool restart = tr.at(k + 1, 0x60) == 0x8B && tr.at(k + 1, 0x61) == 0x8F && !(tr.at(k, 0x60) == 0x8B && tr.at(k, 0x61) == 0x8F);
            const std::uint8_t oldDir = s.pacDirection();
            const auto outcome = junctionStep(s, graph);
            if (restart) {
                ++restarts;
                continue;
            }
            ++actingTicks;
            blocked += outcome == JunctionOutcome::Blocked;
            tunnels += outcome == JunctionOutcome::Tunnel;
            turns += s.pacDirection() != oldDir;
            // fields this piece owns; in a tunnel tick the transit code ($7B2F) also rewrites $64/$65, so only $62 $63 $69 are compared there
            bool ok = s.pacDirection() == tr.at(k + 1, 0x62) && s.requestedDirection() == tr.at(k + 1, 0x63) && s.zp(0x69) == tr.at(k + 1, 0x69);
            if (outcome != JunctionOutcome::Tunnel) ok = ok && s.pacJunction() == tr.at(k + 1, 0x64) && s.pacNextJunction() == tr.at(k + 1, 0x65);
            if (outcome == JunctionOutcome::Blocked) ok = ok && s.zp(0x66) == tr.at(k + 1, 0x66);
            if (!ok && mismatches++ == 0) firstBad = k;
        }
        std::printf("    %s: mismatches %zu%s\n", path.c_str(), mismatches, mismatches ? (" (first at tick " + std::to_string(firstBad) + ")").c_str() : "");
        CHECK(mismatches == 0);
    }
    std::printf("    %zu acting ticks compared (%zu turns, %zu blocked, %zu tunnel), %zu restart ticks skipped\n", actingTicks, turns, blocked, tunnels, restarts);
    CHECK(actingTicks > 3000 && turns > 100 && blocked > 20);
}
