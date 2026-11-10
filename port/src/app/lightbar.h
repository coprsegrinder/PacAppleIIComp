// TASK-034-LIGHTBAR (modernization): the light bar of the DualShock 4 and the DualSense. The setting (Off / Low / Medium / High, kept in the settings file) is the brightness. What the light
// shows is decided here by one pure function of the moment (lightFor): the game's own colours - the hi-res palette of the picture (violet, green, blue, orange, white; video/colour.h), never a
// colour the Apple II cannot show:
//   * playing: white (Pac-Man and the dots are white); while the ghosts are blue: blue, and in the last seconds blue and white by turns, with the timing of the ghosts' own flashing
//     (the same test as the ghost image, tickdraw.cpp ghostImageBase);
//   * a ghost eaten: a flash in that ghost's colour (ghost 1 orange, 2 violet, 3 white, 4 green: read from the oracle's pages, docs/MODERNIZATION.md); Pac-Man caught: a flash in the colour
//     of the ghost that caught him, fading out; a level completed: blue and white by turns (the maze is blue, the dots white: an invention, the port does not show the original's maze flash);
//     the game over (the last catch): the same flash in the catcher's colour, fading out more slowly;
//   * menus, the attract game and the pause menu: white, dim;
//   * Native Black & White: the same, but as shades of the phosphor (video/monochrome.h) by the luminance of each colour.
// The DualSense's five player LEDs show the lives left (Sony's own player patterns); in the menus they show the usual player 1 pattern. GamepadHub::light* feeds the scene in and writes the
// result to the controller in use (gamepad.h). Nothing here touches the game.
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include "../video/colour.h"
#include "../video/monochrome.h"

namespace pacman::app {

enum class LightbarLevel { Off, Low, Medium, High };

inline constexpr int kLightbarLevelCount = 4;

inline const char* lightbarKey(LightbarLevel l) noexcept {   // the settings file's words
    switch (l) {
    case LightbarLevel::Off: return "off";
    case LightbarLevel::Low: return "low";
    case LightbarLevel::Medium: return "medium";
    case LightbarLevel::High: return "high";
    }
    return "medium";
}

inline const char* lightbarLabel(LightbarLevel l) noexcept {   // the menu's words
    switch (l) {
    case LightbarLevel::Off: return "OFF";
    case LightbarLevel::Low: return "LOW";
    case LightbarLevel::Medium: return "MEDIUM";
    case LightbarLevel::High: return "HIGH";
    }
    return "MEDIUM";
}

inline bool parseLightbar(const std::string& v, LightbarLevel& out) {
    for (int i = 0; i < kLightbarLevelCount; ++i)
        if (v == lightbarKey(static_cast<LightbarLevel>(i))) {
            out = static_cast<LightbarLevel>(i);
            return true;
        }
    return false;
}

inline LightbarLevel nextLightbar(LightbarLevel l, int dir) noexcept {
    return static_cast<LightbarLevel>(((static_cast<int>(l) + dir) % kLightbarLevelCount + kLightbarLevelCount) % kLightbarLevelCount);
}

// The brightness of the setting (percent of the full colour).
inline int lightbarPercent(LightbarLevel l) noexcept {
    switch (l) {
    case LightbarLevel::Off: return 0;
    case LightbarLevel::Low: return 40;
    case LightbarLevel::Medium: return 70;
    case LightbarLevel::High: return 100;
    }
    return 70;
}

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb&) const = default;
};

inline Rgb paletteRgb(video::PixelClass c) noexcept {   // the colour the picture shows (the same numbers)
    const std::uint32_t v = video::colourOf(c);
    return Rgb{static_cast<std::uint8_t>((v >> 16) & 255), static_cast<std::uint8_t>((v >> 8) & 255), static_cast<std::uint8_t>(v & 255)};
}

// The ghosts' colours on the Apple II screen (ghost 0-3 = ghost 1-4 of the game).
inline Rgb ghostColour(int g) noexcept {
    static constexpr video::PixelClass k[4] = {video::PixelClass::Orange, video::PixelClass::Violet, video::PixelClass::White, video::PixelClass::Green};
    return paletteRgb(k[g & 3]);
}

inline Rgb scaleRgb(Rgb c, int percent) noexcept {
    const auto f = [&](std::uint8_t v) { return static_cast<std::uint8_t>((v * percent + 50) / 100); };
    return Rgb{f(c.r), f(c.g), f(c.b)};
}

inline Rgb mixRgb(Rgb a, Rgb b, int percentOfB) noexcept {   // a at 0, b at 100
    const auto f = [&](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>((x * (100 - percentOfB) + y * percentOfB + 50) / 100); };
    return Rgb{f(a.r, b.r), f(a.g, b.g), f(a.b, b.b)};
}

enum class LightCue : std::uint8_t { None, GhostEaten, Caught, LevelComplete, GameOver };

