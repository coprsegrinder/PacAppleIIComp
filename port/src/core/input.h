// The input poll of the original (TASK-029c): routine $6F5E and its two stages, rebuilt from the disassembly and checked against the oracle
// (docs/CONTROLS.md has the key table). The original calls it at the start of EVERY tick in keyboard mode ($7861: JSR $6F5E before DEC $59).
//   stage 1 ($6F77): read the keyboard latch once. Ctrl-S ($93) toggles the sound; Esc ($9B) waits (blocking!) for the next key, which is then treated
//                    as the key of this poll; in the demo only Space ($A0) matters (it leaves to the players prompt); in a game the four direction keys
//                    ($C1 up = 1, $95 right = 2, $88 left = 3, $DA down = 0; the codes can be changed by the "customised keys" prompts) set the direction.
//   stage 2 (keyboard mode, $6FD0): in a game, four switches on the game port can also give a direction (mask 1 = 0 down, 2 = 1 up, 4 = 2 right,
//                    8 = 3 left; every other mask nothing). Joystick mode ($7041, analog paddle timing) is NOT rebuilt: its result is passed in.
// Both stages write the requested direction $63 when they find a direction; stage 2 runs after stage 1 and wins.
#pragma once

#include <cstdint>
#include <deque>
#include <optional>

#include "gamestate.h"

namespace pacman::core {

struct KeyConfig {
    std::uint8_t up = 0xC1, right = 0x95, left = 0x88, down = 0xDA;  // Apple II codes of the direction keys (defaults: A, Ctrl-U, Ctrl-H, Z)
};

struct PortInput {
    bool joystickMode = false;                    // $F5 <> 0 (mode 2 of the mode screen)
    std::uint8_t switches = 0;                    // keyboard mode: game-port switch mask (bits 1, 2, 4, 8)
    std::optional<std::uint8_t> joystickDirection;  // joystick mode: what the analog routine $7041 found (not rebuilt)
};

struct PollResult {
    bool paused = false;                // Esc was pressed and no key has resumed the game yet: nothing else may happen this tick
    bool soundToggled = false;          // Ctrl-S was read
    bool leaveToPlayersPrompt = false;  // Space in the demo (the original jumps to $4144)
    bool directionSet = false;          // $63 was written
};

class InputPoller {
public:
    void pushKey(std::uint8_t code);            // a key arrives (code = 7-bit ASCII with bit 7 set)
    PollResult poll(GameState& state, const KeyConfig& keys, const PortInput& port);
    bool soundOn() const noexcept { return soundOn_; }
    bool paused() const noexcept { return paused_; }
    std::size_t queued() const noexcept { return queue_.size(); }

private:
    bool latch(std::uint8_t& code);
    std::deque<std::uint8_t> queue_;
    bool soundOn_ = true;
    bool paused_ = false;
};

}  // namespace pacman::core
