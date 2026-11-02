// Tests of the virtual cycle clock (src/core/cycleclock): exactness, no drift, the catch-up cap, suspend/resume, backwards time, and a run against the
// real clock. The first group uses a fake clock (plain nanosecond numbers), so it is fast and deterministic.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <random>

#include "../src/core/cycleclock.h"
#include "test_framework.h"

using namespace pacman::core;

namespace {
constexpr std::int64_t kMs = 1'000'000;
constexpr std::int64_t kSec = 1'000'000'000;
}  // namespace

TEST(cycleclock_exact_and_drift_free_with_regular_steps) {
    CycleClock c;
    c.start(0);
    std::int64_t total = 0;
    for (std::int64_t t = kMs; t <= 10 * kSec; t += kMs) total += c.cyclesDue(t);  // 1 ms steps for 10 s
    CHECK(total == 10 * kAppleCyclesPerSecond);  // exactly 10,227,270, no rounding loss
    CHECK(c.ran() == total && c.skipped() == 0);
    // one hour in one go would exceed the cap, so build the hour from small steps: still exact
    CycleClock d;
    d.start(5 * kSec);  // a start time other than zero
    std::int64_t sum = 0;
    for (std::int64_t t = 1; t <= 72000; ++t) sum += d.cyclesDue(5 * kSec + t * 50 * kMs);  // 50 ms steps (a bigger step than the 100 ms cap would be dropped by design)
    CHECK(d.skipped() == 0);
    CHECK(sum == 3600 * kAppleCyclesPerSecond);
}

TEST(cycleclock_values_are_floored_exactly) {
    CycleClock c;
    // independent expected values (1,022,727 cycles per second, always rounded DOWN so the clock is never ahead)
    CHECK(c.cyclesIn(1) == 0);
    CHECK(c.cyclesIn(977) == 0);                  // 0.9993
    CHECK(c.cyclesIn(978) == 1);                  // 1.0002
    CHECK(c.cyclesIn(kMs) == 1022);                 // 1022.727
    CHECK(c.cyclesIn(1'500'000'000) == 1534090);    // 1534090.5
    CHECK(c.cyclesIn(kSec) == 1022727);
    CycleClock d;
    d.start(0);
    CHECK(d.cyclesDue(kMs) == 1022);                // not 1023
    CHECK(d.cyclesDue(2 * kMs) == 1023);            // 2045.454 -> 2045 in total
    CHECK(d.ran() == 2045);
}

TEST(cycleclock_irregular_steps_give_the_same_total) {
    CycleClock c;
    c.start(0);
    std::mt19937 rng(7);
    std::int64_t t = 0, total = 0;
    while (t < 20 * kSec) {
        t += 1 + static_cast<std::int64_t>(rng() % (30 * kMs));  // 1 ns to 30 ms steps, all within the cap
        total += c.cyclesDue(t);
    }
    CHECK(total == c.cyclesIn(t));
    CHECK(c.skipped() == 0);
}

TEST(cycleclock_never_runs_ahead_of_real_time) {
    CycleClock c;
    c.start(1000);
    std::int64_t total = 0;
    for (std::int64_t t = 1000; t < 1000 + 2 * kSec; t += 333'333) {
        total += c.cyclesDue(t);
        CHECK(total <= c.cyclesIn(t - 1000));
    }
}

TEST(cycleclock_catch_up_is_capped_and_the_rest_dropped) {
    CycleClock c;
    c.start(0);
    CHECK(c.cyclesDue(50 * kMs) == c.cyclesIn(50 * kMs));  // a 50 ms lag is below the cap: run in full
    const std::int64_t before = c.ran();
    const std::int64_t run = c.cyclesDue(50 * kMs + 5 * kSec);  // the PC stalled for 5 s
    CHECK(run == c.cyclesIn(kDefaultMaxCatchUpNs));             // only 100 ms worth is run
    CHECK(c.ran() == before + run);
    CHECK(c.skipped() > 0);
    CHECK(c.ran() + c.skipped() == c.cyclesIn(50 * kMs + 5 * kSec));  // nothing is lost from the books
    // after the stall the clock continues normally, not in a burst
    const std::int64_t next = c.cyclesDue(50 * kMs + 5 * kSec + 10 * kMs);
    CHECK(next == c.cyclesIn(50 * kMs + 5 * kSec + 10 * kMs) - c.cyclesIn(50 * kMs + 5 * kSec));
}

TEST(cycleclock_suspend_and_resume_cut_out_the_time) {
    CycleClock c;
    c.start(0);
    c.cyclesDue(100 * kMs);
    c.suspend(100 * kMs);
    CHECK(c.suspended());
    CHECK(c.cyclesDue(40 * kSec) == 0);  // nothing while suspended
    c.resume(40 * kSec);
    CHECK(!c.suspended());
    const std::int64_t after = c.cyclesDue(40 * kSec + 10 * kMs);
    CHECK(after == c.cyclesIn(10 * kMs));  // only the 10 ms after resuming
    CHECK(c.skipped() == 0);
}

TEST(cycleclock_ignores_time_going_backwards_and_unstarted_use) {
    CycleClock c;
    CHECK(c.cyclesDue(12345) == 0);  // not started
    c.start(1000 * kMs);
    CHECK(c.cyclesDue(1010 * kMs) == c.cyclesIn(10 * kMs));
    CHECK(c.cyclesDue(1005 * kMs) == 0);  // backwards
    CHECK(c.cyclesDue(1010 * kMs) == 0);  // standing still
    CHECK(c.cyclesDue(1020 * kMs) == c.cyclesIn(10 * kMs));
    CycleClock odd(0, -5);  // invalid arguments fall back to the defaults
    CHECK(odd.cyclesPerSecond() == kAppleCyclesPerSecond);
}

TEST(cycleclock_overflow_safe_for_very_long_times) {
    CycleClock c(kAppleCyclesPerSecond, 1'000'000'000'000'000LL);  // huge cap so one call can cover a long time
    c.start(0);
    const std::int64_t day = 86400 * kSec;
    CHECK(c.cyclesIn(day) == 86400 * kAppleCyclesPerSecond);
    CHECK(c.cyclesIn(365 * day) == 365 * 86400 * kAppleCyclesPerSecond);
    CHECK(c.cyclesIn(-5) == 0);
}

TEST(cycleclock_real_clock_run) {
    CycleClock c;
    const std::int64_t t0 = nowNanoseconds();
    c.start(t0);
    std::int64_t total = 0, last = t0;
    while (last - t0 < 500 * kMs) {
        Sleep(1);
        last = nowNanoseconds();
        total += c.cyclesDue(last);
    }
    const std::int64_t end = nowNanoseconds();
    total += c.cyclesDue(end);
    const std::int64_t exact = c.cyclesIn(end - t0);
    CHECK(total <= exact);              // never ahead of real time
    CHECK(total == c.ran());
    CHECK(c.skipped() == 0);            // a Sleep(1) loop never falls 100 ms behind
    CHECK(exact - total == 0);          // everything earned up to `end` was handed out
    CHECK(total > c.cyclesIn(450 * kMs) && total < c.cyclesIn(700 * kMs));  // about half a second of cycles (the Sleep loop ends between 500 and 520 ms)
    CHECK(nowNanoseconds() >= end);     // the source is monotonic
}
