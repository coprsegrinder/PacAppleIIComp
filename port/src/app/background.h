// TASK-034-7 (modernization): what is drawn around the game picture (the bars of a window or a full screen). The renderers ask this file for the choice; the glow itself is in glow.h
// and the shader. The picture inside the viewport is never touched by any background.
#pragma once

#include <string>

namespace pacman::app {

enum class Background { Black, AmbientGlow, CrtBezel, Desktop };

inline constexpr int kBackgroundCount = 4;

inline const char* backgroundKey(Background b) noexcept {   // the settings file's words
    switch (b) {
    case Background::Black: return "black";
    case Background::AmbientGlow: return "glow";
    case Background::CrtBezel: return "bezel";
    case Background::Desktop: return "desktop";
    }
    return "black";
}

inline const char* backgroundLabel(Background b) noexcept {   // the menu's words
    switch (b) {
    case Background::Black: return "BLACK";
    case Background::AmbientGlow: return "AMBIENT GLOW";
    case Background::CrtBezel: return "APPLE II CRT BEZEL";
    case Background::Desktop: return "APPLE II DESKTOP";
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
