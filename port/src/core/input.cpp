#include "input.h"

namespace pacman::core {

namespace {
constexpr std::size_t kMaxQueued = 64;
}

void InputPoller::pushKey(std::uint8_t code) {
    if (queue_.size() < kMaxQueued) queue_.push_back(static_cast<std::uint8_t>(code | 0x80));
}

// $C000 / $C010: the oracle's keyboard hands out one waiting key per read of the strobe (a queue); real hardware keeps only the newest key.
bool InputPoller::latch(std::uint8_t& code) {
    if (queue_.empty()) return false;
    code = queue_.front();
    queue_.pop_front();
    return true;
}

PollResult InputPoller::poll(GameState& state, const KeyConfig& keys, const PortInput& port) {
    PollResult r;
    const bool demo = state.demo != 0;
    std::optional<std::uint8_t> direction;

    // ---- stage 1: $6F77 ----
    std::uint8_t code = 0;
    bool have = latch(code);
    if (paused_) {                                   // inside the Esc wait loop at $6F9A: only a key leaves it, and that key is then handled below
        if (!have) {
            r.paused = true;
            return r;
        }
        paused_ = false;
    } else if (have && code == 0x93) {               // Ctrl-S: flip the two speaker instructions
        soundOn_ = !soundOn_;
        r.soundToggled = true;
        have = false;
    } else if (have && code == 0x9B) {               // Esc: wait for the next key
        std::uint8_t next = 0;
        if (!latch(next)) {
            paused_ = true;
            r.paused = true;
            return r;
        }
        code = next;                                 // the key that ends the pause is processed like a normal key (A is reloaded at $6F9A)
    }
    if (have) {
        if (demo) {
            if (code == 0xA0) r.leaveToPlayersPrompt = true;
        } else if (code == keys.up) {
            direction = static_cast<std::uint8_t>(1);
        } else if (code == keys.right) {
            direction = static_cast<std::uint8_t>(2);
        } else if (code == keys.left) {
            direction = static_cast<std::uint8_t>(3);
        } else if (code == keys.down) {
            direction = static_cast<std::uint8_t>(0);
        }
    }
    if (direction) {
        state.requestedDirection() = *direction;     // STA $63
        r.directionSet = true;
    }

    // ---- stage 2: $6FD0 (keyboard mode) or $7041 (joystick mode) ----
    std::optional<std::uint8_t> second;
    if (!demo) {
        if (port.joystickMode) {
            second = port.joystickDirection;
        } else {
            switch (port.switches & 15) {  // table at $700E
                case 1: second = static_cast<std::uint8_t>(0); break;
                case 2: second = static_cast<std::uint8_t>(1); break;
                case 4: second = static_cast<std::uint8_t>(2); break;
                case 8: second = static_cast<std::uint8_t>(3); break;
                default: break;
            }
        }
    }
    if (second) {
        state.requestedDirection() = *second;
        r.directionSet = true;
    }
    return r;
}

}  // namespace pacman::core
