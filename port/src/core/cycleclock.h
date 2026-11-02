// The virtual cycle clock (TASK-028l, design in docs/WINDOW_DESIGN.md W2): turns real time into "how many 6502 cycles may the game run now".
// The faithful mode runs the game logic at 1,022,727 cycles per second, so the slowdowns of the original (which come from how many cycles each pass of
// its main loop costs, see ARCHITECTURE.md) are kept. The clock takes the time as a parameter (nanoseconds), so tests use a fake clock.
// Rules: exact integer arithmetic (no drift); never ahead of real time; if the PC stalls, at most maxCatchUp of the missed time is run and the rest is
// dropped (counted in skipped()); time that goes backwards is ignored; suspend/resume lets no time accumulate while suspended.
#pragma once

#include <cstdint>

namespace pacman::core {

inline constexpr std::int64_t kAppleCyclesPerSecond = 1022727;  // 14.31818 MHz / 14, the figure used in docs/ARCHITECTURE.md (the real machine's
                                                                // stretched 65th cycle makes it about 1,020,484 effective; not modelled, U)
inline constexpr std::int64_t kDefaultMaxCatchUpNs = 100'000'000;  // 100 ms

// Real time in nanoseconds from a steady source (QueryPerformanceCounter on Windows).
std::int64_t nowNanoseconds() noexcept;

class CycleClock {
public:
    explicit CycleClock(std::int64_t cyclesPerSecond = kAppleCyclesPerSecond, std::int64_t maxCatchUpNs = kDefaultMaxCatchUpNs) noexcept;

    void start(std::int64_t nowNs) noexcept;     // the schedule begins now; counters are reset
    void suspend(std::int64_t nowNs) noexcept;   // e.g. window minimised: nothing accumulates until resume
    void resume(std::int64_t nowNs) noexcept;

    // Cycles to run now (>= 0). Call it as often as you like; the sum over many calls never exceeds the exact figure for the elapsed time.
    std::int64_t cyclesDue(std::int64_t nowNs) noexcept;

    std::int64_t ran() const noexcept { return ran_; }          // cycles handed out so far (the virtual cycle counter; use it to stamp sound clicks)
    std::int64_t skipped() const noexcept { return skipped_; }  // cycles dropped because the caller fell too far behind
    bool suspended() const noexcept { return suspended_; }
    std::int64_t cyclesPerSecond() const noexcept { return rate_; }

    // Exact whole cycles in `ns` nanoseconds at this clock's rate (floor), without overflow for any ns up to about 10^12 years... of int64 range / rate.
    std::int64_t cyclesIn(std::int64_t ns) const noexcept;

private:
    std::int64_t rate_;
    std::int64_t maxCatchUpCycles_;
    std::int64_t startNs_ = 0;
    std::int64_t scheduled_ = 0;   // cycles that real time has "earned" so far and that were handed out or skipped
    std::int64_t ran_ = 0;
    std::int64_t skipped_ = 0;
    std::int64_t lastNs_ = 0;
    bool started_ = false;
    bool suspended_ = false;
    std::int64_t suspendedAt_ = 0;
};

}  // namespace pacman::core
