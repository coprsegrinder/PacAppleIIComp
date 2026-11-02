// Tests of Pac-Man's step timer (src/core/steptimer): synthetic cases from the code reading, and a per-tick comparison of $59 with the oracle
// recordings (needs --ticks and --image: the three tables come from the data pack).
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

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

GameState at(int level, int counter) {
    GameState s;
    s.level() = static_cast<std::uint8_t>(level);
    s.pacStepCounter() = static_cast<std::uint8_t>(counter);
    return s;
}

}  // namespace

TEST(steptimer_counts_down_and_reloads_from_the_level_table) {
    const StepTables t = tables();
    StepTimer timer;
    GameState s = at(1, 3);
    CHECK(!timer.tick(s, t) && s.pacStepCounter() == 2);
    CHECK(!timer.tick(s, t) && s.pacStepCounter() == 1);
    CHECK(timer.tick(s, t) && s.pacStepCounter() == 9);   // level 1: base 9
    GameState s5 = at(5, 1);
    CHECK(timer.tick(s5, t) && s5.pacStepCounter() == 6);
    GameState s99 = at(200, 1);                           // levels above 15 use the last entry
    CHECK(timer.tick(s99, t) && s99.pacStepCounter() == 4);
}

TEST(steptimer_ghost_states_change_the_reload) {
    const StepTables t = tables();
    StepTimer timer;
    GameState s = at(1, 1);
    s.ghosts[1].b[8] = 1;                                 // ghost 2 frightened: +1
    CHECK(timer.tick(s, t) && s.pacStepCounter() == 10);
    s = at(1, 1);
    s.ghosts[1].b[8] = 1;
    s.ghosts[3].b[8] = 1;                                 // ghost 4 frightened: +1 more (observed value 11 in PACMAN_RULES.md)
    CHECK(timer.tick(s, t) && s.pacStepCounter() == 11);
    s = at(1, 1);
    s.ghosts[0].b[9] = 1;                                 // ghost 1 eyes: -1
    s.ghosts[2].b[9] = 1;                                 // ghost 3 eyes: -1
    CHECK(timer.tick(s, t) && s.pacStepCounter() == 7);
    s = at(1, 1);
    s.ghosts[0].b[8] = 1;                                 // ghost 1 frightened and ghost 2 eyes do NOT count
    s.ghosts[1].b[9] = 1;
    s.ghosts[2].b[8] = 1;
    s.ghosts[3].b[9] = 1;
    CHECK(timer.tick(s, t) && s.pacStepCounter() == 9);
}

TEST(steptimer_eating_delays_pacman_now_and_for_later_reloads) {
    const StepTables t = tables();
    StepTimer timer;
    GameState s = at(1, 1);
    CHECK(timer.tick(s, t) && s.pacStepCounter() == 9);
    timer.itemEaten(s, t);                                // level 1: +2 now, F3 := 2
    CHECK(s.pacStepCounter() == 11 && timer.f3() == 2);
    s.pacStepCounter() = 1;
    CHECK(timer.tick(s, t) && s.pacStepCounter() == 10 && timer.f3() == 1);  // 9 + 1 while F3 > 0
    s.pacStepCounter() = 1;
    CHECK(timer.tick(s, t) && s.pacStepCounter() == 10 && timer.f3() == 0);
    s.pacStepCounter() = 1;
    CHECK(timer.tick(s, t) && s.pacStepCounter() == 9 && timer.f3() == 0);   // back to normal
    GameState late = at(12, 1);                           // level 12: no immediate penalty, F3 start 1
    timer.itemEaten(late, t);
    CHECK(late.pacStepCounter() == 1 && timer.f3() == 1);
    GameState last = at(30, 1);                           // level class 15: F3 start 0
    timer.itemEaten(last, t);
    CHECK(timer.f3() == 0);
}

TEST(steptimer_against_the_oracle_recordings) {
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
    StepTables tab;
    CHECK(loadStepTables(loaded.pack, tab));
    CHECK(tab.base[1] == 9 && tab.penalty[1] == 2 && tab.f3Start[1] == 2);  // level 1 values from the pack equal the documented ones

    std::size_t decisions = 0, reloads = 0, eats = 0, resets = 0;
    for (std::size_t pos = 0; pos <= testfw::config().ticks.size();) {
        const std::size_t comma = testfw::config().ticks.find(',', pos);
        const std::size_t end = comma == std::string::npos ? testfw::config().ticks.size() : comma;
        const std::string path = testfw::config().ticks.substr(pos, end - pos);
        pos = end + 1;
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), path + ": " + why);
        if (!tr.ok()) continue;
        StepTimer timer;
        std::size_t mismatches = 0, firstBad = 0;
        for (std::size_t k = 0; k + 1 < tr.ticks(); ++k) {
            GameState s = fromOracleLayout(tr.state(k));
            const std::uint8_t expectedNext = tr.at(k + 1, 0x59);
            const bool acted = timer.tick(s, tab);
            // an item was eaten in this tick if the counter went down; the LAST item of a level also resets the counter (to 248) and raises the level
            const bool lastItem = tr.at(k, 0x56) == 1 && tr.at(k + 1, 0x55) == tr.at(k, 0x55) + 1;
            const bool eaten = tr.at(k + 1, 0x56) < tr.at(k, 0x56) || lastItem;
            if (acted && eaten) {
                timer.itemEaten(s, tab);
                ++eats;
            }
            if (acted) ++reloads;
            // a new life or level sets the timer directly (other routines); recognise it by Pac-Man being put back to its start, not by the value
            const bool restarted = tr.at(k + 1, 0x60) == 0x8B && tr.at(k + 1, 0x61) == 0x8F && !(tr.at(k, 0x60) == 0x8B && tr.at(k, 0x61) == 0x8F);
            if (restarted) {
                ++resets;
                continue;
            }
            ++decisions;
            if (s.pacStepCounter() != expectedNext) {
                if (mismatches++ == 0) firstBad = k;
            }
        }
        std::printf("    %s: mismatches %zu%s\n", path.c_str(), mismatches, mismatches ? (" (first at tick " + std::to_string(firstBad) + ")").c_str() : "");
        CHECK(mismatches == 0);
    }
    std::printf("    %zu ticks compared, %zu reloads, %zu items eaten, %zu restarts skipped\n", decisions, reloads, eats, resets);
    CHECK(decisions > 20000 && reloads > 2000 && eats > 100);
}
