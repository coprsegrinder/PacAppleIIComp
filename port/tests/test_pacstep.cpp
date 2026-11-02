// Tests of Pac-Man's step and cell counters (src/core/pacstep) together with the junction logic: synthetic cases, and per-tick comparisons with the oracle
// recordings (--ticks, --image): the golden scenarios, the level recordings, the TUNNEL recording (tools/record_special.py tunnel) and, with --demo, the
// ATTRACT DEMO recording whose move stream is fed to the junction logic.
// In every tick in which Pac-Man acts: poll the input, junctionStep, stepPacman; then position, junction numbers, direction fields, animation
// counter $66 and the cell counters $6A-$6D must equal the oracle's values at the next tick.
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "../src/core/input.h"
#include "../src/core/junction.h"
#include "../src/core/pacstep.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;
using namespace pacman::core;

namespace {

GameState pac(int x, int y, int dir) {
    GameState s;
    s.pacX() = static_cast<std::uint8_t>(x);
    s.pacY() = static_cast<std::uint8_t>(y);
    s.pacDirection() = static_cast<std::uint8_t>(dir);
    return s;
}

struct Totals {
    std::size_t acting = 0, blocked = 0, tunnel = 0, items = 0, restarts = 0, turns = 0, wraps = 0, streamReads = 0, streamMismatches = 0, pointerMismatches = 0;
};

// Runs the piece over one recording; returns the number of mismatches. demoStream: the 3-byte records of tools/record_special.py demo (or empty).
std::size_t compareRecording(const TickTrace& tr, const JunctionGraph& graph, const datapack::Pack& pack, const std::vector<std::uint8_t>& demoStream, Totals& tot,
                             std::size_t& firstBad) {
    DemoStream demo(pack);   // the stream pointer and its bytes come from the data pack; checked against the recorded pointer at EVERY tick of a demo recording
    std::multimap<std::uint32_t, std::uint32_t> keysAt;
    for (const auto& in : tr.inputs()) keysAt.emplace(in.tick, in.key);
    InputPoller poller;
    KeyConfig keys;
    std::size_t mismatches = 0;
    std::multimap<std::uint32_t, TickPoke> pokesAt;
    for (const auto& p : tr.pokes()) pokesAt.emplace(p.tick, p);
    for (std::size_t k = 0; k + 1 < tr.ticks(); ++k) {
        const auto range = keysAt.equal_range(static_cast<std::uint32_t>(k));
        for (auto it = range.first; it != range.second; ++it) poller.pushKey(static_cast<std::uint8_t>(it->second));
        GameState s = fromOracleLayout(tr.state(k));
        if (!demoStream.empty() && demo.pointer() != static_cast<std::uint16_t>(demoStream[k * 3] | (demoStream[k * 3 + 1] << 8))) ++tot.pointerMismatches;
        // the scenario's pokes of this tick are applied right after the keys, before the tick runs (docs/GOLDEN_TRACES.md); only zero-page ones matter here
        const auto pr = pokesAt.equal_range(static_cast<std::uint32_t>(k));
        for (auto it = pr.first; it != pr.second; ++it)
            if (it->second.address >= 0x52 && it->second.address <= 0x6F) s.zp(static_cast<std::uint16_t>(it->second.address)) = static_cast<std::uint8_t>(it->second.value);
        if (demoStream.empty()) poller.poll(s, keys, {});   // (in the demo no key is ever pressed)
        if (tr.at(k, 0x59) != 1) continue;                   // Pac-Man does not act in this tick
        const bool restart = tr.at(k + 1, 0x60) == 0x8B && tr.at(k + 1, 0x61) == 0x8F && !(tr.at(k, 0x60) == 0x8B && tr.at(k, 0x61) == 0x8F);
        const std::uint8_t oldDir = s.pacDirection(), oldX = s.pacX();
        JunctionOutcome outcome;
        if (demoStream.empty()) {
            outcome = junctionStep(s, graph);
        } else {
            const std::uint8_t recordedByte = demoStream[k * 3 + 2];
            std::size_t calls = 0;
            outcome = junctionStep(s, graph, StreamByteSource([&] {
                ++calls;
                const std::uint8_t b = demo.next();
                if (b != recordedByte) ++tot.streamMismatches;   // the byte the original loaded into $63 at this tick
                return b;
            }));
            tot.streamReads += calls;
        }
        const auto test = stepPacman(s, outcome);
        if (restart) {
            ++tot.restarts;
            continue;
        }
        // the position jumps by about 160 units when the tunnel wraps
        if (outcome == JunctionOutcome::Tunnel && (s.pacX() > oldX ? s.pacX() - oldX : oldX - s.pacX()) > 10) ++tot.wraps;
        ++tot.acting;
        tot.blocked += outcome == JunctionOutcome::Blocked;
        tot.tunnel += outcome == JunctionOutcome::Tunnel;
        tot.items += test.has_value();
        tot.turns += s.pacDirection() != oldDir;
        bool ok = true;
        for (int a : {0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x69, 0x6A, 0x6B, 0x6C, 0x6D}) ok = ok && s.zp(static_cast<std::uint16_t>(a)) == tr.at(k + 1, static_cast<std::uint16_t>(a));
        if (!ok && mismatches++ == 0) firstBad = k;
    }
    return mismatches;
}

std::vector<std::uint8_t> readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

datapack::Pack& thePack() {
    static datapack::Pack pack;
    return pack;
}

bool loadGraph(JunctionGraph& graph) {
    const auto img = readAll(testfw::config().image);
    const auto built = datapack::buildPack(img, verify::referenceExpected(), datapack::dataManifest());
    if (!built.ok) return false;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    if (!loaded.ok) return false;
    thePack() = loaded.pack;
    return graph.load(thePack());
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

}  // namespace

TEST(pacstep_moves_two_units_and_counts_cells_synthetic) {
    GameState r = pac(100, 50, 2);                           // right
    r.zp(0x6A) = 4;
    r.zp(0x6C) = 0;
    auto t = stepPacman(r, JunctionOutcome::Move);
    CHECK(r.pacX() == 102 && r.pacY() == 50 && r.zp(0x6C) == 2 && r.zp(0x6A) == 4);
    CHECK(t && t->column == 5 && t->row == r.zp(0x6B));      // 2 units into the cell: the NEXT cell is tested
    t = stepPacman(r, JunctionOutcome::Move);
    CHECK(r.zp(0x6C) == 4 && !t);
    t = stepPacman(r, JunctionOutcome::Move);
    CHECK(r.zp(0x6C) == 0 && r.zp(0x6A) == 5 && !t);         // wrapped into the next cell
    GameState l = pac(100, 50, 3);                           // left
    l.zp(0x6A) = 5;
    l.zp(0x6C) = 0;
    t = stepPacman(l, JunctionOutcome::Move);
    CHECK(l.pacX() == 98 && l.zp(0x6C) == 4 && l.zp(0x6A) == 4);
    CHECK(t && t->column == 4);                              // after the wrap the test is at the new cell
    t = stepPacman(l, JunctionOutcome::Move);
    CHECK(l.zp(0x6C) == 2 && !t);
    GameState d = pac(100, 50, 0);                           // down
    d.zp(0x6B) = 7;
    d.zp(0x6D) = 2;
    t = stepPacman(d, JunctionOutcome::Move);
    CHECK(d.pacY() == 52 && d.zp(0x6D) == 4 && !t);
    t = stepPacman(d, JunctionOutcome::Move);
    CHECK(d.zp(0x6D) == 0 && d.zp(0x6B) == 8);
    GameState u = pac(100, 50, 1);                           // up
    u.zp(0x6B) = 8;
    u.zp(0x6D) = 0;
    t = stepPacman(u, JunctionOutcome::Move);
    CHECK(u.pacY() == 48 && u.zp(0x6D) == 4 && u.zp(0x6B) == 7 && t && t->row == 7);
    CHECK(u.zp(0x66) == 1);                                  // animation counter
    GameState wrap = pac(0, 0, 2);
    wrap.zp(0x66) = 15;
    stepPacman(wrap, JunctionOutcome::Move);
    CHECK(wrap.zp(0x66) == 0);                               // only four bits are kept
}

TEST(pacstep_blocked_does_not_move_and_tunnel_wraps_synthetic) {
    GameState b = pac(100, 50, 2);
    CHECK(!stepPacman(b, JunctionOutcome::Blocked) && b.pacX() == 100 && b.zp(0x6C) == 0);
    GameState left = pac(0x3D, 0x59, 3);                     // tunnel from junction 26 heading left, two units from the left edge
    left.pacJunction() = 26;
    left.pacNextJunction() = 29;
    left.zp(0x69) = 1;
    stepPacman(left, JunctionOutcome::Tunnel);
    CHECK(left.pacX() == 0xDB);   // 0x3D - 2 = 0x3B is the edge: reappears at the right
    CHECK(left.pacJunction() == 29 && left.pacNextJunction() == 26);
    CHECK(left.zp(0x6A) == 0x1B && left.zp(0x6C) == 4);       // the wrap sets $6A = $1C, $6C = 0, then the counters step left in the same tick
    GameState right = pac(0xDB, 0x59, 2);                    // tunnel from junction 29 heading right: wraps at 0xDD
    right.pacJunction() = 29;
    right.pacNextJunction() = 26;
    stepPacman(right, JunctionOutcome::Tunnel);
    CHECK(right.pacX() == 0x3D && right.pacJunction() == 26 && right.pacNextJunction() == 0x1D && right.zp(0x6A) == 0);
    GameState inside = pac(0xB0, 0x59, 3);                   // at 29 heading left: plain steps through the tunnel row
    inside.pacJunction() = 29;
    stepPacman(inside, JunctionOutcome::Tunnel);
    CHECK(inside.pacX() == 0xAE);
}

TEST(pacstep_against_the_oracle_recordings) {
    if (testfw::config().ticks.empty() || testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --ticks and --image)\n");
        return;
    }
    JunctionGraph graph;
    CHECK(loadGraph(graph));
    Totals tot;
    for (const auto& path : split(testfw::config().ticks)) {
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), path + ": " + why);
        if (!tr.ok()) continue;
        std::size_t firstBad = 0;
        const std::size_t bad = compareRecording(tr, graph, thePack(), {}, tot, firstBad);
        std::printf("    %s: mismatches %zu%s\n", path.c_str(), bad, bad ? (" (first at tick " + std::to_string(firstBad) + ")").c_str() : "");
        CHECK(bad == 0);
    }
    std::printf("    %zu acting ticks (%zu turns, %zu blocked, %zu tunnel of which %zu wraps, %zu item-test points), %zu restarts skipped\n", tot.acting, tot.turns, tot.blocked,
                tot.tunnel, tot.wraps, tot.items, tot.restarts);
    CHECK(tot.acting > 5000 && tot.items > 500);
}

