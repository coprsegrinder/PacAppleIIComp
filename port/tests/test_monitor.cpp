// TASK-033-4e3: the 6502 core and the monitor ROM in the port: the ROM is loaded and runs (COUT1, VTAB, scrolling, a key wait), and the cycle counts of the instructions.
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

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

// ---- TASK-034-CPU: every documented opcode against tools/cpu6502.py (the core that ran the original program), the undocumented ones refused, decimal mode by hand ----
#include "../src/core/cpu6502.h"
#include "cpu_vectors.inc"

namespace {

// The memory of the vectors: a fixed function of (seed, address), with an overlay of everything written or placed (the same arithmetic as tools/record_cpu_vectors.py).
class VectorBus : public core::Bus {
public:
    explicit VectorBus(std::uint32_t seed) : seed_(seed) {}
    std::uint8_t read(std::uint16_t a) override {
        const auto it = over_.find(a);
        if (it != over_.end()) return it->second;
        std::uint32_t h = a * 0x9E3779B1u + seed_ * 0x85EBCA6Bu + 0x1234567u;
        h ^= h >> 15;
        h *= 0x2C1B3C6Du;
        h ^= h >> 12;
        return static_cast<std::uint8_t>(h >> 8);
    }
    void write(std::uint16_t a, std::uint8_t v) override {
        over_[a] = v;
        writes.emplace_back(a, v);
    }
    void place(std::uint16_t a, std::uint8_t v) { over_[a] = v; }
    std::vector<std::pair<std::uint16_t, std::uint8_t>> writes;
private:
    std::uint32_t seed_;
    std::map<std::uint16_t, std::uint8_t> over_;
};

}  // namespace

TEST(cpu6502_every_documented_opcode_equals_the_core_that_ran_the_original) {
    std::size_t n = 0, bad = 0;
    for (const auto& v : kCpuVec) {
        VectorBus bus(static_cast<std::uint32_t>(v[1]));
        core::Cpu6502 cpu(bus);
        cpu.a = static_cast<std::uint8_t>(v[2]);
        cpu.x = static_cast<std::uint8_t>(v[3]);
        cpu.y = static_cast<std::uint8_t>(v[4]);
        cpu.p = static_cast<std::uint8_t>(v[5]);
        cpu.sp = static_cast<std::uint8_t>(v[6]);
        cpu.pc = static_cast<std::uint16_t>(v[7]);
        // (the writes of the instruction's own set-up come after the overlay: the python core placed the bytes first, then ran)
        bus.place(cpu.pc, static_cast<std::uint8_t>(v[0]));
        bus.place(static_cast<std::uint16_t>(cpu.pc + 1), static_cast<std::uint8_t>(v[8]));
        bus.place(static_cast<std::uint16_t>(cpu.pc + 2), static_cast<std::uint8_t>(v[9]));
        const int cyc = cpu.step();
        bool same = !cpu.failed() && cpu.a == v[10] && cpu.x == v[11] && cpu.y == v[12] && cpu.p == v[13] && cpu.sp == v[14] && cpu.pc == v[15] && cyc == v[16] && cpu.cycles == v[16];
        for (int i = 0; i < 3; ++i) {
            const int wa = v[17 + 2 * i], wv = v[18 + 2 * i];
            if (wa < 0) same = same && bus.writes.size() <= static_cast<std::size_t>(i);
            else same = same && bus.writes.size() > static_cast<std::size_t>(i) && bus.writes[static_cast<std::size_t>(i)].first == wa && bus.writes[static_cast<std::size_t>(i)].second == wv;
        }
        ++n;
        if (!same) {
            ++bad;
            if (bad <= 6)
                std::printf("    differs: opcode %02X seed %d a %d x %d y %d p %02X sp %d pc %d | mine a %d x %d y %d p %02X sp %d pc %d cycles %d | python a %d x %d y %d p %02X sp %d pc %d cycles %d\n", v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7],
                            cpu.a, cpu.x, cpu.y, cpu.p, cpu.sp, cpu.pc, cyc, v[10], v[11], v[12], v[13], v[14], v[15], v[16]);
        }
    }
    std::printf("    %zu runs of the 151 documented opcodes compared with the core that ran the original, %zu differ\n", n, bad);
    CHECK(n >= 151 * 24 && bad == 0);
}

TEST(cpu6502_refuses_every_undocumented_opcode_and_leaves_the_registers_alone) {
    int refused = 0;
    for (int op = 0; op < 256; ++op) {
        if (kDocumented[op]) continue;
        VectorBus bus(77);
        core::Cpu6502 cpu(bus);
        cpu.pc = 0x2000;
        cpu.a = 0x12;
        cpu.x = 0x34;
        cpu.y = 0x56;
        bus.place(0x2000, static_cast<std::uint8_t>(op));
        const int cyc = cpu.step();
        CHECK(cpu.failed() && cyc == 0 && cpu.a == 0x12 && cpu.x == 0x34 && cpu.y == 0x56 && bus.writes.empty());
        ++refused;
    }
    CHECK(refused == 105);
}

