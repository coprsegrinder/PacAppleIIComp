// TASK-034-11 (modernization): the FPS setting. It is the rate at which the window shows the game's pictures; the game itself (the simulation, its clock, the sound) runs from the virtual
// 1,022,727 Hz cycle clock on its own thread and never depends on it (docs/MODERNIZATION.md Section 11).
//   Native    the Apple II's own picture rate: one picture per video frame of 17,030 cycles (262 lines x 65 cycles) = 60.05 pictures a second on the game's cycle clock
//   30 / 60 / 120 / 144   that many pictures a second at most
//   Unlocked  a picture as soon as the game has a new one (no cap)
#pragma once

#include <cstdint>
#include <string>

namespace pacman::app {

enum class FpsMode { Native, Fps30, Fps60, Fps120, Fps144, Unlocked };

inline constexpr int kFpsModeCount = 6;
inline constexpr std::int64_t kNativeFrameCycles = 262 * 65;            // one Apple II video frame
inline constexpr std::int64_t kNativeCyclesPerSecond = 1022727;         // the game's cycle clock (core/cycleclock.h)

inline const char* fpsModeKey(FpsMode m) noexcept {   // the settings file's words
    switch (m) {
    case FpsMode::Native: return "native";
    case FpsMode::Fps30: return "30";
    case FpsMode::Fps60: return "60";
    case FpsMode::Fps120: return "120";
    case FpsMode::Fps144: return "144";
    case FpsMode::Unlocked: return "unlocked";
    }
    return "native";
}

inline const char* fpsModeLabel(FpsMode m) noexcept {   // the menu's words
    switch (m) {
    case FpsMode::Native: return "NATIVE";
    case FpsMode::Fps30: return "30 FPS";
    case FpsMode::Fps60: return "60 FPS";
    case FpsMode::Fps120: return "120 FPS";
    case FpsMode::Fps144: return "144 FPS";
    case FpsMode::Unlocked: return "UNLOCKED";
    }
    return "NATIVE";
}

inline bool parseFpsMode(const std::string& v, FpsMode& out) {
    for (int i = 0; i < kFpsModeCount; ++i)
        if (v == fpsModeKey(static_cast<FpsMode>(i))) {
            out = static_cast<FpsMode>(i);
            return true;
        }
    return false;
}

inline FpsMode nextFpsMode(FpsMode m, int dir) noexcept {
    return static_cast<FpsMode>(((static_cast<int>(m) + dir) % kFpsModeCount + kFpsModeCount) % kFpsModeCount);
}

// Nanoseconds between two pictures; 0 = no cap (Unlocked).
inline std::int64_t fpsPeriodNs(FpsMode m) noexcept {
    switch (m) {
    case FpsMode::Native: return kNativeFrameCycles * 1'000'000'000LL / kNativeCyclesPerSecond;
    case FpsMode::Fps30: return 1'000'000'000LL / 30;
    case FpsMode::Fps60: return 1'000'000'000LL / 60;
    case FpsMode::Fps120: return 1'000'000'000LL / 120;
    case FpsMode::Fps144: return 1'000'000'000LL / 144;
    case FpsMode::Unlocked: return 0;
    }
    return 0;
}

}  // namespace pacman::app
