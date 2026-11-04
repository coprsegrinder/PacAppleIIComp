// TASK-034-11 (modernization): a sleep that wakes up when asked. std::this_thread::sleep_for(2 ms) on this Windows sleeps about 15.6 ms (measured: docs/MODERNIZATION.md Section 11), so the game's
// worker loop woke up only 64 times a second and handed out its pictures in bursts of about 5 game ticks. A high-resolution waitable timer wakes within about 0.3 ms. The game's speed never
// depended on this (the cycle clock counts real time and catches up exactly); only how finely the pictures and the keyboard are served does.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>
#include <thread>

namespace pacman::app {

class PreciseTimer {
public:
    PreciseTimer() {
        h_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);   // Windows 10 1803 and later
    }
    ~PreciseTimer() {
        if (h_) CloseHandle(h_);
    }
    PreciseTimer(const PreciseTimer&) = delete;
    PreciseTimer& operator=(const PreciseTimer&) = delete;
    HANDLE handle() const noexcept { return h_; }
    void sleepNs(long long ns) {   // waits at least about `ns`; falls back to the ordinary sleep where the high-resolution timer does not exist
        if (ns < 100) ns = 100;
        if (!h_) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
            return;
        }
        LARGE_INTEGER due;
        due.QuadPart = -ns / 100;
        SetWaitableTimer(h_, &due, 0, nullptr, nullptr, FALSE);
        WaitForSingleObject(h_, INFINITE);
    }
private:
    HANDLE h_ = nullptr;
};

inline void preciseSleepMs(int ms) {
    thread_local PreciseTimer t;
    t.sleepNs(static_cast<long long>(ms) * 1'000'000LL);
}

}  // namespace pacman::app
