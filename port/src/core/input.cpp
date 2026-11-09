#include "input.h"

#include "cycles.h"

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

PaddleCount paddleRoutine701E(int paddleX, int paddleY) noexcept {
    // $701E: LDA $C070 (trigger) / SEC / LDA #$63 / LDX #0 / LDY #0, then 99 passes of: BIT $C064; BPL; INX; BNE (or two NOPs) / the same for $C065 / SBC #1; BNE.
    // Every branch stays inside page $70. The two arms of each test take the same 11 cycles, so a pass is always 27 cycles. The trigger and every sample happen in the 4th cycle of
    // their instruction, so only the distance between the starts of the instructions counts.
    const long highX = static_cast<long>(paddleX) * kCyclesPerPaddleUnit, highY = static_cast<long>(paddleY) * kCyclesPerPaddleUnit;
    long clock = 4 + 2 + 2 + 2 + 2;
    int x = 0, y = 0;
    for (int a = kPaddleLoops; a != 0; --a) {
        const bool hx = clock < highX;
        clock += 4 + (hx ? 2 + 2 + 3 : 3 + 2 + 2);           // BIT abs; BPL not taken, INX, BNE taken / BPL taken, NOP, NOP
        x += hx ? 1 : 0;
        const bool hy = clock < highY;
        clock += 4 + (hy ? 2 + 2 + 3 : 3 + 2 + 2);
        y += hy ? 1 : 0;
        clock += 2 + (a != 1 ? 3 : 2);                        // SBC #1; BNE taken (not on the last pass)
    }
    clock += 6;                                               // RTS
    PaddleCount r;
    r.x = static_cast<std::uint8_t>(x);
    r.y = static_cast<std::uint8_t>(y);
    r.cycles = static_cast<int>(clock);
    return r;
}

JoystickResult joystickRoutine7041(bool demo, std::uint8_t direction, int paddleX, int paddleY) noexcept {
    JoystickResult r;
    int c = 6 + 3;                                            // JSR $7041; LDA $E8
    const auto done = [&](std::uint8_t a) {                   // CLC / LDA #a / RTS
        r.carry = false;
        r.a = a;
        r.cycles = c + 2 + 2 + 6;
        return r;
    };
    if (demo) {                                               // BNE $708D taken
        c += 3;
        return done(0x80);
    }
    c += 2;                                                   // BNE not taken
    const PaddleCount p = paddleRoutine701E(paddleX, paddleY);
    r.x = p.x;
    r.y = p.y;
    c += 6 + p.cycles;                                        // JSR $701E ... RTS
    c += 3 + 2;                                               // LDA $62; LSR A
    const bool carryIn = (direction & 1) != 0;                // the bit LSR shifted out
    // $7053 / $7067: CPX #$1C, BCC left; CPX #$44, BCS right; else LDA #$80, RTS
    const auto horizontal = [&]() {
        c += 2;
        if (p.x < 0x1C) { c += 3; return done(3); }
        c += 2 + 2;
        if (p.x >= 0x44) { c += 3; return done(2); }
        c += 2;
        r.carry = false;                                      // (the carry is still clear from CPX #$44)
        r.a = 0x80;
        r.cycles = c + 2 + 6;
        return r;
    };
    // $706B: CPY #$1C, BCC up; CPY #$44, BCS down; else LDA #$80, RTS
    const auto vertical = [&]() {
        c += 2;
        if (p.y < 0x1C) { c += 3; return done(1); }
        c += 2 + 2;
        if (p.y >= 0x44) { c += 3; return done(0); }
        c += 2;
        r.carry = false;
        r.a = 0x80;
        r.cycles = c + 2 + 6;
        return r;
    };
    if ((direction >> 1) == 0) {                              // $62 = 0 or 1: BNE not taken
        c += 2;
        if (carryIn) {                                        // $705E: BCS taken; CPY #$44; BCS $707D (down); else JMP $7053
            c += 3 + 2;
            if (p.y >= 0x44) { c += 3; return done(0); }
            c += 2 + 3;
            return horizontal();
        }
        c += 2 + 2;                                           // BCS not taken; $704F: CPY #$1C
        if (p.y < 0x1C) { c += 3; return done(1); }
        c += 2;                                               // BCC not taken, falls into $7053
        return horizontal();
    }
    c += 3;                                                   // $62 = 2 or 3: BNE $7065 taken
    if (carryIn) {                                            // BCS $7076 taken; CPX #$44; BCS $7085 (right); else JMP $706B
        c += 3 + 2;
        if (p.x >= 0x44) { c += 3; return done(2); }
        c += 2 + 3;
        return vertical();
    }
    c += 2 + 2;                                               // BCS not taken; $7067: CPX #$1C
    if (p.x < 0x1C) { c += 3; return done(3); }
    c += 2;                                                   // BCC not taken, falls into $706B
    return vertical();
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
        // The cycles beyond the poll without a key (LDA $C000, BPL taken, SEC, RTS: 15): the key goes down the list of $6F7C-$6FC9 (the game's four keys are A, Z and the two arrows; the
        // entries at $6FC4 / $6FC7 run through the BIT instructions that hide the next LDA) and a key that is taken costs STA $63 and a BCS that is not taken (2 more).
        // The attract game looks for the space bar only.
        if (demo) {
            cy(27);
        } else if (code == keys.up) {
            cy(26 + 2);
        } else if (code == keys.right) {
            cy(30 + 4 + 2);
        } else if (code == keys.left) {
            cy(34 + 8 + 2);
        } else if (code == keys.down) {
            cy(49 + 2);
        } else {
            cy(36);
        }
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
    if (port.joystickMode) {                         // $6F6F: JSR $7041; BCS skips the store, and $7041 never sets the carry: $63 is always written
        const JoystickResult j = joystickRoutine7041(demo, state.pacDirection(), port.paddleX, port.paddleY);
        if (!j.carry) second = j.a;
        // the poll costs j.cycles + 17 (LDA $F5, BNE, the call, BCS, STA, RTS) here; the keyboard poll without a switch costs 98 in a game and 38 in the demo (measured on the oracle)
        r.extraCycles = j.cycles + 17 - (demo ? 38 : 98);
    } else if (!demo) {
        switch (port.switches & 15) {  // table at $700E
            case 1: second = static_cast<std::uint8_t>(0); break;
            case 2: second = static_cast<std::uint8_t>(1); break;
            case 4: second = static_cast<std::uint8_t>(2); break;
            case 8: second = static_cast<std::uint8_t>(3); break;
            default: break;
        }
    }
    if (second) {
        state.requestedDirection() = *second;
        r.directionSet = true;
    }
    return r;
}

}  // namespace pacman::core
