// TASK-033-4b: the attract title screen against the oracle. Needs --image and --titleevents (every routine entry of one title turn with its cycle) and, for the picture test,
// --titlesnaps (the screen memory every 50,000 cycles). Both files come from the original running on the oracle (tools/capture_events.py, tools/capture_title_snaps.py).
#include <algorithm>
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

TEST(attract_title_pictures_equal_the_oracle) {
    if (testfw::config().image.empty() || testfw::config().titleEvents.empty() || testfw::config().titleSnaps.empty()) {
        std::printf("    SKIPPED (needs --image, --titleevents and --titlesnaps)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(loadPack(pack));
    core::ScreenScript script;
    CHECK(core::buildTitleScript(pack, script, true));
    const auto snaps = readFile(testfw::config().titleSnaps);
    CHECK(snaps.size() > 12 && std::memcmp(snaps.data(), "TSN1", 4) == 0);
    if (snaps.size() <= 12) return;
    std::uint32_t count = 0;
    std::memcpy(&count, snaps.data() + 4, 4);
    const std::size_t rec = 5 + 8192 + 1024 + 1024;
    CHECK(snaps.size() == 12 + static_cast<std::size_t>(count) * rec);
    // times at which a drawing is half done in the original (a step takes effect at its first cycle): from a routine entry to the next routine entry
    std::vector<std::pair<std::int64_t, std::int64_t>> busy;
    for (std::size_t i = 0; i + 1 < script.trace.size(); ++i) {
        const int k = script.trace[i].kind;
        if (k == 3 || k == 6 || k == 7 || k == 8) busy.emplace_back(script.trace[i].at, script.trace[i + 1].at);
    }
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
        if (inFlight) {
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
