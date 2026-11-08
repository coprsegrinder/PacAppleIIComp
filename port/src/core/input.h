// The input poll of the original (TASK-029c): routine $6F5E and its two stages, rebuilt from the disassembly and checked against the oracle
// (docs/CONTROLS.md has the key table). The original calls it at the start of EVERY tick in keyboard mode ($7865, before DEC $59) but, in joystick mode, only when the step timer has run out ($7871).
//   stage 1 ($6F77): read the keyboard latch once. Ctrl-S ($93) toggles the sound; Esc ($9B) waits (blocking!) for the next key, which is then treated
//                    as the key of this poll; in the demo only Space ($A0) matters (it leaves to the players prompt); in a game the four direction keys
//                    ($C1 up = 1, $95 right = 2, $88 left = 3, $DA down = 0; the codes can be changed by the "customised keys" prompts) set the direction.
//   stage 2 (keyboard mode, $6FD0): in a game, four switches on the game port can also give a direction (mask 1 = 0 down, 2 = 1 up, 4 = 2 right,
//                    8 = 3 left; every other mask nothing). Joystick mode ($F5 <> 0, mode 2 of the mode screen): $7041 runs instead; it times the two paddles of the game port
//                    ($701E) and decides a direction from the counts (rebuilt below, TASK-034-PADDLE). It ALWAYS writes $63: a direction 0-3, or $80 with the stick in the middle.
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

// The game-port paddles (TASK-034-PADDLE). The hardware is modelled the way the oracle models it (tools/apple2.py): a paddle at position v (0..255) reads "high" at $C064 / $C065 for
// v * 11 cycles after the trigger ($C070). The routines below are the original's, cycle by cycle; only the paddle hardware (this timer) is a model.
inline constexpr int kCyclesPerPaddleUnit = 11;
inline constexpr int kPaddleLoops = 99;           // $701E: A = $63, counted down to 0

struct PaddleCount {
    std::uint8_t x = 0, y = 0;                    // X and Y after $701E: how many of the 99 passes still saw paddle 0 / paddle 1 high
    int cycles = 0;                               // the routine from its first instruction to the end of its RTS (the calling JSR is not counted)
};
PaddleCount paddleRoutine701E(int paddleX, int paddleY) noexcept;   // the paddles' positions 0..255

struct JoystickResult {
    bool carry = false;                           // set: no direction (never happens in the original: every exit has CLC)
    std::uint8_t a = 0x80;                        // 0 = down, 1 = up, 2 = right, 3 = left, $80 = none (the stick is in the middle)
    std::uint8_t x = 0, y = 0;                    // the counts $701E left in X and Y ($7041 compares them)
    int cycles = 0;                               // JSR $7041 .. RTS, including the JSR
};
// $7041: `demo` is $E8, `direction` is $62 (Pac-Man's current direction: it decides which axis is looked at first).
JoystickResult joystickRoutine7041(bool demo, std::uint8_t direction, int paddleX, int paddleY) noexcept;

struct PortInput {
    bool joystickMode = false;                    // $F5 <> 0 (mode 2 of the mode screen)
    std::uint8_t switches = 0;                    // keyboard mode: game-port switch mask (bits 1, 2, 4, 8)
    std::uint8_t paddleX = 127, paddleY = 127;    // joystick mode: the stick's position on the two paddles (0 = left / up, 255 = right / down, 127 = middle)
};

struct PollResult {
    bool paused = false;                // Esc was pressed and no key has resumed the game yet: nothing else may happen this tick
    bool soundToggled = false;          // Ctrl-S was read
    bool leaveToPlayersPrompt = false;  // Space in the demo (the original jumps to $4144)
    bool directionSet = false;          // $63 was written
    int extraCycles = 0;                // joystick mode: the cycles the paddle timing adds to the tick compared with the keyboard poll (about 2,650)
};

class InputPoller {
public:
    void pushKey(std::uint8_t code);            // a key arrives (code = 7-bit ASCII with bit 7 set)
    PollResult poll(GameState& state, const KeyConfig& keys, const PortInput& port);
    bool soundOn() const noexcept { return soundOn_; }
    void setSoundOn(bool on) noexcept { soundOn_ = on; }   // the switch of the program (Ctrl-S) goes with the player from the menus into the game and back
    bool paused() const noexcept { return paused_; }
    std::size_t queued() const noexcept { return queue_.size(); }

private:
    bool latch(std::uint8_t& code);
    std::deque<std::uint8_t> queue_;
    bool soundOn_ = true;
    bool paused_ = false;
};

}  // namespace pacman::core
