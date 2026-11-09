// TASK-033-4b: the attract title screen against the oracle. Needs --image and --titleevents (every routine entry of one title turn with its cycle) and, for the picture test,
// --titlesnaps (the screen memory every 50,000 cycles). Both files come from the original running on the oracle (tools/capture_events.py, tools/capture_title_snaps.py).
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/core/attract.h"
#include "../src/datapack/datapack.h"
#include "../src/verify/verify.h"
#include "test_framework.h"

using namespace pacman;

namespace {

std::vector<std::uint8_t> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool loadPack(datapack::Pack& pack) {
    const auto built = datapack::buildPack(readFile(testfw::config().image), verify::referenceExpected(), datapack::dataManifest());
    if (!built.ok) return false;
    const auto loaded = datapack::loadPack(built.bytes, verify::referenceExpected());
    if (!loaded.ok) return false;
    pack = loaded.pack;
    return true;
}

}  // namespace

TEST(attract_title_routine_entries_equal_the_oracle_to_the_cycle) {
    if (testfw::config().image.empty() || testfw::config().titleEvents.empty()) {
        std::printf("    SKIPPED (needs --image and --titleevents)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPack(pack));
    core::ScreenScript script;
    CHECK(core::buildTitleScript(pack, script, true));
    const auto f = readFile(testfw::config().titleEvents);
    CHECK(f.size() > 8 && std::memcmp(f.data(), "TEV1", 4) == 0);
    if (f.size() <= 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    CHECK(f.size() == 8 + static_cast<std::size_t>(count) * 28);
    // the oracle's list has one hline event at the start of every logo line, and the events of the whole turn up to the first call of the chase
    CHECK(script.trace.size() == count);
    std::size_t bad = 0;
    const std::size_t n = std::min<std::size_t>(count, script.trace.size());
    for (std::size_t i = 0; i < n; ++i) {
        std::int32_t v[7];
        std::memcpy(v, f.data() + 8 + i * 28, 28);
        const core::TraceEvent& e = script.trace[i];
        bool same = e.at == v[0] && e.kind == v[1];
        if (e.kind == 5) {
            same = same && e.p[0] == v[2];
        } else if (e.kind == 2) {
            same = same && (e.p[0] & 255) == v[2] && e.p[1] == v[3] && e.p[2] == v[4] && e.p[3] == v[5] && e.p[4] == v[6];
        } else if (e.kind != 4 && e.kind != 1) {
            for (int k = 0; k < 5; ++k) same = same && e.p[static_cast<std::size_t>(k)] == v[2 + k];
        }
        if (!same) {
            if (bad++ < 5) std::printf("    event %zu: mine t=%lld kind %d (%d %d %d %d %d) | oracle t=%d kind %d (%d %d %d %d %d)\n", i, static_cast<long long>(e.at), e.kind, e.p[0], e.p[1], e.p[2], e.p[3], e.p[4], v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
        }
    }
    std::printf("    %zu events of one title turn compared (clear, logo lines, blit rows, key polls, delay passes, images, dots, tiles): %zu differ; the script ends at cycle %lld\n", n, bad, static_cast<long long>(script.length));
    CHECK(bad == 0);
    CHECK(script.length == 12'250'257);   // the first call of the chase ($8712 at $46B5)
}

TEST(attract_chase_routine_entries_equal_the_oracle_to_the_cycle) {
    if (testfw::config().image.empty() || testfw::config().chaseEvents.empty()) {
        std::printf("    SKIPPED (needs --image and --chaseevents)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPack(pack));
    core::ScreenScript script;
    CHECK(core::buildAttractScript(pack, script, true));
    const auto f = readFile(testfw::config().chaseEvents);
    CHECK(f.size() > 8 && std::memcmp(f.data(), "TEV1", 4) == 0);
    if (f.size() <= 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    CHECK(f.size() == 8 + static_cast<std::size_t>(count) * 28);
    // the chase starts where the title script ends
    std::vector<core::TraceEvent> mine;
    for (const auto& e : script.trace)
        if (e.at >= 12'250'257 && e.at < 22'486'410) mine.push_back(e);
    std::vector<std::array<std::int32_t, 7>> theirs;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::array<std::int32_t, 7> v{};
        std::memcpy(v.data(), f.data() + 8 + static_cast<std::size_t>(i) * 28, 28);
        if (v[0] >= 12'250'257 && v[0] < 22'486'410) theirs.push_back(v);
    }
    CHECK(mine.size() == theirs.size());
    std::size_t bad = 0;
    const std::size_t n = std::min(mine.size(), theirs.size());
    for (std::size_t i = 0; i < n; ++i) {
        const core::TraceEvent& e = mine[i];
        const auto& v = theirs[i];
        bool same = e.at == v[0] && e.kind == v[1];
        if (e.kind == 5) {
            same = same && e.p[0] == v[2];
        } else if (e.kind != 4) {
            for (int k = 0; k < 5; ++k) same = same && e.p[static_cast<std::size_t>(k)] == v[2 + static_cast<std::size_t>(k)];
        }
        if (!same && bad++ < 5) std::printf("    event %zu: mine t=%lld kind %d (%d %d %d %d %d) | oracle t=%d kind %d (%d %d %d %d %d)\n", i, static_cast<long long>(e.at), e.kind, e.p[0], e.p[1], e.p[2], e.p[3], e.p[4], v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
    }
    std::printf("    %zu events of the chase compared (erasures, Pac-Man and ghost images, blit rows, key polls, delay passes, energizer tiles, the four ghost-eaten animations): %zu differ; the chase ends at cycle %lld (the demo game's set-up starts)\n", n, bad, static_cast<long long>(22'486'410));
    CHECK(bad == 0);
}

TEST(attract_demo_setup_routine_entries_equal_the_oracle_to_the_cycle) {
    if (testfw::config().image.empty() || testfw::config().setupEvents.empty()) {
        std::printf("    SKIPPED (needs --image and --setupevents)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPack(pack));
    core::ScreenScript script;
    CHECK(core::buildAttractScript(pack, script, true));
    const auto f = readFile(testfw::config().setupEvents);
    CHECK(f.size() > 8 && std::memcmp(f.data(), "TEV1", 4) == 0);
    if (f.size() <= 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    CHECK(f.size() == 8 + static_cast<std::size_t>(count) * 28);
    std::vector<core::TraceEvent> mine;
    for (const auto& e : script.trace)
        if (e.at >= 22'486'410) mine.push_back(e);
    std::vector<std::array<std::int32_t, 7>> theirs;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::array<std::int32_t, 7> v{};
        std::memcpy(v.data(), f.data() + 8 + static_cast<std::size_t>(i) * 28, 28);
        // the first tick (cycle 23,955,112 on) belongs to the game itself
        if (v[0] >= 22'486'410 && v[0] < script.length) theirs.push_back(v);
    }
    CHECK(mine.size() == theirs.size());
    std::size_t bad = 0;
    const std::size_t n = std::min(mine.size(), theirs.size());
    for (std::size_t i = 0; i < n; ++i) {
        const core::TraceEvent& e = mine[i];
        const auto& v = theirs[i];
        bool same = e.at == v[0] && e.kind == v[1];
        if (e.kind == 5) {
            same = same && e.p[0] == v[2];
        } else if (e.kind == 2) {
            same = same && (e.p[0] & 255) == v[2] && e.p[1] == v[3] && e.p[2] == v[4] && e.p[3] == v[5] && e.p[4] == v[6];
        } else if (e.kind != 4 && e.kind != 1) {
            for (int k = 0; k < 5; ++k) same = same && e.p[static_cast<std::size_t>(k)] == v[2 + static_cast<std::size_t>(k)];
        }
        if (!same && bad++ < 5) std::printf("    event %zu: mine t=%lld kind %d (%d %d %d %d %d) | oracle t=%d kind %d (%d %d %d %d %d)\n", i, static_cast<long long>(e.at), e.kind, e.p[0], e.p[1], e.p[2], e.p[3], e.p[4], v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
    }
    std::printf("    %zu events of the attract game's set-up compared (page clear, 74 lines, 70 verticals, 244 dots and tiles, labels, scores, the fruit, the start image, GAME OVER, the 1,020 delay passes): %zu differ; the first tick starts at cycle %lld\n", n, bad, static_cast<long long>(script.length));
    CHECK(bad == 0);
    CHECK(script.length == 23'955'112);
}

TEST(attract_demo_death_equals_the_oracle_to_the_cycle) {
    if (testfw::config().image.empty() || testfw::config().endEvents.empty()) {
        std::printf("    SKIPPED (needs --image and --endevents)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPack(pack));
    // where the oracle's demo ends: the places read off its erase calls (ghosts 4, 3, 2, 1, then Pac-Man; x, y are the erase calls' plus 4) - ghost 1 touched Pac-Man
    const std::array<std::pair<int, int>, 4> ghosts = {{{167, 77}, {67, 89}, {125, 125}, {185, 89}}};
    core::ScreenScript script;
    CHECK(core::buildDemoDeathScript(pack, script, ghosts, 189, 89, 1, true));
    const auto f = readFile(testfw::config().endEvents);
    CHECK(f.size() > 8 && std::memcmp(f.data(), "TEV1", 4) == 0);
    if (f.size() <= 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    CHECK(f.size() == 8 + static_cast<std::size_t>(count) * 28);
    // anchor: the first row of the player's label ($556E) is at cycle 60,828,435 of the oracle's turn; the next turn starts at 63,189,736
    std::int64_t anchorMine = -1;
    for (const auto& e : script.trace)
        if (e.kind == 3) {
            anchorMine = e.at;
            break;
        }
    CHECK(anchorMine > 0);
    std::vector<std::array<std::int32_t, 7>> theirs;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::array<std::int32_t, 7> v{};
        std::memcpy(v.data(), f.data() + 8 + static_cast<std::size_t>(i) * 28, 28);
        if (v[0] >= 60'828'435 && v[0] < 63'189'736 && v[1] != 4) theirs.push_back(v);
    }
    std::vector<core::TraceEvent> mine;
    for (const auto& e : script.trace)
        if (e.at >= anchorMine && e.kind != 4) mine.push_back(e);
    CHECK(mine.size() == theirs.size());
    std::size_t bad = 0;
    const std::size_t n = std::min(mine.size(), theirs.size());
    for (std::size_t i = 0; i < n; ++i) {
        const core::TraceEvent& e = mine[i];
        const auto& v = theirs[i];
        bool same = e.at - anchorMine + 60'828'435 == v[0] && e.kind == v[1];
        if (e.kind == 5) {
            same = same && e.p[0] == v[2];
        } else {
            for (int k = 0; k < 5; ++k) same = same && e.p[static_cast<std::size_t>(k)] == v[2 + static_cast<std::size_t>(k)];
        }
        if (!same && bad++ < 5) std::printf("    event %zu: mine t=%lld kind %d | oracle t=%d kind %d\n", i, static_cast<long long>(e.at - anchorMine + 60'828'435), e.kind, v[0], v[1]);
    }
    std::printf("    %zu events of the attract game's death compared (label, 3 x 250 delay passes, five erases, Pac-Man's last image, 3 x 255 delay passes, six death frames with their erases and 128-pass delays): %zu differ; the next turn starts %lld cycles after the label (oracle: %d)\n", n, bad,
                static_cast<long long>(script.length - anchorMine), 63'189'736 - 60'828'435);
    CHECK(bad == 0);
    CHECK(script.length - anchorMine == 63'189'736 - 60'828'435);
}

TEST(attract_demo_game_pictures_equal_the_oracle) {
    if (testfw::config().image.empty() || testfw::config().demoPages.empty()) {
        std::printf("    SKIPPED (needs --image and --demopages)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPack(pack));
    const auto f = readFile(testfw::config().demoPages);
    CHECK(f.size() > 8 && std::memcmp(f.data(), "PGDM", 4) == 0);
    if (f.size() <= 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    const std::size_t rec = 4 + hires::kPageBytes;
    CHECK(f.size() == 8 + static_cast<std::size_t>(count) * rec);
    core::AttractTurn turn;
    CHECK(turn.start(pack));
    std::size_t compared = 0, bad = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint8_t* r = f.data() + 8 + static_cast<std::size_t>(i) * rec;
        std::uint32_t k = 0;
        std::memcpy(&k, r, 4);
        if (!turn.runToDemoTick(k)) break;
        ++compared;
        std::size_t diff = 0;
        for (std::size_t b = 0; b < hires::kPageBytes; ++b) diff += turn.hires().byte(b) != r[4 + b];
        if (diff != 0 && bad++ < 6) {
            std::printf("    tick %u: the page differs in %zu bytes:", k, diff);
            int shown = 0;
            for (std::size_t b = 0; b < hires::kPageBytes && shown < 6; ++b)
                if (turn.hires().byte(b) != r[4 + b]) {
                    // row from the interleaved offset
                    const std::size_t blockRow = (b / 0x80) % 8, third = b / 0x28 % 3, line = (b / 0x400) % 8;
                    (void)third;
                    std::printf(" [off %04zx mine %02x oracle %02x, row group %zu line %zu col %zu]", b, turn.hires().byte(b), r[4 + b], blockRow, line, (b % 0x80) % 0x28);
                    ++shown;
                }
            std::printf("\n");
        }
    }
    std::printf("    %zu pages of the attract game compared (the page at the start of every 10th tick, 0 to 7,890): %zu differ\n", compared, bad);
    CHECK(compared >= 780);
    CHECK(bad == 0);
    // the end of the turn: the ticks cost what the cost model says; the oracle's turn is 63,189,736 cycles long
    std::int64_t guard = 0;
    while (!turn.finished() && guard++ < 5000) turn.advance(50'000);
    std::printf("    the demo game ran %zu ticks (oracle: 7,894); the turn ended after %lld cycles (oracle: 63,189,736; the difference is the tick cost model's error over the demo)\n", turn.demoTicks(), static_cast<long long>(turn.length()));
    CHECK(turn.finished());
    CHECK(turn.demoTicks() == 7'894);
}

TEST(attract_demo_tick_cost_report) {
    if (testfw::config().image.empty() || testfw::config().demoCycles.empty()) {
        std::printf("    SKIPPED (needs --image and --democycles)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPack(pack));
    const auto f = readFile(testfw::config().demoCycles);
    CHECK(f.size() >= 8);
    if (f.size() < 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data(), 4);
    CHECK(f.size() == 4 + static_cast<std::size_t>(count) * 4);
    std::vector<std::uint32_t> cyc(count);
    std::memcpy(cyc.data(), f.data() + 4, static_cast<std::size_t>(count) * 4);
    core::GameDriver drv;
    CHECK(drv.init(pack));
    hires::Page blank;
    CHECK(drv.newDemo(blank));
    std::ofstream csv("scratch/tickfit.csv");   // one row per tick for the analysis in tools/tick_fit.py
    csv << "oracle,exact,acted,ate,energizer,eaten,z59,ncalls,t1,t2,t3,t4,t5,t6,t9,t10,t11,t12\n";
    std::map<long long, int> hist;
    double sum = 0, sum2 = 0;
    std::size_t n = 0;
    std::int64_t totalOracle = 0, totalExact = 0, totalModel = 0;
    for (std::uint32_t k = 0; k + 1 < count; ++k) {
        const core::TickEvents ev = drv.tick();
        const std::int64_t oracle = static_cast<std::int64_t>(cyc[k + 1]) - static_cast<std::int64_t>(cyc[k]);
        if (ev.demoOver) break;
        std::int64_t exact = 0;
        for (const auto& c : drv.lastCalls()) exact += core::drawCallCycles(c, pack);
        {
            int cnt[16] = {};
            for (const auto& c : drv.lastCalls()) ++cnt[std::min(c.type, 15)];
            csv << oracle << ',' << exact << ',' << ev.acted << ',' << ev.ate << ',' << ev.energizer << ',' << (ev.ghostEaten[0] || ev.ghostEaten[1] || ev.ghostEaten[2] || ev.ghostEaten[3]) << ',' << static_cast<int>(drv.core().state().zp(0x59)) << ','
                << drv.lastCalls().size() << ',' << cnt[1] << ',' << cnt[2] << ',' << cnt[3] << ',' << cnt[4] << ',' << cnt[5] << ',' << cnt[6] << ',' << cnt[9] << ',' << cnt[10] << ',' << cnt[11] << ',' << cnt[12] << '\n';
        }
        const std::int64_t resid = oracle - exact;
        hist[resid]++;
        sum += static_cast<double>(resid);
        sum2 += static_cast<double>(resid) * static_cast<double>(resid);
        totalOracle += oracle;
        totalExact += exact;
        totalModel += drv.tickCycles();
        ++n;
    }
    const double mean = sum / static_cast<double>(n);
    std::printf("    %zu ticks: oracle total %lld, drawing routines (exact) %lld, cost model %lld; residual (oracle - drawing) mean %.1f, sd %.1f, %zu distinct values\n", n, static_cast<long long>(totalOracle),
                static_cast<long long>(totalExact), static_cast<long long>(totalModel), mean, std::sqrt(sum2 / static_cast<double>(n) - mean * mean), hist.size());
    std::vector<std::pair<int, long long>> top;
    for (const auto& kv : hist) top.emplace_back(kv.second, kv.first);
    std::sort(top.rbegin(), top.rend());
    std::printf("    most frequent residuals (count x cycles):");
    for (std::size_t i = 0; i < top.size() && i < 16; ++i) std::printf(" %dx%lld", top[i].first, top[i].second);
    std::printf("\n");
}

TEST(attract_demo_tick_cost_in_joystick_mode) {
    if (testfw::config().image.empty() || testfw::config().joyDemoCycles.empty()) {
        std::printf("    SKIPPED (needs --image and --joydemocycles)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPack(pack));
    const auto f = readFile(testfw::config().joyDemoCycles);
    CHECK(f.size() >= 8);
    if (f.size() < 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data(), 4);
    CHECK(f.size() == 4 + static_cast<std::size_t>(count) * 4);
    std::vector<std::uint32_t> cyc(count);
    std::memcpy(cyc.data(), f.data() + 4, static_cast<std::size_t>(count) * 4);
    core::GameDriver drv;
    CHECK(drv.init(pack));
    hires::Page blank;
    CHECK(drv.newDemo(blank));
    drv.core().port().joystickMode = true;
    std::map<long long, int> hist;
    std::size_t n = 0, exact = 0, actedN = 0, idleN = 0;
    std::int64_t totalOracle = 0, totalModel = 0, actedErr = 0, idleErr = 0;
    for (std::uint32_t k = 0; k + 1 < count; ++k) {
        const core::TickEvents ev = drv.tick();
        if (ev.demoOver) break;
        const std::int64_t oracle = static_cast<std::int64_t>(cyc[k + 1]) - static_cast<std::int64_t>(cyc[k]);
        const std::int64_t mine = drv.tickCycles();
        ++hist[mine - oracle];
        if (ev.acted) { actedErr += mine - oracle; ++actedN; } else if (drv.lastCalls().empty()) { idleErr += mine - oracle; ++idleN; }
        exact += mine == oracle;
        totalOracle += oracle;
        totalModel += mine;
        ++n;
    }
    std::printf("    %zu ticks in joystick mode: oracle total %lld, cost model %lld (%+lld); %zu ticks exact; most frequent errors (count x cycles):", n, static_cast<long long>(totalOracle),
                static_cast<long long>(totalModel), static_cast<long long>(totalModel - totalOracle), exact);
    std::vector<std::pair<int, long long>> top;
    for (const auto& kv : hist) top.emplace_back(kv.second, kv.first);
    std::sort(top.rbegin(), top.rend());
    for (std::size_t i = 0; i < top.size() && i < 8; ++i) std::printf(" %dx%lld", top[i].first, top[i].second);
    std::printf("\n    ticks in which Pac-Man steps: %zu, mean error %.2f; ticks that draw nothing: %zu, mean error %.2f\n", actedN, actedN ? static_cast<double>(actedErr) / static_cast<double>(actedN) : 0.0, idleN,
                idleN ? static_cast<double>(idleErr) / static_cast<double>(idleN) : 0.0);
    CHECK(n >= 3000);
}

TEST(attract_pictures_equal_the_oracle) {
    if (testfw::config().image.empty() || testfw::config().titleEvents.empty() || testfw::config().titleSnaps.empty()) {
        std::printf("    SKIPPED (needs --image, --titleevents and --titlesnaps)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPack(pack));
    core::ScreenScript script;
    CHECK(core::buildAttractScript(pack, script, true));
    const auto snaps = readFile(testfw::config().titleSnaps);
    CHECK(snaps.size() > 12 && std::memcmp(snaps.data(), "TSN1", 4) == 0);
    if (snaps.size() <= 12) return;
    std::uint32_t count = 0;
    std::memcpy(&count, snaps.data() + 4, 4);
    const std::size_t rec = 5 + 8192 + 1024 + 1024;
    CHECK(snaps.size() == 12 + static_cast<std::size_t>(count) * rec);
    // the windows in which a drawing of the original is half done (a step takes effect at its first cycle)
    const auto& busy = script.busy;
    core::ScreenPlayer player;
    player.start(&script, &pack);
    std::size_t compared = 0, bad = 0, skipped = 0, loresChecked = 0, hiresChecked = 0;
    for (std::uint32_t s = 0; s < count; ++s) {
        const std::uint8_t* r = snaps.data() + 12 + static_cast<std::size_t>(s) * rec;
        std::uint32_t off = 0;
        std::memcpy(&off, r, 4);
        const std::uint8_t sw = r[4];
        if (static_cast<std::int64_t>(off) >= script.length) break;   // the chase follows (033-4c)
        if (static_cast<std::int64_t>(off) < player.now()) continue;
        player.advance(static_cast<std::int64_t>(off) - player.now());
        if (off < 7000) {   // the lo-res fill is under way
            ++skipped;
            continue;
        }
        bool inFlight = false;
        for (const auto& w : busy)
            if (off > w.first && off < w.second) {
                inFlight = true;
                break;
            }
        // the logo is drawn on the hidden page; the lo-res page is what is shown then
        if (inFlight && (sw & 8) != 0) {   // (only the hi-res page can show a half-drawn picture)
            ++skipped;
            continue;
        }
        ++compared;
        const bool hiresOracle = (sw & 8) != 0;
        bool same = player.hiresVisible() == hiresOracle;
        if (hiresOracle) {
            ++hiresChecked;
            for (std::size_t i = 0; i < hires::kPageBytes && same; ++i) same = player.hires().byte(i) == r[5 + i];
        } else {
            ++loresChecked;
            for (std::size_t i = 0; i < 1024 && same; ++i) same = player.loresByte(i) == r[5 + 8192 + 1024 + i];
        }
        if (!same && bad++ < 5) std::printf("    snapshot at +%u (switches %02x): picture differs\n", off, sw);
    }
    std::printf("    %zu of %u snapshots compared (%zu hi-res, %zu lo-res; %zu skipped: lo-res fill under way or a drawing half done), %zu differ\n", compared, count, hiresChecked, loresChecked, skipped, bad);
    CHECK(compared >= 150);
    CHECK(bad == 0);
}