// Everything the light depends on.
struct LightScene {
    LightbarLevel level = LightbarLevel::Medium;
    bool mono = false;                 // Native Black & White
    bool playing = false;              // a real game is running (not the menus, not the attract game)
    bool paused = false;               // the pause menu is open
    bool frightened = false;           // the ghosts are blue ($E4)
    std::uint16_t timer = 0;           // the frightened timer $E1/$E2
    int lives = 0;                     // $58 & $7F
    LightCue cue = LightCue::None;     // the latest event and when it began (steady milliseconds)
    int cueGhost = 0;                  // the ghost of a cue (0-3): the one eaten, or the one that caught Pac-Man
    std::uint64_t cueStartMs = 0;
};

// What to put on the controller. `control` false: leave the controller's own light alone (the setting is Off).
struct LightOut {
    bool control = false;
    Rgb rgb;
    std::uint8_t leds = 0;             // the DualSense's player LEDs, bit 0 = the leftmost of five
    bool operator==(const LightOut&) const = default;
};

// The ghosts' own test (ghostImageBase): blue while the timer is above $7F, then blue when bit 4 of its low byte is set, white when not.
inline bool frightenedBlue(std::uint16_t timer) noexcept {
    const unsigned high = timer >> 8, low = timer & 255u;
    if (high != 0 || low >= 0x80) return true;
    return (low & 0x10) != 0;
}

// Sony's player patterns for 1 to 5 (the pad's own for the player it assigns): the lives left.
inline std::uint8_t playerLedsFor(int lives) noexcept {
    static constexpr std::uint8_t p[6] = {0x00, 0x04, 0x0A, 0x15, 0x1B, 0x1F};
    return p[std::clamp(lives, 0, 5)];
}

inline constexpr int kCueEatenMs = 260, kCueCaughtFlashMs = 250, kCueCaughtFadeMs = 1000, kCueFadeInMs = 400, kCueLevelStepMs = 120, kCueLevelSteps = 7, kCueGameOverMs = 1500;
inline constexpr int kDimPercent = 35;   // menus, the attract game, the pause menu

inline LightOut lightFor(const LightScene& s, std::uint64_t nowMs) noexcept {
    LightOut o;
    const int pct = lightbarPercent(s.level);
    if (pct == 0) return o;
    o.control = true;
    const Rgb white = paletteRgb(video::PixelClass::White), blue = paletteRgb(video::PixelClass::Blue);
    const bool lit = s.playing && !s.paused;
    Rgb c = white;
    if (!lit) c = scaleRgb(white, kDimPercent);
    else if (s.frightened) c = frightenedBlue(s.timer) ? blue : white;
    if (s.cue != LightCue::None && nowMs >= s.cueStartMs) {
        const std::uint64_t t = nowMs - s.cueStartMs;
        switch (s.cue) {
        case LightCue::GhostEaten:
            if (t < kCueEatenMs) c = mixRgb(ghostColour(s.cueGhost), c, static_cast<int>(t * 100 / kCueEatenMs));
            break;
        case LightCue::Caught: {
            const Rgb g = ghostColour(s.cueGhost);
            if (t < kCueCaughtFlashMs) c = g;
            else if (t < kCueCaughtFlashMs + kCueCaughtFadeMs) c = scaleRgb(g, 100 - static_cast<int>((t - kCueCaughtFlashMs) * 100 / kCueCaughtFadeMs));
            else if (t < kCueCaughtFlashMs + kCueCaughtFadeMs + kCueFadeInMs) c = scaleRgb(c, static_cast<int>((t - kCueCaughtFlashMs - kCueCaughtFadeMs) * 100 / kCueFadeInMs));
            break;
        }
        case LightCue::LevelComplete:
            if (t < static_cast<std::uint64_t>(kCueLevelStepMs) * kCueLevelSteps) c = ((t / kCueLevelStepMs) % 2 == 0) ? blue : white;
            break;
        case LightCue::GameOver: {   // the last catch: the same flash in the catcher's colour, fading out more slowly
            const Rgb g = ghostColour(s.cueGhost);
            if (t < kCueCaughtFlashMs) c = g;
            else if (t < kCueCaughtFlashMs + kCueGameOverMs) c = scaleRgb(g, 100 - static_cast<int>((t - kCueCaughtFlashMs) * 100 / kCueGameOverMs));
            else if (t < kCueCaughtFlashMs + kCueGameOverMs + kCueFadeInMs) c = scaleRgb(c, static_cast<int>((t - kCueCaughtFlashMs - kCueGameOverMs) * 100 / kCueFadeInMs));
            break;
        }
        case LightCue::None: break;
        }
    }
    c = scaleRgb(c, pct);
    if (s.mono) {   // Native Black & White: the phosphor's colour at the brightness of the colour it replaces
        const float lum = (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b) / 255.0f;
        c = Rgb{static_cast<std::uint8_t>(video::kPhosphorR * lum * 255.0f + 0.5f), static_cast<std::uint8_t>(video::kPhosphorG * lum * 255.0f + 0.5f), static_cast<std::uint8_t>(video::kPhosphorB * lum * 255.0f + 0.5f)};
    }
    o.rgb = c;
    o.leds = s.playing ? playerLedsFor(s.lives) : std::uint8_t{0x04};
    return o;
}

}  // namespace pacman::app
