// Tests of the start of a life (src/core/lifestart): synthetic checks, the new game built from the pack tables against tick 0 of every scenario, and the
// first tick after every catch in the recordings (--ticks --image): pacmanCaught + save + startOfLife must give the oracle's state field for field.
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/core/lifestart.h"
#include "../src/core/lives.h"
#include "../src/core/steptimer.h"
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

StartTables fakeTables() {
    StartTables t;
    for (std::size_t i = 0; i < 14; ++i) t.pacman[i] = static_cast<std::uint8_t>(0x10 + i);
    for (std::size_t i = 0; i < 80; ++i) t.ghosts[i] = static_cast<std::uint8_t>(0x80 + i);
    for (std::size_t i = 0; i < 16; ++i) {
        t.house2[i] = static_cast<std::uint8_t>(100 - i);
        t.house3[i] = static_cast<std::uint8_t>(50 - i);
        t.house4[i] = static_cast<std::uint8_t>(150 - i);
    }
    return t;
}

}  // namespace

TEST(lifestart_synthetic) {
    const StartTables t = fakeTables();
    GameState s;
    s.level() = 3;
    s.zp(0x58) = 0x82;
    s.zp(0x52) = 0x34;
    s.pacX() = 1;
    s.mode78 = 9;
    s.demo = 0;
    s.map[5] = 2;
    const PlayerSave save = savePlayer(s);
    s.zp(0x59) = 77;           // changes after the save are lost when the save is loaded back
    s.zp(0x52) = 0x99;
    startOfLife(s, t, save);
    CHECK(s.zp(0x52) == 0x34 && s.zp(0x58) == 0x82 && s.level() == 3 && s.zp(0x59) == save.bytes[7]);   // $52-$59 come from the save
    CHECK(s.pacX() == 0x10 && s.zp(0x6D) == 0x1D);                                                          // $60-$6D from the table
    CHECK(s.ghosts[0].b[0] == 0x80 && s.ghosts[3].b[19] == 0x80 + 79);
    CHECK(s.ghosts[1].b[18] == 100 - 3 && s.ghosts[2].b[18] == 50 - 3 && s.ghosts[3].b[18] == 150 - 3);   // level 3 -> index 3
    for (std::uint16_t a = 0x5A; a <= 0x5F; ++a) CHECK(s.zp(a) == 0);
    CHECK(s.cornerPhase == 1 && s.mode78 == 9 && s.map[5] == 2);                                           // untouched: working-slot flags, the map
    GameState hi;
    hi.level() = 200;                                                                                       // levels above 15 use the last table entry
    startOfLife(hi, t, savePlayer(hi));
    CHECK(hi.ghosts[1].b[18] == 100 - 15);
}

TEST(lifestart_against_the_oracle_recordings) {
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
    StartTables tab;
    CHECK(loadStartTables(loaded.pack, tab));
    StepTables steps;
    CHECK(loadStepTables(loaded.pack, steps));
    // 1. the new game: the pristine map, the new-game constants and startOfLife equal tick 0 of every scenario (except $78/$79, set elsewhere: compared below)
    GameState fresh;
    const auto map = loaded.pack.find(0x65BA, 896);
    CHECK(!map.empty());
    std::copy(map.begin(), map.end(), fresh.map.begin());
    PlayerSave save;
    save.bytes = {0x00, 0x00, 0x00, 0x01, 0xF8, 0x40, 0x83, 0x01};   // $531E-$532D: score 0, level 1, items $F8, fruit counter $40, lives $83, step counter 1
    startOfLife(fresh, tab, save);
    std::size_t files = 0, catches = 0, bad = 0;
    for (const auto& path : split(testfw::config().ticks)) {
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), path + ": " + why);
        if (!tr.ok()) continue;
        if (path.find("ticks_A.bin") != std::string::npos || path.find("ticks_B.bin") != std::string::npos) {
            const GameState zero = fromOracleLayout(tr.state(0));
            GameState mine = fresh;
            mine.mode78 = zero.mode78;   // $78/$79 are working-slot leftovers of the ghost set-up, not from the start-of-life tables
            mine.mode79 = zero.mode79;
            const auto a = toOracleLayout(mine), b = toOracleLayout(zero);
            CHECK(std::memcmp(a.data(), b.data(), a.size()) == 0);
            ++files;
        }
        // 2. every catch: lives lost in tick k -> the state at tick k+1 is the restarted life
        for (std::size_t k = 0; k + 1 < tr.ticks(); ++k) {
            const GameState a = fromOracleLayout(tr.state(k));
            const GameState b = fromOracleLayout(tr.state(k + 1));
            if (!((b.lives() & 0x7F) < (a.lives() & 0x7F) && a.lives() != 0)) continue;
            ++catches;
            GameState mine = a;
            // the catch happens late in the tick: the step timer has counted down (and possibly reloaded) before the player's bytes are saved
            StepTimer timer;
            timer.tick(mine, steps);
            pacmanCaught(mine);
            const PlayerSave kept = savePlayer(mine);   // $8C1A: JSR $5394, $8C1D: JSR $53BE -- the state is saved after the life was taken
            startOfLife(mine, tab, kept);
            mine.mode78 = b.mode78;                     // working-slot flags: not set by this routine
            mine.mode79 = b.mode79;
            const auto x = toOracleLayout(mine), y = toOracleLayout(b);
            std::size_t diff = 0;
            for (std::size_t i = 0; i < x.size(); ++i) diff += x[i] != y[i];
            if (diff != 0) {
                ++bad;
                for (std::size_t i = 0; i < x.size() && i < 114; ++i)
                    if (x[i] != y[i]) {
                        std::printf("    %s tick %zu: byte %zu (state offset) mine %02X oracle %02X\n", path.c_str(), k, i, x[i], y[i]);
                        break;
                    }
            }
        }
    }
    std::printf("    new game equals tick 0 in %zu scenario files; %zu restarts after a catch compared, %zu differ\n", files, catches, bad);
    CHECK(files >= 1 && catches >= 8 && bad == 0);
}
