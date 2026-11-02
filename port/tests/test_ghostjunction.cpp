// TASK-030e4: junction detection, the steering decision and the move queue against the oracle.
// Needs --image, --ticks and a ghostram_X.bin next to each ticks_X.bin (tools/record_ghost_ram.py). Per tick the four ghosts are run in the order 4, 3, 2, 1 from the
// oracle's state at the start of the tick; the results are compared with the state at the start of the next tick, except in ticks in which something else changes the
// same fields (an energizer or a ghost eaten, a catch, a level end, a poke).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "../src/core/ghostjunction.h"
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

constexpr std::size_t kRecord = 1280;   // zero page 256 + four queue pages 1024

}  // namespace

TEST(ghostjunction_against_the_oracle_recordings) {
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
    std::size_t files = 0, ticks = 0, skipped = 0, onJunction = 0, decided = 0, junctionBad = 0, quietBad = 0, quiet = 0, queueBad = 0, specialTicks = 0, eyesDecided = 0, eyesAtDoor = 0;
    std::size_t shown = 0;
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
        CHECK(ram.size() == 12 + static_cast<std::size_t>(count) * kRecord);
        std::map<std::uint32_t, int> pokedAt;
        for (const auto& p : tr.pokes()) pokedAt[p.tick] = 1;
        ++files;
        const std::size_t n = std::min<std::size_t>(count, tr.ticks());
        for (std::size_t k = 0; k + 1 < n; ++k) {
            const std::uint8_t* za = ram.data() + 12 + k * kRecord;
            const std::uint8_t* zb = ram.data() + 12 + (k + 1) * kRecord;
            const GameState a = fromOracleLayout(tr.state(k));
            const GameState b = fromOracleLayout(tr.state(k + 1));
            bool other = pokedAt.count(static_cast<std::uint32_t>(k)) != 0 || a.demo != 0 || b.level() != a.level() || (b.lives() & 0x7F) < (a.lives() & 0x7F) ||
                         (zb[0xE1] + zb[0xE2] * 256) > (za[0xE1] + za[0xE2] * 256) || (zb[0xE4] != 0 && za[0xE4] == 0) || b.zp(0x68) != a.zp(0x68) || b.cornerPhase != a.cornerPhase;   // a corner/chase flip reverses every ghost before the ghost loop ($80C1, 030e7)
            for (std::size_t g = 0; g < 4; ++g) other = other || (a.ghosts[g].b[9] == 0 && b.ghosts[g].b[9] != 0);
            if (other) {
                ++skipped;
                continue;
            }
            ++ticks;
            GameState s = a;
            for (const std::uint16_t addr : {std::uint16_t{0x60}, std::uint16_t{0x61}, std::uint16_t{0x62}, std::uint16_t{0x69}}) s.zp(addr) = b.zp(addr);   // Pac-Man has moved before the ghosts do
            s.cornerPhase = b.cornerPhase;
            GhostShared sh;
            sh.e1 = za[0xE1];
            sh.e2 = za[0xE2];
            sh.e4 = za[0xE4];
            GhostQueues queues;
            for (std::size_t g = 0; g < 4; ++g) std::copy(za + 256 + 256 * g, za + 512 + 256 * g, queues[g].begin());
            for (int g = 4; g >= 1; --g) {
                const auto gi = static_cast<std::size_t>(g - 1);
                const HeadResult h = ghostHead(s, g, sh, tables);
                if (!h.acted) continue;
                const auto was = s.ghosts[gi];
                const JunctionStepResult j = ghostJunction(s, g, queues, graph, data, tables);
                const auto& mine = s.ghosts[gi];
                const auto& theirs = b.ghosts[gi];
                const bool special = j.onJunction && (j.junction == 0x17 || j.junction == 0x18 || j.junction == 0x1A || j.junction == 0x1D);
                if (j.onJunction && special && was.b[9] != 0) {   // an eaten ghost arriving at the door: the house code (030e8)
                    ++eyesAtDoor;
                    continue;
                }
                if (j.onJunction) {
                    ++onJunction;
                    decided += j.decided;
                    eyesDecided += j.decided && was.b[9] != 0;
                    specialTicks += special;
                    bool ok = mine.b[2] == theirs.b[2] && mine.b[7] == theirs.b[7];
                    if (!special) ok = ok && mine.b[3] == theirs.b[3] && mine.b[4] == theirs.b[4];
                    bool queueOk = true;
                    for (std::size_t i = 0; i < 256; ++i) queueOk = queueOk && queues[gi][i] == zb[256 + 256 * gi + i];
                    if (!ok || !queueOk) {
                        if (shown++ < 8)
                            std::printf("    %s tick %zu ghost %d on junction %u: dir mine %u oracle %u, queue index mine %u oracle %u, junction mine %u/%u oracle %u/%u, queue %s\n", path.c_str(), k, g, j.junction, mine.b[2], theirs.b[2], mine.b[7],
                                        theirs.b[7], mine.b[3], mine.b[4], theirs.b[3], theirs.b[4], queueOk ? "equal" : "DIFFERENT");
                        junctionBad += !ok;
                        queueBad += !queueOk;
                    }
                } else {
                    // not on a junction: direction, queue index and the two junction numbers stay (the slow-flag path of the move code can change the junctions: left out)
                    ++quiet;
                    if (was.b[10] == 0 && was.b[17] == 0 && !(mine.b[2] == theirs.b[2] && mine.b[7] == theirs.b[7] && mine.b[3] == theirs.b[3] && mine.b[4] == theirs.b[4])) {
                        if (shown++ < 8) std::printf("    %s tick %zu ghost %d off junction: dir %u/%u idx %u/%u junctions %u,%u / %u,%u\n", path.c_str(), k, g, mine.b[2], theirs.b[2], mine.b[7], theirs.b[7], mine.b[3], mine.b[4], theirs.b[3], theirs.b[4]);
                        ++quietBad;
                    }
                }
            }
        }
    }
    std::printf("    %zu files, %zu ticks (%zu skipped); acting ghosts on a junction: %zu (decision run %zu, of them eyes %zu, special junctions %zu), wrong %zu, queue pages wrong %zu; off a junction: %zu, wrong %zu\n", files, ticks, skipped,
                onJunction, decided, eyesDecided, specialTicks, junctionBad, queueBad, quiet, quietBad);
    std::printf("    eyes arriving at the door (left to 030e8): %zu\n", eyesAtDoor);
    CHECK(files >= 1 && onJunction > 500);
    CHECK(junctionBad == 0 && queueBad == 0 && quietBad == 0);
}
