// TASK-034-VIBRATION (modernization): controller vibration. The setting (Off / Low / Medium / High, kept in the settings file) scales a few short patterns that the game window starts on
// the game's events (a power pill eaten, a ghost eaten, Pac-Man caught, a level completed, the game over). The patterns are plain data (a list of steps: how hard each motor runs and for how
// long) so that the tests can check them; GamepadHub::rumble plays them on the controller in use (gamepad.h). Nothing here touches the game.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pacman::app {

enum class VibrationLevel { Off, Low, Medium, High };

inline constexpr int kVibrationLevelCount = 4;

inline const char* vibrationKey(VibrationLevel l) noexcept {   // the settings file's words
    switch (l) {
    case VibrationLevel::Off: return "off";
    case VibrationLevel::Low: return "low";
    case VibrationLevel::Medium: return "medium";
    case VibrationLevel::High: return "high";
    }
    return "medium";
}

inline const char* vibrationLabel(VibrationLevel l) noexcept {   // the menu's words
    switch (l) {
    case VibrationLevel::Off: return "OFF";
    case VibrationLevel::Low: return "LOW";
    case VibrationLevel::Medium: return "MEDIUM";
    case VibrationLevel::High: return "HIGH";
    }
    return "MEDIUM";
}

inline bool parseVibration(const std::string& v, VibrationLevel& out) {
    for (int i = 0; i < kVibrationLevelCount; ++i)
        if (v == vibrationKey(static_cast<VibrationLevel>(i))) {
            out = static_cast<VibrationLevel>(i);
            return true;
        }
    return false;
}

inline VibrationLevel nextVibration(VibrationLevel l, int dir) noexcept {
    return static_cast<VibrationLevel>(((static_cast<int>(l) + dir) % kVibrationLevelCount + kVibrationLevelCount) % kVibrationLevelCount);
}

enum class RumbleEvent { PowerPill, GhostEaten, Caught, LevelComplete, GameOver, Test };

// One step of a pattern: the heavy (low-frequency, "left") motor and the light (high-frequency, "right") motor, 0..255, for `ms` milliseconds.
struct RumbleStep {
    std::uint8_t strong = 0, weak = 0;
    std::uint16_t ms = 0;
    bool operator==(const RumbleStep&) const = default;
};

// How strongly the setting plays the patterns (percent of the full pattern).
inline int vibrationPercent(VibrationLevel l) noexcept {
    switch (l) {
    case VibrationLevel::Off: return 0;
    case VibrationLevel::Low: return 40;
    case VibrationLevel::Medium: return 70;
    case VibrationLevel::High: return 100;
    }
    return 70;
}

// The pattern of an event at a level (empty when Off). The full-strength patterns are short on purpose: a power pill or an eaten ghost is a tap, being caught a long fading rumble.
inline std::vector<RumbleStep> rumbleFor(RumbleEvent e, VibrationLevel level) {
    const int pct = vibrationPercent(level);
    if (pct == 0) return {};
    std::vector<RumbleStep> full;
    switch (e) {
    case RumbleEvent::PowerPill: full = {{0, 170, 90}}; break;
    case RumbleEvent::GhostEaten: full = {{210, 210, 130}}; break;
    case RumbleEvent::Caught: full = {{255, 200, 420}, {160, 120, 360}, {80, 60, 320}}; break;
    case RumbleEvent::LevelComplete: full = {{0, 190, 110}, {0, 0, 90}, {0, 190, 110}}; break;
    case RumbleEvent::GameOver: full = {{255, 255, 700}, {110, 110, 500}}; break;
    case RumbleEvent::Test: full = {{180, 180, 150}}; break;
    }
    for (RumbleStep& s : full) {
        s.strong = static_cast<std::uint8_t>((s.strong * pct + 50) / 100);
        s.weak = static_cast<std::uint8_t>((s.weak * pct + 50) / 100);
    }
    return full;
}

}  // namespace pacman::app
