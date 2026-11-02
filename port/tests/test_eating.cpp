// Tests of eating (src/core/eating): synthetic cases and a per-tick comparison with all oracle recordings (--ticks --image, optionally --demo): a complete
// acting tick is run (input poll, step timer, junction logic, step, eating) and the fields eating owns must equal the oracle's at the next tick:
// the score $52-$54, the counters $56 and $57, Pac-Man's delay $59, and the cleared map cell. Energizer ticks are compared on the same fields
// (the ghost effects of an energizer are not rebuilt and touch other fields only).
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "../src/core/eating.h"
#include "../src/core/input.h"
#include "../src/core/junction.h"
#include "../src/core/pacstep.h"
#include "../src/core/steptimer.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;
using namespace pacman::core;

namespace {

StepTables tables() {
    StepTables t;
    const std::uint8_t base[16] = {9, 9, 8, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 4, 4, 4};
    const std::uint8_t pen[16] = {2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
    const std::uint8_t f3[16] = {2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0};
    for (int i = 0; i < 16; ++i) {
        t.base[static_cast<std::size_t>(i)] = base[i];
        t.penalty[static_cast<std::size_t>(i)] = pen[i];
        t.f3Start[static_cast<std::size_t>(i)] = f3[i];
    }
    return t;
}

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

struct Totals {
    std::size_t acting = 0, dots = 0, energizers = 0, demoItems = 0, restarts = 0, levelEnds = 0, outsideMap = 0, ghostScore = 0, fruitDue = 0;
};

std::size_t run(const TickTrace& tr, const JunctionGraph& graph, const datapack::Pack& pack, const StepTables& tab, bool demo, Totals& tot, std::size_t& firstBad) {
    std::multimap<std::uint32_t, std::uint32_t> keysAt;
    for (const auto& in : tr.inputs()) keysAt.emplace(in.tick, in.key);
    std::multimap<std::uint32_t, TickPoke> pokesAt;
    for (const auto& p : tr.pokes()) pokesAt.emplace(p.tick, p);
    InputPoller poller;
    KeyConfig keys;
    StepTimer timer;
    DemoStream stream(pack);
    std::size_t mismatches = 0;
    for (std::size_t k = 0; k + 1 < tr.ticks(); ++k) {
        const auto kr = keysAt.equal_range(static_cast<std::uint32_t>(k));
        for (auto it = kr.first; it != kr.second; ++it) poller.pushKey(static_cast<std::uint8_t>(it->second));
        GameState s = fromOracleLayout(tr.state(k));
        const auto pr = pokesAt.equal_range(static_cast<std::uint32_t>(k));
        for (auto it = pr.first; it != pr.second; ++it) {
            const auto a = it->second.address;
            if (a >= 0x52 && a <= 0x6F) s.zp(static_cast<std::uint16_t>(a)) = static_cast<std::uint8_t>(it->second.value);
            if (a >= 0x623A && a <= 0x65B9) s.map[a - 0x623A] = static_cast<std::uint8_t>(it->second.value);
        }
        if (!demo) poller.poll(s, keys, {});
        const bool acted = timer.tick(s, tab);
        const bool restart = tr.at(k + 1, 0x60) == 0x8B && tr.at(k + 1, 0x61) == 0x8F && !(tr.at(k, 0x60) == 0x8B && tr.at(k, 0x61) == 0x8F);
        if (!acted) continue;
        const auto outcome = junctionStep(s, graph, StreamByteSource([&] { return stream.next(); }));
        const auto test = stepPacman(s, outcome);
        EatResult r;
        if (test) r = eatAt(s, *test, timer, tab);
        if (restart) {
            ++tot.restarts;
            tot.levelEnds += r.levelComplete;
            continue;
        }
        if (r.outsideMap) {   // poked position with stale cell counters (scenario D): the original ate from the master copy behind the map; not modelled
            ++tot.outsideMap;
            if (tr.at(k + 1, 0x56) < tr.at(k, 0x56)) timer.itemEaten(s, tab);   // the original did eat (from the master copy): keep the delay counter F3 in step
            continue;
        }
        // a ghost eaten in the same tick (200, 400, 800, 1600 points by the ghost logic, M5) adds to the score: exempt, counted
        const long scoreGap = static_cast<long>(fromOracleLayout(tr.state(k + 1)).score()) - static_cast<long>(s.score());
        // a bonus fruit eaten in the same tick (points 100, 300, 500, 700, 1000, 2000, 3000, 5000: another routine, $6CB2) is exempt too
        const bool ghostScore = !r.ate && (scoreGap == 200 || scoreGap == 400 || scoreGap == 800 || scoreGap == 1600 || scoreGap == 100 || scoreGap == 300 || scoreGap == 500 ||
                                           scoreGap == 700 || scoreGap == 1000 || scoreGap == 2000 || scoreGap == 3000 || scoreGap == 5000);
        if (ghostScore) {
            ++tot.ghostScore;
            continue;
        }
        ++tot.acting;
        if (r.ate) {
            if (s.demo != 0) ++tot.demoItems;
            else if (r.energizer) ++tot.energizers;
            else ++tot.dots;
        }
        // when the fruit counter $57 runs out the fruit routine ($6A7C, levels and fruit, M6) reloads it in the same pass: exempt, counted
        const bool fruitDue = r.ate && s.demo == 0 && s.zp(0x57) == 0;
        tot.fruitDue += fruitDue;
        bool ok = s.zp(0x59) == tr.at(k + 1, 0x59) && s.zp(0x56) == tr.at(k + 1, 0x56) && (fruitDue || s.zp(0x57) == tr.at(k + 1, 0x57)) && s.zp(0x58) == tr.at(k + 1, 0x58);   // $58: the extra life at 10,000 points is part of eating now
        for (std::uint16_t a : {std::uint16_t{0x52}, std::uint16_t{0x53}, std::uint16_t{0x54}}) ok = ok && s.zp(a) == tr.at(k + 1, a);
        if (r.ate && test) ok = ok && tr.at(k + 1, static_cast<std::uint16_t>(0x623A + test->column * 32 + test->row)) == 0;
        if (!ok && mismatches == 0)  // show the first difference in detail
            std::printf("    tick %zu: mine $59=%u $56=%u $57=%u score=%02X%02X%02X, oracle $59=%u $56=%u $57=%u score=%02X%02X%02X, item=%d energizer=%d\n", k, s.zp(0x59), s.zp(0x56), s.zp(0x57), s.zp(0x54), s.zp(0x53), s.zp(0x52), tr.at(k + 1, 0x59), tr.at(k + 1, 0x56), tr.at(k + 1, 0x57), tr.at(k + 1, 0x54), tr.at(k + 1, 0x53), tr.at(k + 1, 0x52), r.ate ? 1 : 0, r.energizer ? 1 : 0);
        if (!ok && mismatches++ == 0) firstBad = k;
    }
    return mismatches;
}

}  // namespace

TEST(eating_scores_and_counters_synthetic) {
    const StepTables tab = tables();
    StepTimer timer;
    GameState s;
    s.level() = 1;
    s.dotsLeft() = 248;
    s.fruitCounter() = 64;
    s.pacStepCounter() = 5;
    s.map[3 * 32 + 4] = 1;
    s.map[3 * 32 + 5] = 2;
    ItemTest dot{3, 4}, big{3, 5}, none{9, 9};
    CHECK(!eatAt(s, none, timer, tab).ate);
    auto r = eatAt(s, dot, timer, tab);
    CHECK(r.ate && !r.energizer && !r.levelComplete);
    CHECK(s.score() == 10 && s.dotsLeft() == 247 && s.fruitCounter() == 63 && s.map[3 * 32 + 4] == 0);
    CHECK(s.pacStepCounter() == 7 && timer.f3() == 2);       // level 1 delay of 2 and F3 := 2
    r = eatAt(s, big, timer, tab);
    CHECK(r.ate && r.energizer && s.score() == 60 && s.dotsLeft() == 246);
    CHECK(!eatAt(s, dot, timer, tab).ate);                   // the cell is empty now
    s.zp(0x52) = 0x95;                                       // BCD carry: 95 + 10 = 105
    s.map[1] = 1;
    eatAt(s, ItemTest{0, 1}, timer, tab);
    CHECK(s.zp(0x52) == 0x05 && s.zp(0x53) == 0x01);
    s.zp(0x52) = 0x95;
    s.zp(0x53) = 0x99;
    s.zp(0x54) = 0x00;
    s.map[2] = 1;
    eatAt(s, ItemTest{0, 2}, timer, tab);
    CHECK(s.zp(0x52) == 0x05 && s.zp(0x53) == 0x00 && s.zp(0x54) == 0x01);   // carry through to the third byte: 9,995 + 10 = 10,005
    s.dotsLeft() = 1;                                        // the last item
    s.map[3] = 1;
    CHECK(eatAt(s, ItemTest{0, 3}, timer, tab).levelComplete && s.dotsLeft() == 0);
}

TEST(eating_in_the_demo_gives_no_score) {
    const StepTables tab = tables();
    StepTimer timer;
    GameState s;
    s.demo = 1;
    s.level() = 1;
    s.dotsLeft() = 100;
    s.fruitCounter() = 50;
    s.map[7] = 2;
    const auto r = eatAt(s, ItemTest{0, 7}, timer, tab);
    CHECK(r.ate && r.energizer && s.score() == 0 && s.fruitCounter() == 50 && s.dotsLeft() == 99 && s.map[7] == 0);
}

TEST(eating_against_the_oracle_recordings) {
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
    JunctionGraph graph;
    CHECK(graph.load(loaded.pack));
    StepTables tab;
    CHECK(loadStepTables(loaded.pack, tab));
    Totals tot;
    std::vector<std::pair<std::string, bool>> files;
    for (const auto& p : split(testfw::config().ticks)) files.emplace_back(p, false);
    if (!testfw::config().demo.empty()) files.emplace_back(testfw::config().demo, true);
    for (const auto& [path, demo] : files) {
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), path + ": " + why);
        if (!tr.ok()) continue;
        std::size_t firstBad = 0;
        const std::size_t bad = run(tr, graph, loaded.pack, tab, demo, tot, firstBad);
        std::printf("    %s: mismatches %zu%s\n", path.c_str(), bad, bad ? (" (first at tick " + std::to_string(firstBad) + ")").c_str() : "");
        CHECK(bad == 0);
    }
    std::printf("    %zu acting ticks: %zu dots, %zu energizers, %zu demo items eaten, %zu restart ticks (%zu of them level ends) skipped\n", tot.acting, tot.dots, tot.energizers,
                tot.demoItems, tot.restarts, tot.levelEnds);
    CHECK(tot.dots > 300 && tot.energizers > 5);
}
