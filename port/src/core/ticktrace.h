// Reader for the per-tick oracle recordings made by tools/record_ticks.py (TASK-029a). The files are LOCAL test inputs derived from original data
// (git-ignored); the format is described in that tool. The C++ game-core pieces are tested by running them on the oracle's state at the start of a
// tick and comparing the fields they own with the oracle's state at the next tick (docs/M4_PLAN.md section 2).
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pacman::core {

// The logical state of one tick, exactly as the golden traces define it (docs/GOLDEN_TRACES.md).
inline constexpr std::size_t kStateBytes = 1010;
inline constexpr std::size_t kZeroPageBytes = 30;   // $52-$6F
inline constexpr std::size_t kGhostRecordsAt = 34;  // $84-$D3, 80 bytes
inline constexpr std::size_t kMapAt = 114;          // $623A-$65B9, 896 bytes

// Where a zero-page address lives inside a state (or -1 for an address that is not part of the logical state).
int stateOffsetOf(std::uint16_t address) noexcept;

struct TickInput {
    std::uint32_t tick = 0;
    std::uint32_t key = 0;  // the key code the original received ($C1 = 'A', ...)
};
struct TickPoke {
    std::uint32_t tick = 0, address = 0, value = 0;  // applied after the keys of that tick
};

class TickTrace {
public:
    // Empty trace and a reason if the file is missing, truncated or inconsistent.
    static TickTrace load(const std::string& path, std::string& reason);

    bool ok() const noexcept { return !states_.empty(); }
    std::size_t ticks() const noexcept { return states_.size() / kStateBytes; }
    std::span<const std::uint8_t> state(std::size_t tick) const noexcept;
    std::uint8_t at(std::size_t tick, std::uint16_t address) const noexcept;  // value of a logical-state address at the start of a tick (0 if not part of it)
    const std::vector<TickInput>& inputs() const noexcept { return inputs_; }
    const std::vector<TickPoke>& pokes() const noexcept { return pokes_; }
    const std::array<std::uint8_t, 32>& goldenFinalHash() const noexcept { return golden_; }

    // SHA-256 chain h(t) = SHA-256(h(t-1) + state(t)) over all ticks, as the golden traces define it.
    std::array<std::uint8_t, 32> chainHash() const;

private:
    std::vector<std::uint8_t> states_;
    std::vector<TickInput> inputs_;
    std::vector<TickPoke> pokes_;
    std::array<std::uint8_t, 32> golden_{};
};

}  // namespace pacman::core
