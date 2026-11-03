// TASK-033-4e2 / 4e3: the hidden menu (Ctrl-V, Ctrl-C at the players prompt) against the oracle running the same monitor ROM: tools/capture_hidden.py (A2ROM=1) drove the original
// with 13 key lists (cartoons 1-3, ranks 7, 21, 22, 0, 12, 10, 13, 5, empty lines, editing keys); tools/export_hidden.py packs them. Needs --image and --hidden.
// The line input is the ROM's own ($FD6F), so the editing keys (Backspace, Ctrl-X, lower case, ...) are checked by the same comparison.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "flow_common.h"

using namespace pacman;
using flowtest::settle;

TEST(hidden_menu_equals_the_oracle) {
    if (testfw::config().image.empty() || testfw::config().hidden.empty()) {
        std::printf("    SKIPPED (needs --image and --hidden)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    const auto f = flowtest::readAll(testfw::config().hidden);
    CHECK(f.size() > 8 && std::memcmp(f.data(), "HIDN", 4) == 0);
    if (f.size() <= 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    std::size_t p = 8, bad = 0, waits = 0, pageDiffs = 0, zpDiffs = 0;
    for (std::uint32_t run = 0; run < count; ++run) {
        std::uint32_t n = 0;
        std::memcpy(&n, f.data() + p, 4);
        p += 4;
        const std::uint8_t* keys = f.data() + p;
        p += n;
        core::AttractFlow flow;
        CHECK(flow.start(pack, flowtest::romPath()));
        flow.advance(100'000);
        flow.keyDown(core::kKeySpace);
        while (flow.mode() == core::AttractFlow::Mode::Attract) flow.advance(10'000);
        bool ok = true;
        std::int64_t spun = 0;
        int rndShift = 0, rnd0 = 0;
        for (std::uint32_t i = 0; i <= n; ++i) {
            std::uint32_t cyc = 0;
            std::memcpy(&cyc, f.data() + p, 4);
            const int rank = f.data()[p + 4];
            const std::uint8_t* page = f.data() + p + 6;
            const std::uint8_t* zp = page + 1024;
            p += 6 + 1024 + 256;
            if (i == 0) rnd0 = zp[0x4E] | (zp[0x4F] << 8);
            if (i == 1) {                                   // the oracle presses its keys at once: what the port spun while waiting for the first one shifts its clock and counter
                spun = flow.promptClock() - flow.waitClock();
                rndShift = flowtest::rndOf(flow.monitor()) - rnd0;
            }
            if (i > 0) flow.keyDown(keys[i - 1]);
            settle(flow);
            if (flow.mode() == core::AttractFlow::Mode::Cartoon) flow.advance(100'000'000);   // the intermission runs out, then the players prompt
            settle(flow);
            ++waits;
            const bool clockOk = flow.waitClock() == static_cast<std::int64_t>(cyc) + spun && flow.textReady();
            const bool rankOk = flow.startLevel() == rank;
            std::size_t d = 0;
            for (std::size_t b = 0; b < 1024; ++b) d += flow.text().byte(b) != page[b];
            const std::size_t dz = flowtest::zeroPageDiff(flow.waitZeroPage(), zp, rndShift);
            if (d != 0) ++pageDiffs;
            if (dz != 0) ++zpDiffs;
            if (!clockOk || !rankOk || d != 0 || dz != 0) {
                ok = false;
                std::printf("    run %u key %u: clock mine %lld oracle %u, rank mine %d oracle %d, page differs in %zu bytes, zero page in %zu\n", run + 1, i, static_cast<long long>(flow.waitClock()) - static_cast<long long>(spun), cyc, flow.startLevel(), rank, d, dz);
            }
        }
        if (!ok) ++bad;
    }
    std::printf("    %u key lists of the hidden menu, %zu key waits compared (cycle, text page with the cursor, zero page, random counter, rank $54E1): %zu runs differ, %zu pages and %zu zero pages differ\n", count, waits, bad, pageDiffs, zpDiffs);
    CHECK(bad == 0);
}
