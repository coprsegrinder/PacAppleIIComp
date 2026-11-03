// TASK-033-4f: the two boot menus of the original ($4027: "CHOOSE A MODE (1 - 2)", "NORMAL OR CUSTOMIZED KEYBOARD (N/C)?", the four key prompts) against the oracle running the real monitor
// ROM (tools/capture_boot.py, tools/export_boot.py: 8 key lists). Needs --image and --boot. Cycles count from the first instruction of the program ($4027).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "flow_common.h"

using namespace pacman;
using flowtest::settle;

TEST(boot_menus_equal_the_oracle) {
    if (testfw::config().image.empty() || testfw::config().boot.empty()) {
        std::printf("    SKIPPED (needs --image and --boot)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    const auto f = flowtest::readAll(testfw::config().boot);
    CHECK(f.size() > 8 && std::memcmp(f.data(), "BOOT", 4) == 0);
    if (f.size() <= 8) return;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    std::size_t p = 8, bad = 0, waits = 0;
    for (std::uint32_t run = 0; run < count; ++run) {
        std::uint32_t n = 0;
        std::memcpy(&n, f.data() + p, 4);
        p += 4;
        const std::uint8_t* keys = f.data() + p;
        p += n;
        core::AttractFlow flow;
        CHECK(flow.startBoot(pack, flowtest::romPath()));
        settle(flow);
        bool ok = true;
        for (std::uint32_t i = 0; i <= n; ++i) {
            std::uint32_t cyc = 0, t0 = 0;
            std::memcpy(&cyc, f.data() + p, 4);
            const int f5 = f.data()[p + 4];
            const std::uint8_t* kc = f.data() + p + 5;
            std::memcpy(&t0, f.data() + p + 9, 4);
            const std::uint8_t* page = f.data() + p + 13;
            const std::uint8_t* zp = page + 1024;
            p += 13 + 1024 + 256;
            if (i > 0) {
                flow.keyDown(keys[i - 1]);
                settle(flow);
            }
            ++waits;
            const bool last = i == n;
            bool good = true;
            if (last && t0 != 0) {                                   // the last key ended the menus: the first turn starts
                good = flow.mode() == core::AttractFlow::Mode::Attract && flow.bootT0() == static_cast<std::int64_t>(t0);
            } else {
                good = flow.textReady() && flow.waitClock() == static_cast<std::int64_t>(cyc);
            }
            std::size_t d = 0;
            if (!(last && t0 != 0)) {
                for (std::size_t b = 0; b < 1024; ++b) d += flow.text().byte(b) != page[b];
                good = good && flowtest::zeroPageDiff(flow.waitZeroPage(), zp) == 0;
            }
            good = good && d == 0 && flow.inputDevice() == f5 && flow.keyLeft() == kc[0] && flow.keyRight() == kc[1] && flow.keyUp() == kc[2] && flow.keyDown_() == kc[3];
            if (!good) {
                ok = false;
                std::printf("    run %u key %u: clock mine %lld oracle %u (T0 mine %lld oracle %u), page %zu bytes, F5 %d/%d, keys %02X%02X%02X%02X/%02X%02X%02X%02X\n", run + 1, i, static_cast<long long>(flow.waitClock()), cyc,
                            static_cast<long long>(flow.bootT0()), t0, d, flow.inputDevice(), f5, flow.keyLeft(), flow.keyRight(), flow.keyUp(), flow.keyDown_(), kc[0], kc[1], kc[2], kc[3]);
            }
        }
        if (!ok) ++bad;
    }
    std::printf("    %u key lists of the boot menus, %zu key waits compared (cycle, text page, zero page, random counter, $F5, the four key codes, the cycle of the first turn): %zu runs differ\n", count, waits, bad);
    CHECK(bad == 0);
}
