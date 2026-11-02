// TASK-030e9: the complete game core WITH ITS OWN GHOST LOGIC against the oracle, free-running: nothing is fed. The state is carried from tick to tick (keys and pokes of the
// recording applied at the start of each tick); after every tick ALL 1,010 bytes of the logical state (including the four ghost records), the shared ghost variables
// $E1 $E2 $E4 $EA $EB and the four move-queue pages must equal the oracle's. Only a completed maze (the next level's set-up is M6), a game over and the poke artefact of
// scenario D (Pac-Man outside the map) replace our state by the oracle's. Needs --image, --ticks and ghostram_X.bin next to ticks_X.bin.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "../src/core/gamecore.h"
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

std::string offsetName(std::size_t i) {
    char buf[40];
    if (i < 30) std::snprintf(buf, sizeof buf, "$%02zX", 0x52 + i);
    else if (i < 34) std::snprintf(buf, sizeof buf, "$%02X", i == 30 ? 0x78 : i == 31 ? 0x79 : i == 32 ? 0xE8 : 0xE9);
    else if (i < 114) std::snprintf(buf, sizeof buf, "ghost%zu.b%zu", (i - 34) / 20 + 1, (i - 34) % 20);
    else std::snprintf(buf, sizeof buf, "map[%zu]", i - 114);
    return buf;
}

void loadShared(GameCore& core, const std::uint8_t* z) {
    GhostShared& sh = core.ghostShared();
    sh.e1 = z[0xE1];
    sh.e2 = z[0xE2];
    sh.e4 = z[0xE4];
    sh.ea = z[0xEA];
    sh.eb = z[0xEB];
    for (std::size_t g = 0; g < 4; ++g) std::copy(z + 256 + 256 * g, z + 512 + 256 * g, core.ghostQueues()[g].begin());
}

}  // namespace

