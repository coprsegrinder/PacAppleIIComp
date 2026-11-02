// TASK-030e5/e6: the complete ghost tick for ghosts outside the house (head, junction logic and decision, special junctions, the move with its cell counters and the
// tunnel wrap) against the oracle: ALL 20 bytes of every ghost record at the start of the next tick. The ghosts are processed in the order 4, 3, 2, 1 from the oracle's state at
// the start of the tick (Pac-Man's post-step position is taken from the next state, as the ghost loop runs after Pac-Man's step). Left out: ticks with
// pokes, eaten ghosts, energizers, catches, level changes and corner/chase flips (030e7/e8), and the demo.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "../src/core/ghostmode.h"
#include "../src/core/ticktrace.h"
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

constexpr std::size_t kRecord = 1280;

}  // namespace

TEST(ghostmove_whole_record_against_the_oracle_recordings) {
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
    GhostTables tables;
    CHECK(tables.load(loaded.pack));
    GhostDecisionData data;
    CHECK(data.load(loaded.pack));
    JunctionGraph graph;
    CHECK(graph.load(loaded.pack));
    GhostModeTables modeTables;
    CHECK(modeTables.load(loaded.pack));
    std::size_t files = 0, ticks = 0, skipped = 0, compared = 0, bad = 0, houseSteps = 0, tunnel = 0, specials = 0;
    std::array<std::size_t, 20> badByte{};
    std::size_t shown = 0, queueBad = 0, sharedBad = 0, energizers = 0, flips = 0;
    for (const auto& path : split(testfw::config().ticks)) {
        std::string ramPath = path;
        const std::size_t at = ramPath.rfind("ticks_");
        if (at == std::string::npos) continue;
        ramPath.replace(at, 6, "ghostram_");
        const auto ram = readAll(ramPath);
        if (ram.size() < 12 || std::memcmp(ram.data(), "GRAM", 4) != 0) continue;
        std::string why;
        const auto tr = TickTrace::load(path, why);
        CHECK_MSG(tr.ok(), why);
        if (!tr.ok()) continue;
        std::uint32_t count = 0;
        std::memcpy(&count, ram.data() + 4, 4);
        std::map<std::uint32_t, int> pokedAt;
        for (const auto& p : tr.pokes()) pokedAt[p.tick] = 1;
        ++files;
        const std::size_t n = std::min<std::size_t>(count, tr.ticks());
        for (std::size_t k = 0; k + 1 < n; ++k) {
            const std::uint8_t* za = ram.data() + 12 + k * kRecord;
            const std::uint8_t* zb = ram.data() + 12 + (k + 1) * kRecord;
            const GameState a = fromOracleLayout(tr.state(k));
            const GameState b = fromOracleLayout(tr.state(k + 1));
            bool energizer = false;   // an energizer was eaten in this tick: the map cell went from energizer to empty
            for (std::size_t i = 0; i < a.map.size(); ++i) energizer = energizer || ((a.map[i] >> 1) != 0 && b.map[i] == 0);
            bool other = pokedAt.count(static_cast<std::uint32_t>(k)) != 0 || a.demo != 0 || b.level() != a.level() || (b.lives() & 0x7F) < (a.lives() & 0x7F);
            if (!energizer) other = other || (zb[0xE1] + zb[0xE2] * 256) > (za[0xE1] + za[0xE2] * 256);
            for (std::size_t g = 0; g < 4; ++g) other = other || (a.ghosts[g].b[9] == 0 && b.ghosts[g].b[9] != 0);
            if (other) {
                ++skipped;
                continue;
            }
            ++ticks;
            energizers += energizer;
            flips += b.cornerPhase != a.cornerPhase;
            GameState s = a;
            for (const std::uint16_t addr : {std::uint16_t{0x60}, std::uint16_t{0x61}, std::uint16_t{0x62}, std::uint16_t{0x69}}) s.zp(addr) = b.zp(addr);
            GhostShared sh;
            sh.e1 = za[0xE1];
            sh.e2 = za[0xE2];
            sh.e4 = za[0xE4];
            sh.ea = za[0xEA];
            sh.eb = za[0xEB];
            GhostQueues queues;
            for (std::size_t g = 0; g < 4; ++g) std::copy(za + 256 + 256 * g, za + 512 + 256 * g, queues[g].begin());
            // before the ghost loop: Pac-Man's step ($7861) runs the mode countdown when he acts, and an energizer eaten in it frightens the ghosts
            if (a.zp(0x59) == 1) modeTimerStep(s, sh, queues);
            if (energizer) energizerGhostEffects(s, sh, queues, modeTables);
            for (int g = 4; g >= 1; --g) {
                const auto gi = static_cast<std::size_t>(g - 1);
                const HeadResult h = ghostHead(s, g, sh, tables);
                if (!h.acted) continue;
                const JunctionStepResult j = ghostJunction(s, g, queues, graph, data, tables);
                specials += j.onJunction && (j.junction == 0x1A || j.junction == 0x1D);
                tunnel += s.ghosts[gi].b[10] != 0;
                houseSteps += s.ghosts[gi].b[17] != 0 && s.ghosts[gi].b[10] == 0;
                ghostMove(s, g, queues, data);
            }
            if (sh.e1 != zb[0xE1] || sh.e2 != zb[0xE2] || sh.e4 != zb[0xE4] || sh.ea != zb[0xEA] || sh.eb != zb[0xEB] || s.cornerPhase != b.cornerPhase || s.zp(0x68) != b.zp(0x68)) {
                ++sharedBad;
                if (shown++ < 10)
                    std::printf("    %s tick %zu: shared mine E1/2 %02X%02X E4 %u EA/B %02X%02X phase %u step68 %u | oracle %02X%02X %u %02X%02X %u %u\n", path.c_str(), k, sh.e2, sh.e1, sh.e4, sh.eb, sh.ea, s.cornerPhase, s.zp(0x68),
                                zb[0xE2], zb[0xE1], zb[0xE4], zb[0xEB], zb[0xEA], b.cornerPhase, b.zp(0x68));
            }
            for (int g = 1; g <= 4; ++g) {
                const auto gi = static_cast<std::size_t>(g - 1);
                ++compared;
                bool ok = true;
                {   // the queue page: everything up to the first terminator at or after the current index (what follows it is never read)
                    const std::uint8_t* oq = zb + 256 + 256 * gi;
                    std::size_t stop = std::min<std::size_t>(255, b.ghosts[gi].b[7]);
                    while (stop < 255 && oq[stop] != 0xFF) ++stop;
                    for (std::size_t i = 0; i <= stop; ++i)
                        if (queues[gi][i] != oq[i]) {
                            ok = false;
                            ++queueBad;
                            break;
                        }
                }
                for (std::size_t i = 0; i < 20; ++i) {
                    if (s.ghosts[gi].b[i] != b.ghosts[gi].b[i]) {
                        ok = false;
                        ++badByte[i];
                    }
                }
                if (!ok) {
                    ++bad;
                    if (shown++ < 10) {
                        std::printf("    %s tick %zu ghost %d: mine", path.c_str(), k, g);
                        for (std::size_t i = 0; i < 20; ++i) std::printf(" %02X", s.ghosts[gi].b[i]);
                        std::printf(" | oracle");
                        for (std::size_t i = 0; i < 20; ++i) std::printf(" %02X", b.ghosts[gi].b[i]);
                        std::printf(" | start");
                        for (std::size_t i = 0; i < 20; ++i) std::printf(" %02X", a.ghosts[gi].b[i]);
                        std::printf("\n");
                    }
                }
            }
        }
    }
    std::printf("    %zu files, %zu ticks (%zu skipped); ghost records compared %zu (of them %zu house steps), wrong %zu; tunnel moves %zu, tunnel-end junctions %zu\n", files, ticks, skipped, compared, houseSteps,
                bad, tunnel, specials);
    std::printf("    queue pages wrong: %zu; shared variables ($E1 $E2 $E4 $EA $EB $E9 $68) wrong in %zu ticks; energizer ticks %zu, corner/chase flips %zu\n    wrong bytes by record offset:", queueBad, sharedBad, energizers, flips);
    for (std::size_t i = 0; i < 20; ++i)
        if (badByte[i] != 0) std::printf(" b[%zu]=%zu", i, badByte[i]);
    std::printf("\n");
    CHECK(files >= 1 && compared > 50000);
    CHECK(bad == 0 && sharedBad == 0);
}

