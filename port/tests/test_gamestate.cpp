// Tests of the game state (src/core/gamestate): the layout round trip on EVERY recorded tick, named fields, and the new-game state against tick 0 of
// the golden scenarios. The trace tests need --ticks; the new-game test needs --image as well (the map comes from the data pack).
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/core/gamestate.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;
using core::GameState;
using core::TickTrace;

namespace {

std::vector<std::string> splitList(const std::string& list) {
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

TEST(gamestate_layout_and_named_fields_synthetic) {
    std::array<std::uint8_t, core::kStateBytes> raw{};
    for (std::size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<std::uint8_t>(i * 31 + 5);
    const GameState s = core::fromOracleLayout(raw);
    CHECK(core::toOracleLayout(s) == raw);                  // exact round trip of arbitrary bytes
    CHECK(s.zp(0x52) == raw[0] && s.zp(0x6F) == raw[29]);
    CHECK(s.mode78 == raw[30] && s.mode79 == raw[31] && s.demo == raw[32] && s.cornerPhase == raw[33]);
    CHECK(s.ghosts[0].x() == raw[34] && s.ghosts[0].y() == raw[35] && s.ghosts[3].b[19] == raw[113]);
    CHECK(s.map[0] == raw[114] && s.map[895] == raw[1009]);
    CHECK(s.level() == raw[0x55 - 0x52] && s.pacX() == raw[0x60 - 0x52] && s.pacY() == raw[0x61 - 0x52]);
    // the wrong length gives an all-zero state, not garbage
    const auto bad = core::fromOracleLayout(std::span<const std::uint8_t>(raw).first(100));
    const std::array<std::uint8_t, core::kStateBytes> zeros{};
    CHECK(core::toOracleLayout(bad) == zeros);
    // score = three BCD bytes, low first
    GameState sc;
    sc.zp(0x52) = 0x50;
    sc.zp(0x53) = 0x99;
    sc.zp(0x54) = 0x01;
    CHECK(sc.score() == 50u + 9900u + 10000u);  // 10,000 + 9,900 + 50 = 19,950
}

TEST(gamestate_round_trip_on_every_recorded_tick) {
    if (testfw::config().ticks.empty()) {
        std::printf("    SKIPPED (needs --ticks)\n");
        return;
    }
    std::size_t total = 0;
    for (const auto& path : splitList(testfw::config().ticks)) {
        std::string why;
        const auto t = TickTrace::load(path, why);
        CHECK_MSG(t.ok(), path + ": " + why);
        if (!t.ok()) continue;
        std::size_t bad = 0;
        for (std::size_t k = 0; k < t.ticks(); ++k) {
            const auto s = t.state(k);
            const auto back = core::toOracleLayout(core::fromOracleLayout(s));
            if (std::memcmp(back.data(), s.data(), s.size()) != 0) ++bad;
        }
        CHECK(bad == 0);
        total += t.ticks();
    }
    std::printf("    %zu ticks round-tripped\n", total);
    CHECK(total > 1000);
}

TEST(gamestate_new_game_equals_tick_zero_of_every_scenario) {
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
    GameState fresh;
    CHECK(core::newGameState(loaded.pack, fresh));
    const auto mine = core::toOracleLayout(fresh);
    int scenarios = 0;
    for (const auto& path : splitList(testfw::config().ticks)) {
        std::string why;
        const auto t = TickTrace::load(path, why);
        if (!t.ok()) continue;
        ++scenarios;
        const auto zero = t.state(0);
        CHECK(std::memcmp(mine.data(), zero.data(), zero.size()) == 0);
    }
    CHECK(scenarios >= 1);
    // a few facts that the plan relies on
    CHECK(fresh.lives() == 0x83 && fresh.level() == 1 && fresh.dotsLeft() == 248 && fresh.score() == 0);
    int dots = 0, energizers = 0;
    for (auto c : fresh.map) {
        dots += c == 1;
        energizers += c == 2;
    }
    CHECK(dots == 244 && energizers == 4);
    // a pack without the map is refused and leaves the state alone
    datapack::Pack empty;
    GameState keep = fresh;
    CHECK(!core::newGameState(empty, keep));
    CHECK(core::toOracleLayout(keep) == mine);
}
