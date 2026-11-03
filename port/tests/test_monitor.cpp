// TASK-033-4e3: the 6502 core and the monitor ROM in the port: the ROM is loaded and runs (COUT1, VTAB, scrolling, a key wait), and the cycle counts of the instructions.
#include <cstdint>
#include <cstdio>

#include "../src/core/monitor.h"
#include "flow_common.h"

using namespace pacman;

namespace {
core::Monitor makeMonitor(bool& ok) {
    core::Monitor m;
    ok = m.loadRom(flowtest::romPath());
    m.reset();
    return m;
}
}  // namespace

TEST(monitor_rom_prints_scrolls_and_waits_for_keys) {
    bool ok = false;
    core::Monitor m = makeMonitor(ok);
    if (!ok) {
        std::printf("    SKIPPED (the ROM dump is not in \"Apple II roms/\")\n");
        return;
    }
    // COUT1 stores the character at the cursor: row 0 of text page 1 is $0400 (the monitor's own page); the cursor moves right
    m.setA(0xC1);
    CHECK(m.call(0xFDF0) == core::Monitor::Status::Done);
    CHECK(m.peek(0x400) == 0xC1 && m.peek(0x24) == 1);
    // a Return moves to the next line (the monitor's CR: $0480 is row 1)
    m.setA(0x8D);
    CHECK(m.call(0xFDF0) == core::Monitor::Status::Done);
    CHECK(m.peek(0x24) == 0 && m.peek(0x25) == 1);
    m.setA(0xC2);
    m.call(0xFDF0);
    CHECK(m.peek(0x480) == 0xC2);
    // 25 lines scroll the page up: the first A is gone from row 0
    for (int i = 0; i < 24; ++i) {
        m.setA(0x8D);
        m.call(0xFDF0);
    }
    CHECK(m.peek(0x400) != 0xC1);
    // RDKEY waits without a key; the cursor is a flashing character; a key ends the wait
    CHECK(m.call(0xFD0C) == core::Monitor::Status::Waiting);
    m.spin(1000);
    m.pressKey(0xC1);
    CHECK(m.resume() == core::Monitor::Status::Done);
    CHECK(m.a() == 0xC1);
}

TEST(cpu6502_cycle_counts) {
    bool ok = false;
    core::Monitor m = makeMonitor(ok);
    if (!ok) {
        std::printf("    SKIPPED (the ROM dump is not in \"Apple II roms/\")\n");
        return;
    }
    // CLREOL ($FC9C) on an empty line: LDY CH, LDA #$A0, loop STA (BASL),Y / INY / CPY WNDWDTH / BCC; RTS
    m.call(0xFC9C);                               // CH = 0: 40 passes
    const std::int64_t c = m.cycles();
    // 3 + 2 + 40 x (6 + 2 + 3 + 3) - 1 (the last branch is not taken) + 6 = 5 + 40 * 14 - 1 + 6, the JSR of the caller excluded (the call pushes the return address for free)
    CHECK(c == 3 + 2 + 40 * (6 + 2 + 3 + 3) - 1 + 6);
}