TEST(cpu6502_decimal_mode_adds_and_subtracts_as_the_nmos_chip_does) {
    struct Case { bool sub; std::uint8_t a, m; bool carryIn; std::uint8_t result; bool carryOut; };
    // known results of the NMOS 6502 in decimal mode (BCD arithmetic)
    const Case cases[] = {
        {false, 0x09, 0x01, false, 0x10, false}, {false, 0x99, 0x01, false, 0x00, true}, {false, 0x45, 0x55, false, 0x00, true}, {false, 0x58, 0x46, true, 0x05, true},
        {false, 0x12, 0x34, false, 0x46, false}, {true, 0x10, 0x01, true, 0x09, true}, {true, 0x00, 0x01, true, 0x99, false}, {true, 0x46, 0x12, true, 0x34, true},
        {true, 0x40, 0x13, true, 0x27, true}, {true, 0x32, 0x02, false, 0x29, true},
    };
    for (const Case& c : cases) {
        VectorBus bus(5);
        core::Cpu6502 cpu(bus);
        cpu.pc = 0x3000;
        cpu.a = c.a;
        cpu.p = static_cast<std::uint8_t>(0x20 | 0x08 | (c.carryIn ? 1 : 0));   // D set
        bus.place(0x3000, c.sub ? 0xE9 : 0x69);                               // SBC # / ADC #
        bus.place(0x3001, c.m);
        CHECK(cpu.step() == 2);
        CHECK_MSG(cpu.a == c.result && ((cpu.p & 1) != 0) == c.carryOut, std::string(c.sub ? "SBC " : "ADC ") + std::to_string(c.a) + "," + std::to_string(c.m));
    }
}

TEST(cpu6502_known_chip_behaviours_jmp_indirect_wrap_brk_rti_php_plp_stack) {
    // JMP ($10FF) takes the high byte from $1000, not $1100 (the NMOS page-wrap bug)
    {
        VectorBus bus(1);
        core::Cpu6502 cpu(bus);
        cpu.pc = 0x2000;
        bus.place(0x2000, 0x6C);
        bus.place(0x2001, 0xFF);
        bus.place(0x2002, 0x10);
        bus.place(0x10FF, 0x34);
        bus.place(0x1000, 0x12);
        bus.place(0x1100, 0x99);
        CHECK(cpu.step() == 5 && cpu.pc == 0x1234);
    }
    // BRK: pushes PC+2 and the status with B set, sets I, jumps through $FFFE; RTI restores the status (B ignored) and the address; 7 and 6 cycles
    {
        VectorBus bus(2);
        core::Cpu6502 cpu(bus);
        cpu.pc = 0x2000;
        cpu.sp = 0xFF;
        cpu.p = 0x20 | 0x01;   // C set
        bus.place(0x2000, 0x00);
        bus.place(0xFFFE, 0x00);
        bus.place(0xFFFF, 0x40);
        CHECK(cpu.step() == 7 && cpu.pc == 0x4000 && cpu.sp == 0xFC && (cpu.p & 0x04) != 0);
        CHECK(bus.writes.size() == 3 && bus.writes[0] == std::make_pair(std::uint16_t{0x1FF}, std::uint8_t{0x20}) && bus.writes[1] == std::make_pair(std::uint16_t{0x1FE}, std::uint8_t{0x02}) &&
              bus.writes[2].first == 0x1FD && bus.writes[2].second == (0x20 | 0x10 | 0x01));
        bus.place(0x4000, 0x40);   // RTI
        CHECK(cpu.step() == 6 && cpu.pc == 0x2002 && cpu.sp == 0xFF && (cpu.p & 0x10) == 0 && (cpu.p & 0x01) != 0 && (cpu.p & 0x04) == 0);
    }
    // PHP pushes the status with B and bit 5 set; PLP loads it without B; PHA / PLA move a value through the stack and set the flags
    {
        VectorBus bus(3);
        core::Cpu6502 cpu(bus);
        cpu.pc = 0x2000;
        cpu.sp = 0xF0;
        cpu.p = 0x20 | 0x80;   // N
        cpu.a = 0;
        bus.place(0x2000, 0x08);   // PHP
        bus.place(0x2001, 0x28);   // PLP
        bus.place(0x2002, 0x48);   // PHA
        bus.place(0x2003, 0x68);   // PLA
        CHECK(cpu.step() == 3 && bus.writes.back().second == (0x80 | 0x20 | 0x10) && cpu.sp == 0xEF);
        cpu.p = 0x20;
        CHECK(cpu.step() == 4 && (cpu.p & 0x10) == 0 && (cpu.p & 0x80) != 0 && cpu.sp == 0xF0);
        CHECK(cpu.step() == 3 && cpu.sp == 0xEF);
        cpu.a = 0x55;
        CHECK(cpu.step() == 4 && cpu.a == 0 && (cpu.p & 0x02) != 0 && cpu.sp == 0xF0);   // PLA pulls the 0 that PHA pushed: Z set
    }
}