// $6E79: the move queues at the start of a life equal the recording's at tick 0 (ghost 1: the 15 bytes of its script; ghosts 2-4 the whole page).
TEST(ghostmode_queues_at_the_start_of_a_life) {
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
    GhostDecisionData data;
    CHECK(data.load(loaded.pack));
    GhostQueues queues{};
    initGhostQueues(queues, data);
    std::size_t files = 0, bad = 0;
    for (const auto& path : split(testfw::config().ticks)) {
        std::string ramPath = path;
        const std::size_t at = ramPath.rfind("ticks_");
        if (at == std::string::npos) continue;
        ramPath.replace(at, 6, "ghostram_");
        const auto ram = readAll(ramPath);
        if (ram.size() < 12 + kRecord || std::memcmp(ram.data(), "GRAM", 4) != 0) continue;
        const std::uint8_t* z = ram.data() + 12;   // tick 0 of a game
        ++files;
        for (std::size_t i = 0; i < 15; ++i) bad += queues[0][i] != z[256 + i];
        for (std::size_t g = 1; g < 4; ++g)
            for (std::size_t i = 0; i < 256; ++i) bad += queues[g][i] != z[256 + 256 * g + i];
    }
    std::printf("    %zu recordings: queues at tick 0, %zu bytes differ\n", files, bad);
    CHECK(files >= 1 && bad == 0);
}
