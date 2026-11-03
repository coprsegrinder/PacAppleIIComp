// TASK-033-4e3: a small NMOS 6502 (documented opcodes) with the cycle counts of the real chip (page crossings of indexed reads, taken branches, stores that always pay the indexing
// cycle). It runs the monitor ROM of the Apple II Plus (monitor.h) so that the text screens of the original cost what they cost there. Ported from tools/cpu6502.py, which ran the
// original program for all recordings of this project; tests/test_monitor.cpp compares the two on the ROM routines.
#pragma once

#include <cstdint>

namespace pacman::core {

class Bus {
public:
    virtual ~Bus() = default;
    virtual std::uint8_t read(std::uint16_t address) = 0;
    virtual void write(std::uint16_t address, std::uint8_t value) = 0;
};

class Cpu6502 {
public:
    explicit Cpu6502(Bus& bus) : bus_(bus) {}
    std::uint8_t a = 0, x = 0, y = 0, sp = 0xFD, p = 0x24;
    std::uint16_t pc = 0;
    std::int64_t cycles = 0;
    int step();                      // executes one instruction, returns its cycles (0 and failed() for an undocumented opcode)
    bool failed() const noexcept { return failed_; }
    void push(std::uint8_t v) { bus_.write(static_cast<std::uint16_t>(0x100 | sp), v); --sp; }
    std::uint8_t pull() { ++sp; return bus_.read(static_cast<std::uint16_t>(0x100 | sp)); }

private:
    std::uint8_t rd(std::uint16_t a_) { return bus_.read(a_); }
    void wr(std::uint16_t a_, std::uint8_t v) { bus_.write(a_, v); }
    std::uint8_t fetch() { return rd(pc++); }
    std::uint8_t setnz(int v) noexcept;
    void flag(std::uint8_t mask, bool on) noexcept { p = on ? static_cast<std::uint8_t>(p | mask) : static_cast<std::uint8_t>(p & ~mask); }
    void adc(std::uint8_t m) noexcept;
    void sbc(std::uint8_t m) noexcept;
    void compare(std::uint8_t reg, std::uint8_t m) noexcept;
    std::uint8_t rmw(int mn, std::uint8_t v) noexcept;
    int branch(int mn);
    int implied(int mn, bool accumulator);
    Bus& bus_;
    bool failed_ = false;
};

}  // namespace pacman::core
