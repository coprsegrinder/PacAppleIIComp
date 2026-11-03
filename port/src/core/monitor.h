// TASK-033-4e3: the Apple II Plus with its Autostart Monitor ROM (341-0020): a 6502 (cpu6502.h), 64 KB of memory, the keyboard latch, and the ROM at $F800 that really RUNS. The
// original program does its text output and key waits through that ROM (COUT1 $FDF0, VTAB $FC22, RDKEY $FD0C, GETLN's line input $FD6F, ...), so the text screens of the port
// are made by the same ROM code, with the same cycles and the same side effects (cursor, flashing cursor character, the random counter $4E/$4F that counts while a key is awaited).
// The small pieces of the GAME's own code between the ROM calls ($4868 page clear, $485C, $4A6A, $6F3C and the print loops) are re-implemented in C++ (game text routines below)
// with the cycle counts read from the original's listing; only the ROM is run as machine code. The ROM dump is read from a file at run time (the repository's "Apple II roms" folder,
// shipped with the release as a file; user decision 2026-11-03); the game image is never part of it.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include "cpu6502.h"

namespace pacman::core {

class Monitor final : public Bus {
public:
    Monitor() : cpu_(*this) { mem_.fill(0); }
    bool loadRom(const std::string& path);                 // the 2,048 bytes of $F800-$FFFF
    bool loaded() const noexcept { return loaded_; }
    // The state the program finds when it starts its text screens (zero page of the monitor: window 0-39 x 0-23, normal text, prompt ']', the vectors of COUT1 and KEYIN, cursor at the top left)
    void reset();

    // memory and the keyboard
    std::uint8_t peek(std::uint16_t a) const noexcept { return mem_[a]; }
    void poke(std::uint16_t a, std::uint8_t v) noexcept { mem_[a] = v; }
    void pressKey(std::uint8_t code) noexcept { kbd_ = static_cast<std::uint8_t>(code | 0x80); }   // the latch ($C000): one key, a new one replaces it
    bool keyLatched() const noexcept { return (kbd_ & 0x80) != 0; }
    std::int64_t cycles() const noexcept { return cpu_.cycles; }
    void setCycles(std::int64_t c) noexcept { cpu_.cycles = c; }
    void addCycles(std::int64_t n) noexcept { cpu_.cycles += n; }       // cycles of the game's own instructions that are not run as code
    bool textPageIs2() const noexcept { return textPage2_; }

    // Calling the ROM like a JSR: the registers are loaded, the routine runs until its RTS (Done) or until it waits for a key in the loop `BIT $C000 / BPL` at $FD1B-$FD24 with no key
    // in the latch (Waiting: the state is kept; pressKey() and resume() go on).
    enum class Status { Done, Waiting, Failed };
    Status call(std::uint16_t address);
    Status resume();
    bool waiting() const noexcept { return waiting_; }
    // While a key is awaited the ROM keeps looping (and counts $4E/$4F): runs the loop for `n` more cycles without leaving the wait
    void spin(std::int64_t n);
    std::uint8_t a() const noexcept { return cpu_.a; }
    std::uint8_t x() const noexcept { return cpu_.x; }
    std::uint8_t y() const noexcept { return cpu_.y; }
    void setA(std::uint8_t v) noexcept { cpu_.a = v; }
    void setX(std::uint8_t v) noexcept { cpu_.x = v; }
    void setY(std::uint8_t v) noexcept { cpu_.y = v; }

    // ---- the game's own text routines (original addresses in the comments) ----
    void gameSetTextMode();                                 // $6F3C: JSR cost excluded; text on, page 2 (the original's last switch is page 2)
    void gameClearPage();                                   // $4868: page 2 filled with $A0, cursor ($24, $25) to 0
    Status gameVtab();                                      // $485C: JSR $FC22 (the ROM computes BASL/BASH), then the high byte of BASH = $08 + page bits
    Status gameCout(std::uint8_t c);                        // $4A6A: Return = new line through VTAB, everything else to COUT1 ($FDF0)
    Status gamePrintZ(std::span<const std::uint8_t> s, std::uint16_t tableAddress);   // $48B7: LDX #0 / LDA table,X / BEQ end / JSR $4A6A / INX / BNE loop (stops at the first zero byte)
    Status gamePrint(std::span<const std::uint8_t> s, std::uint16_t tableAddress);   // LDX #0 / LDA table,X / JSR $4A6A / INX / CPX / BCC loop (JSR of the caller excluded); the address decides the page crossings of LDA table,X

    // Bus
    std::uint8_t read(std::uint16_t a) override;
    void write(std::uint16_t a, std::uint8_t v) override;

private:
    Status run();
    static constexpr std::uint16_t kSentinel = 0x03F0;      // the return address of a call (never executed)
    std::array<std::uint8_t, 65536> mem_{};
    Cpu6502 cpu_;
    bool loaded_ = false;
    bool waiting_ = false;
    bool calling_ = false;
    bool textPage2_ = true;
    std::uint8_t kbd_ = 0;
    std::int64_t spinDebt_ = 0;
};

}  // namespace pacman::core
