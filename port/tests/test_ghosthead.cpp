// TASK-030e3: the head of the ghost tick (countdown, shared frightened timer and its end, choice of the next countdown) against the oracle.
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

#include "../src/core/ghosttick.h"
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

TEST(ghosthead_synthetic_rules) {
    GhostTables t;
    for (std::size_t i = 0; i < 16; ++i) {
        t.normal[i] = static_cast<std::uint8_t>(10 - i / 2);
        t.frightened[i] = static_cast<std::uint8_t>(19 - i);
        t.eyes[i] = 2;
        t.turn[i] = 5;
    }
    GameState s;
    s.level() = 1;
    GhostShared sh;
    s.ghosts[3].b[16] = 3;
    CHECK(!ghostHead(s, 4, sh, t).acted && s.ghosts[3].b[16] == 2);   // counting down
    s.ghosts[3].b[16] = 1;
    s.ghosts[3].b[0] = 0x41;   // bit 1 of X clear: the ghost term applies; ghost 4: (4 >> 1) - 1 = 1, carry 0
    const auto r = ghostHead(s, 4, sh, t);
    CHECK(r.acted && r.reload == t.normal[1] + 1 && s.ghosts[3].b[16] == r.reload);
    s.ghosts[0].b[16] = 1;
    s.ghosts[0].b[0] = 0x42;   // bit 1 set: no term
    s.ghosts[0].b[10] = 1;     // slow flag: doubled
    CHECK(ghostHead(s, 1, sh, t).reload == 2 * t.normal[1]);
    // the frightened timer: counted down by every acting ghost, ends when it reaches exactly zero
    s.ghosts[1].b[16] = 1;
    s.ghosts[1].b[8] = 1;
    sh.e4 = 1;
    sh.e1 = 2;
    sh.e2 = 0;
    s.cornerPhase = 1;
    CHECK(!ghostHead(s, 2, sh, t).frightenedEnded && sh.e1 == 1);
    s.ghosts[1].b[16] = 1;
    const auto e = ghostHead(s, 2, sh, t);
    CHECK(e.frightenedEnded && sh.e4 == 0 && s.cornerPhase == 0 && s.zp(0x68) == 2 && s.ghosts[1].b[8] == 0 && e.reload == t.normal[1]);
}

TEST(ghosthead_against_the_oracle_recordings) {
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
    std::size_t files = 0, ticks = 0, skipped = 0, idle = 0, idleBad = 0, acted = 0, actedSame = 0, actedTurn = 0, actedOther = 0, sharedBad = 0, ended = 0, endedBad = 0;
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
            // ticks in which something outside the head changes the same fields
            bool other = pokedAt.count(static_cast<std::uint32_t>(k)) != 0 || a.demo != 0 || b.level() != a.level() || (b.lives() & 0x7F) < (a.lives() & 0x7F) ||
                         (zb[0xE1] + zb[0xE2] * 256) > (za[0xE1] + za[0xE2] * 256) || (zb[0xE4] != 0 && za[0xE4] == 0) || b.zp(0x68) != a.zp(0x68);
            for (std::size_t g = 0; g < 4; ++g) other = other || (a.ghosts[g].b[9] == 0 && b.ghosts[g].b[9] != 0);   // a ghost eaten
            if (other) {
                ++skipped;
                continue;
            }
            ++ticks;
            GameState s = a;
            GhostShared sh;
            sh.e1 = za[0xE1];
            sh.e2 = za[0xE2];
            sh.e4 = za[0xE4];
            HeadResult res[5];
            for (int g = 4; g >= 1; --g) res[g] = ghostHead(s, g, sh, tables);
            if (sh.e1 != zb[0xE1] || sh.e2 != zb[0xE2] || sh.e4 != zb[0xE4]) {
                if (shown++ < 5) std::printf("    %s tick %zu: shared timer mine %02X%02X e4 %d, oracle %02X%02X e4 %d\n", path.c_str(), k, sh.e2, sh.e1, sh.e4, zb[0xE2], zb[0xE1], zb[0xE4]);
                ++sharedBad;
            }
            for (int g = 1; g <= 4; ++g) {
                const auto& mine = s.ghosts[static_cast<std::size_t>(g - 1)];
                const auto& was = a.ghosts[static_cast<std::size_t>(g - 1)];
                const auto& theirs = b.ghosts[static_cast<std::size_t>(g - 1)];
                if (res[g].frightenedEnded) {
                    ++ended;
                    bool ok = b.cornerPhase == 0 && b.zp(0x68) == 2;
                    for (const auto& gh : b.ghosts) ok = ok && gh.b[8] == 0;
                    endedBad += !ok;
                }
                if (!res[g].acted) {
                    ++idle;
                    if (mine.b[16] != theirs.b[16]) {
                        if (shown++ < 5) std::printf("    %s tick %zu ghost %d: countdown mine %u oracle %u\n", path.c_str(), k, g, mine.b[16], theirs.b[16]);
                        ++idleBad;
                    }
                    continue;
                }
                ++acted;
                const bool turned = theirs.b[2] != was.b[2];
                if (theirs.b[16] == res[g].reload) ++actedSame;
                else if (turned && theirs.b[16] == tables.turn[std::min<std::size_t>(a.level(), 15)]) ++actedTurn;
                else {
                    ++actedOther;
                    if (shown++ < 12) std::printf("    %s tick %zu ghost %d: reload mine %u, oracle %u (turned %d, level %u, flags 78=%u 79=%u 7A=%u)\n", path.c_str(), k, g, res[g].reload, theirs.b[16], turned, a.level(), was.b[8], was.b[9], was.b[10]);
                }
            }
        }
    }
    std::printf("    %zu files, %zu ticks compared (%zu skipped: pokes, eaten ghosts, energizers, catches, level changes, demo)\n", files, ticks, skipped);
    std::printf("    waiting ghosts: %zu, countdown wrong in %zu; acting ghosts: %zu (reload as chosen %zu, turn table %zu, other %zu); shared timer wrong in %zu ticks; frightened ends: %zu (%zu wrong)\n", idle, idleBad,
                acted, actedSame, actedTurn, actedOther, sharedBad, ended, endedBad);
    CHECK(files >= 1 && ticks > 10000);
    CHECK(idleBad == 0 && sharedBad == 0 && endedBad == 0);
    CHECK(acted > 1000);
}
