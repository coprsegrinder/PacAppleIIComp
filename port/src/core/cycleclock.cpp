#include "cycleclock.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pacman::core {

std::int64_t nowNanoseconds() noexcept {
#ifdef _WIN32
    static const std::int64_t freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return static_cast<std::int64_t>(f.QuadPart);
    }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    const std::int64_t ticks = static_cast<std::int64_t>(c.QuadPart);
    // split to avoid overflow: whole seconds plus the remainder
    return (ticks / freq) * 1'000'000'000 + (ticks % freq) * 1'000'000'000 / freq;
#else
    return 0;
#endif
}

CycleClock::CycleClock(std::int64_t cyclesPerSecond, std::int64_t maxCatchUpNs) noexcept
    : rate_(cyclesPerSecond > 0 ? cyclesPerSecond : kAppleCyclesPerSecond), maxCatchUpCycles_(0) {
    maxCatchUpCycles_ = cyclesIn(maxCatchUpNs > 0 ? maxCatchUpNs : kDefaultMaxCatchUpNs);
}

std::int64_t CycleClock::cyclesIn(std::int64_t ns) const noexcept {
    if (ns <= 0) return 0;
    const std::int64_t sec = ns / 1'000'000'000;
    const std::int64_t rem = ns % 1'000'000'000;  // < 1e9, times rate (about 1e6) stays far below 2^63
    return sec * rate_ + rem * rate_ / 1'000'000'000;
}

void CycleClock::start(std::int64_t nowNs) noexcept {
    startNs_ = nowNs;
    lastNs_ = nowNs;
    scheduled_ = ran_ = skipped_ = 0;
    started_ = true;
    suspended_ = false;
}

void CycleClock::suspend(std::int64_t nowNs) noexcept {
    if (!started_ || suspended_) return;
    suspended_ = true;
    suspendedAt_ = nowNs;
}

void CycleClock::resume(std::int64_t nowNs) noexcept {
    if (!started_ || !suspended_) return;
    if (nowNs > suspendedAt_) startNs_ += nowNs - suspendedAt_;  // the suspended time is cut out of the schedule
    suspended_ = false;
    if (nowNs > lastNs_) lastNs_ = nowNs;
}

std::int64_t CycleClock::cyclesDue(std::int64_t nowNs) noexcept {
    if (!started_ || suspended_) return 0;
    if (nowNs <= lastNs_) return 0;  // time standing still or going backwards
    lastNs_ = nowNs;
    const std::int64_t earned = cyclesIn(nowNs - startNs_);
    std::int64_t lag = earned - scheduled_;
    if (lag <= 0) return 0;
    std::int64_t run = lag;
    if (run > maxCatchUpCycles_) {
        skipped_ += run - maxCatchUpCycles_;
        run = maxCatchUpCycles_;
    }
    scheduled_ = earned;
    ran_ += run;
    return run;
}

}  // namespace pacman::core