TEST(ghostworld_free_running_against_the_oracle_recordings) {
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
    std::map<std::size_t, std::pair<std::size_t, std::size_t>> diffs;   // offset -> (count, first tick)
    std::map<std::size_t, std::string> firstWhere;
    std::size_t ticks = 0, resyncs = 0, sharedBad = 0, queueBad = 0, fileCount = 0, caught = 0, eaten = 0, shown = 0;
    std::vector<std::string> files = split(testfw::config().ticks);
    for (const auto& extra : split(testfw::config().drawticks))   // the poked and custom scenarios of --drawticks are free-running tests too (one-player ones with ghostram_X.bin)
        if (std::find(files.begin(), files.end(), extra) == files.end()) files.push_back(extra);
    for (const auto& path : files) {
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
        ++fileCount;
        GameCore core;
        CHECK(core.init(loaded.pack));
        CHECK(core.newGame());
        core.state() = fromOracleLayout(tr.state(0));
        loadShared(core, ram.data() + 12);
        std::multimap<std::uint32_t, std::uint32_t> keysAt;
        for (const auto& in : tr.inputs()) keysAt.emplace(in.tick, in.key);
        std::multimap<std::uint32_t, TickPoke> pokesAt;
        for (const auto& p : tr.pokes()) pokesAt.emplace(p.tick, p);
        const std::size_t n = std::min<std::size_t>(count, tr.ticks());
        for (std::size_t k = 0; k + 1 < n; ++k) {
            const auto kr = keysAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = kr.first; it != kr.second; ++it) core.input().pushKey(static_cast<std::uint8_t>(it->second));
            GameState& s = core.state();
            const auto pr = pokesAt.equal_range(static_cast<std::uint32_t>(k));
            for (auto it = pr.first; it != pr.second; ++it) {
                const auto a = it->second.address;
                const auto v = static_cast<std::uint8_t>(it->second.value);
                if (a >= 0x52 && a <= 0x6F) s.zp(static_cast<std::uint16_t>(a)) = v;
                if (a >= 0x623A && a <= 0x65B9) s.map[a - 0x623A] = v;
                if (a >= 0x84 && a <= 0xD3) s.ghosts[(a - 0x84) / 20].b[(a - 0x84) % 20] = v;
                if (a == 0x78) s.mode78 = v;
                if (a == 0x79) s.mode79 = v;
                if (a == 0xE9) s.cornerPhase = v;
                if (a == 0xE1) core.ghostShared().e1 = v;
                if (a == 0xE2) core.ghostShared().e2 = v;
                if (a == 0xE4) core.ghostShared().e4 = v;
                if (a == 0xEA) core.ghostShared().ea = v;
                if (a == 0xEB) core.ghostShared().eb = v;
            }
            const GameState a = fromOracleLayout(tr.state(k));
            const GameState b = fromOracleLayout(tr.state(k + 1));
            const std::uint8_t* zb = ram.data() + 12 + (k + 1) * kRecord;
            const TickEvents ev = core.tick();
            ++ticks;
            caught += ev.caught;
            for (bool e : ev.ghostEaten) eaten += e;
            const bool resync = ev.levelComplete || ev.outsideMap || ev.gameOver || (b.lives() & 0x7F) == 0;
            if (!resync) {
                const auto mine = toOracleLayout(core.state());
                const auto theirs = toOracleLayout(b);
                bool anyDiff = false;
                for (std::size_t i = 0; i < mine.size(); ++i) {
                    if (mine[i] == theirs[i]) continue;
                    anyDiff = true;
                    auto& d = diffs[i];
                    if (d.first++ == 0) {
                        d.second = k;
                        char buf[200];
                        std::snprintf(buf, sizeof buf, "%s (mine %02X oracle %02X before %02X; caught %d eaten %d%d%d%d energizer %d acted %d)", path.c_str(), mine[i], theirs[i], toOracleLayout(a)[i], ev.caught,
                                      ev.ghostEaten[0], ev.ghostEaten[1], ev.ghostEaten[2], ev.ghostEaten[3], ev.energizer, ev.acted);
                        firstWhere[i] = buf;
                    }
                }
                const GhostShared& sh = core.ghostShared();
                const bool sharedOk = sh.e1 == zb[0xE1] && sh.e2 == zb[0xE2] && sh.e4 == zb[0xE4] && sh.ea == zb[0xEA] && sh.eb == zb[0xEB];
                if (!sharedOk) {
                    ++sharedBad;
                    if (shown++ < 6)
                        std::printf("    %s tick %zu: shared mine E1/2 %02X%02X E4 %u EA/B %02X%02X | oracle %02X%02X %u %02X%02X (energizer %d, eaten %d%d%d%d, caught %d)\n", path.c_str(), k, sh.e2, sh.e1, sh.e4, sh.eb, sh.ea,
                                    zb[0xE2], zb[0xE1], zb[0xE4], zb[0xEB], zb[0xEA], ev.energizer, ev.ghostEaten[0], ev.ghostEaten[1], ev.ghostEaten[2], ev.ghostEaten[3], ev.caught);
                }
                bool queuesOk = true;
                for (std::size_t g = 0; g < 4; ++g) {
                    const std::uint8_t* oq = zb + 256 + 256 * g;
                    std::size_t stop = std::min<std::size_t>(255, b.ghosts[g].b[7]);
                    while (stop < 255 && oq[stop] != 0xFF) ++stop;
                    for (std::size_t i = 0; i <= stop; ++i)
                        if (core.ghostQueues()[g][i] != oq[i]) {
                            queuesOk = false;
                            break;
                        }
                }
                if (!queuesOk) {
                    ++queueBad;
                    if (shown++ < 6) std::printf("    %s tick %zu: a move queue differs\n", path.c_str(), k);
                }
                if (anyDiff || !sharedOk || !queuesOk) {
                    core.state() = b;   // so that one difference does not cascade
                    loadShared(core, zb);
                    ++resyncs;
                }
            } else {
                core.state() = b;
                loadShared(core, zb);
                if (ev.outsideMap && b.dotsLeft() < a.dotsLeft()) core.itemEatenOutsideTheMap();
                ++resyncs;
            }
        }
    }
    std::printf("    %zu files, %zu ticks run free (nothing fed): %zu catches and %zu ghosts eaten by the core; shared variables wrong in %zu ticks, queues wrong in %zu\n", fileCount, ticks, caught, eaten, sharedBad, queueBad);
    for (const auto& [off, d] : diffs) std::printf("    %-12s differs in %zu ticks (first: tick %zu of %s)\n", offsetName(off).c_str(), d.first, d.second, firstWhere[off].c_str());
    CHECK(fileCount >= 1 && ticks > 10000);
    CHECK(diffs.empty() && sharedBad == 0 && queueBad == 0);
}
