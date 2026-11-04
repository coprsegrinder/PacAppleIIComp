// TASK-034-7 (modernization): what is drawn around the game picture (the bars of a window or a full screen). The renderers ask this file for the choice; the glow itself is in glow.h
// and the shader. The picture inside the viewport is never touched by any background.
#pragma once

#include <string>

namespace pacman::app {

enum class Background { Black, AmbientGlow };   // the CRT bezel and the Apple II desktop of the specification follow when their artwork is decided (docs/MODERNIZATION.md section 7)

inline constexpr int kBackgroundCount = 2;

inline const char* backgroundKey(Background b) noexcept {   // the settings file's words
    switch (b) {
    case Background::Black: return "black";
    case Background::AmbientGlow: return "glow";
    }
    return "black";
}

inline const char* backgroundLabel(Background b) noexcept {   // the menu's words
    switch (b) {
    case Background::Black: return "BLACK";
    case Background::AmbientGlow: return "AMBIENT GLOW";
    }
    return "BLACK";
}

inline bool parseBackground(const std::string& v, Background& out) {
    for (int i = 0; i < kBackgroundCount; ++i)
        if (v == backgroundKey(static_cast<Background>(i))) {
            out = static_cast<Background>(i);
            return true;
        }
    return false;
}

inline Background nextBackground(Background b, int dir) noexcept {
    return static_cast<Background>(((static_cast<int>(b) + dir) % kBackgroundCount + kBackgroundCount) % kBackgroundCount);
}

// What the old (GDI) drawing fills the bars with: black for every background (the glow needs Direct3D).
struct BackgroundFill {
    float r = 0, g = 0, b = 0;
};

inline BackgroundFill backgroundFill(Background) noexcept { return {}; }

}  // namespace pacman::app