TEST(pacstep_tunnel_recording_has_many_tunnel_ticks) {
    // needs the T00 recording among the --ticks files; checks that the weak spot (only 8 tunnel ticks in the golden scenarios) is now covered
    if (testfw::config().ticks.empty() || testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --ticks and --image)\n");
        return;
    }
    JunctionGraph graph;
    CHECK(loadGraph(graph));
    Totals tot;
    bool found = false;
    for (const auto& path : split(testfw::config().ticks)) {
        if (path.find("T00") == std::string::npos) continue;
        found = true;
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), why);
        if (!tr.ok()) continue;
        std::size_t firstBad = 0;
        CHECK(compareRecording(tr, graph, thePack(), {}, tot, firstBad) == 0);
    }
    if (!found) {
        std::printf("    SKIPPED (no T00 file given)\n");
        return;
    }
    std::printf("    tunnel recording: %zu tunnel ticks, %zu edge wraps\n", tot.tunnel, tot.wraps);
    CHECK(tot.tunnel > 100 && tot.wraps >= 5);
}

TEST(pacstep_attract_demo_recording) {
    if (testfw::config().demo.empty() || testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --demo and --image)\n");
        return;
    }
    JunctionGraph graph;
    CHECK(loadGraph(graph));
    std::string why;
    const auto tr = TickTrace::load(testfw::config().demo, why);
    CHECK_MSG(tr.ok(), why);
    if (!tr.ok()) return;
    const std::string& p = testfw::config().demo;
    const std::size_t slash = p.find_last_of("/\\");
    const auto stream = readAll((slash == std::string::npos ? std::string() : p.substr(0, slash + 1)) + "demo_stream.bin");
    CHECK(stream.size() == tr.ticks() * 3);
    CHECK(tr.at(0, 0xE8) != 0);                              // really the demo
    Totals tot;
    std::size_t firstBad = 0;
    const std::size_t bad = compareRecording(tr, graph, thePack(), stream, tot, firstBad);
    std::printf("    demo: %zu ticks, %zu acting ticks (%zu turns, %zu blocked, %zu tunnel), mismatches %zu%s\n", tr.ticks(), tot.acting, tot.turns, tot.blocked, tot.tunnel, bad,
                bad ? (" (first at tick " + std::to_string(firstBad) + ")").c_str() : "");
    std::printf("    demo stream: %zu pointer advances, %zu byte mismatches, %zu pointer mismatches over %zu ticks (start $%04X)\n", tot.streamReads, tot.streamMismatches,
                tot.pointerMismatches, tr.ticks(), DemoStream::kStart);
    CHECK(bad == 0);
    CHECK(tot.streamMismatches == 0 && tot.pointerMismatches == 0 && tot.streamReads >= 25);
    CHECK(tot.acting > 300 && tot.turns >= 15);
}
