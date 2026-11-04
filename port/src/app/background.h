// TASK-034-1 (modernization): what is drawn around the game picture (the bars of a window or a full screen). Section 7 adds the other options (ambient glow, CRT bezel, Apple II
// desktop); the renderers ask this file for the fill, so they need no change when an option is added.
#pragma once

namespace pacman::app {

enum class Background { Black };

struct BackgroundFill {
    float r = 0, g = 0, b = 0;
};

inline BackgroundFill backgroundFill(Background which) noexcept {
    switch (which) {
    case Background::Black: break;
    }
    return {};
}

}  // namespace pacman::app
