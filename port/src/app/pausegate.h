// TASK-034-3 (modernization): holds the game threads while the pause menu is open. The game loops call update() once per pass: while it returns true they do nothing else (no cycles
// are earned, nothing is drawn or played), so the game state is exactly what it was when the menu opened and continues from that exact point. The cycle clock is suspended, so the
// time spent in the menu is never "made up" afterwards (no speed-up on resume). The hooks pause and resume the sound.
#pragma once

#include <cstdint>
#include <functional>

#include "../core/cycleclock.h"

namespace pacman::app {

class PauseGate {
public:
    std::function<void()> onPause, onResume;

    // `open`: the menu is open now. Returns true while the game must stand still.
    bool update(bool open, std::int64_t nowNs, core::CycleClock& clock) {
        if (open && !held_) {
            held_ = true;
            clock.suspend(nowNs);
            if (onPause) onPause();
        } else if (!open && held_) {
            held_ = false;
            clock.resume(nowNs);
            if (onResume) onResume();
        }
        return held_;
    }
    bool held() const noexcept { return held_; }

private:
    bool held_ = false;
};

}  // namespace pacman::app
