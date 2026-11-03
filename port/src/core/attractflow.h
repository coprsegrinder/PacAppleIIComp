// TASK-033-4e / 4e2 / 4e3: the loop of the original's top level after the boot menus: the attract turns one after the other, the keyboard checks that lead out of them (Space), the
// text-mode prompts "HOW MANY PLAYERS? (1 - 5)" and "PRESS <RETURN> TO PLAY / OR <SPACE> TO GO BACK TO DEMO." ($4144-$4272), the hidden menu (Ctrl-V, Ctrl-C: "RANK OR
// CARTOON?", the cartoon number or the rank number, $4147-$41DB), and the way back to the demo or on to a game ($52FC).
//
// The text output and the key waits are done by the monitor ROM of the Apple II Plus, which RUNS in monitor.h (a 6502): the line input, the echo, the cursor, the scrolling and
// every cycle they cost are the ROM's own. The pieces of the game's code between the ROM calls are ported with their cycle counts (monitor.cpp, this file). tests/test_flow.cpp and
// tests/test_hidden.cpp compare the cycle of every key wait, the text page and the monitor's zero page with the oracle running the same ROM (tools/apple2.py, real_rom).
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "attract.h"
#include "monitor.h"
#include "textscreen.h"

namespace pacman::core {

// Apple II key codes (7-bit ASCII with bit 7 set), as the original compares them
inline constexpr std::uint8_t kKeySpace = 0xA0, kKeyReturn = 0x8D, kKeyEsc = 0x9B, kKeyCtrlS = 0x93, kKeyCtrlV = 0x96, kKeyCtrlC = 0x83, kKeyCtrlX = 0x98, kKeyBackspace = 0x88;

class AttractFlow {
public:
    enum class Mode : std::uint8_t {
        BootMode,       // the first menu of the program: "CHOOSE A MODE (1 - 2)" ($48A6)
        BootKeyboard,   // "NORMAL OR CUSTOMIZED KEYBOARD (N/C)?" ($492F)
        BootKeys,       // "PRESS KEY USED TO MOVE LEFT / RIGHT / UP / DOWN" ($4955), keyIndex() = 0-3
        Attract,        // an attract turn is running (the title, the chase, the demo game)
        Players,        // text screen: "HOW MANY PLAYERS?"
        PressReturn,    // text screen: "PRESS <RETURN> TO PLAY"
        RankOrCartoon,  // hidden menu, text screen: "RANK OR CARTOON?" with a line input
        CartoonNumber,  // hidden menu: "CARTOON NUMBER? (1-3)" with a line input
        RankNumber,     // hidden menu: "RANK NUMBER? (1-21)" with a line input
        Cartoon,        // hidden menu: an intermission plays (cartoonNumber(), cartoonElapsed()); it ends by itself and the players prompt follows
        Game,           // Return was pressed: a real game starts (the caller starts it with players() and startLevel())
    };

    // `romPath`: the Autostart Monitor ROM dump (341-0020, 2,048 bytes, $F800-$FFFF)
    bool start(const datapack::Pack& pack, const std::string& romPath);   // the first turn, at its cycle 0 ($52BB), as after the boot menus with "1" and "N" (the state of the random counter, the default keys)
    bool startBoot(const datapack::Pack& pack, const std::string& romPath);   // the program from its first instruction ($4027): the two boot menus, then the first turn
    int inputDevice() const noexcept { return monitor_.peek(0xF5); }          // `$F5`: 0 = keyboard, 1 = joystick (the first menu)
    // the four key codes of the keyboard mode (`$6FBA`, `$6FB6`, `$6FB2`, `$6FBE`): left, right, up, down; the customised keyboard changes them (defaults $88, $95, $C1, $DA)
    std::uint8_t keyLeft() const noexcept { return monitor_.peek(0x6FBA); }
    std::uint8_t keyRight() const noexcept { return monitor_.peek(0x6FB6); }
    std::uint8_t keyUp() const noexcept { return monitor_.peek(0x6FB2); }
    std::uint8_t keyDown_() const noexcept { return monitor_.peek(0x6FBE); }
    int keyIndex() const noexcept { return keyIndex_; }
    std::int64_t bootT0() const noexcept { return bootT0_; }   // the cycle (counted from $4027) at which the first turn started, after the boot menus
    // A key goes into the keyboard latch (one key; a second key replaces it, as the Apple II keyboard does). `code` is an Apple II key code (kKey...), digits 0xB0-0xB9.
    void keyDown(std::uint8_t code) noexcept { latch_ = code; haveKey_ = true; }
    void advance(std::int64_t cycles);
    // After GAME OVER (the caller waits the 4,868,255 cycles of deathtiming.h kGameOverToAttractStart first): the original runs `$52BB` = a new turn from its cycle 0.
    void returnFromGame() { mode_ = Mode::Attract; paused_ = false; newTurn(); }
    // The same with the memory the game left (the last player's bytes, every player's save, the number of players, the high score): the attract game shows and plays it (TASK-033-6c).
    void returnFromGame(const AttractMemory& after) { mem_ = after; players_ = after.players; returnFromGame(); }
    const AttractMemory& memory() const noexcept { return mem_; }

    Mode mode() const noexcept { return mode_; }
    int players() const noexcept { return players_; }
    int startLevel() const noexcept { return monitor_.peek(0x54E1); }   // `$54E1`: 1 at the start, set by the rank number of the hidden menu (the first level of the next game)
    int cartoonNumber() const noexcept { return cartoon_; }  // 1-3 while mode() == Cartoon
    std::int64_t cartoonElapsed() const noexcept { return cartoonElapsed_; }
    bool soundOn() const noexcept { return soundOn_; }
    void setSoundOn(bool on) noexcept { soundOn_ = on; }       // Ctrl-S in the key poll toggles it ($6F7F: it patches the speaker instructions)
    bool paused() const noexcept { return paused_; }          // Esc in the key poll: waits for the next key
    AttractTurn& turn() noexcept { return turn_; }
    const AttractTurn& turn() const noexcept { return turn_; }
    std::size_t turnIndex() const noexcept { return turnIndex_; }   // 0 = the first turn after the boot menus
    bool prompting() const noexcept { return mode_ == Mode::Players || mode_ == Mode::PressReturn || isLineMode() || isBootMode(); }
    bool textReady() const noexcept { return prompting() && busy_ <= 0; }   // the prompt is complete and waits for a key; only then is the text screen shown (until then the last picture stays)
    bool showsText() const noexcept { return textReady(); }
    std::int64_t busyCycles() const noexcept { return busy_ > 0 ? busy_ : 0; }   // cycles until the prompt is complete (the code that makes it has already run)
    const TextScreen& text() const noexcept { return text_; }            // the text page 2 as the monitor left it (with the flashing cursor character at the cursor while a key is awaited)
    int cursorCol() const noexcept { return text_.col(); }
    int cursorRow() const noexcept { return text_.row(); }
    const Monitor& monitor() const noexcept { return monitor_; }
    // The cycle (of the turn that was running) at which the original took the Space (the `JMP $4144`'s address `$6FAE`), and the turn it happened in
    std::int64_t spaceTakenAt() const noexcept { return spaceAt_; }
    std::size_t spaceTakenInTurn() const noexcept { return spaceTurn_; }
    // The cycles of the game's own instructions and the ROM since `$6FAE`: at a key wait of a prompt this is the cycle of the wait (the oracle's cycle minus the cycle of `$6FAE`)
    std::int64_t promptClock() const noexcept { return monitor_.cycles(); }
    // the same at the moment the current key wait was reached (time spent waiting for the key afterwards is not in them), and the monitor's zero page then
    std::int64_t waitClock() const noexcept { return waitClock_; }
    const std::array<std::uint8_t, 256>& waitZeroPage() const noexcept { return waitZp_; }
    std::int64_t endAt() const noexcept { return endAt_; }   // the prompt clock at `$43D2` (Space) or `$52FC` (Return)
    // The line typed in the hidden menu (the original's buffer `$0200`)
    std::array<std::uint8_t, 256> lineBuffer() const;

    // the cycles of `JSR $8155` (the intermission) from its entry to its return, as the oracle measured them in the hidden menu (tools/capture_hidden.py r9, r1, r8): cartoons 1, 2, 3
    static constexpr std::int64_t kHiddenCartoonCycles[3] = {5'585'985, 4'335'890, 8'200'636};

private:
    bool isBootMode() const noexcept { return mode_ == Mode::BootMode || mode_ == Mode::BootKeyboard || mode_ == Mode::BootKeys; }
    bool isLineMode() const noexcept { return mode_ == Mode::RankOrCartoon || mode_ == Mode::CartoonNumber || mode_ == Mode::RankNumber; }
    void takeKeyAtCheck(const KeyCheck& c);
    void enterPlayers(std::int64_t at);
    void playersScreen();                         // $4217: up to the key wait at $4233
    void hiddenScreen();                          // $4147: up to the line input of the first line
    void linePrompt(int which);                   // the text of the second line and its input
    void afterKey(std::uint8_t k);                // the key (or the line) is complete: the game's code up to the next wait
    void afterPlayersKey(std::uint8_t k);
    void bootModeScreen();
    void bootKeyboardScreen();
    void bootKeyPrompt(int index);
    void afterBootKey(std::uint8_t k);
    void finishBoot();
    void afterLine();
    void rankLine();
    void endTransition(std::int64_t startCycles);
    void syncText();
    void newTurn();
    const datapack::Pack* pack_ = nullptr;
    AttractTurn turn_;
    Monitor monitor_;
    TextScreen text_;
    Mode mode_ = Mode::Attract;
    std::uint8_t latch_ = 0;
    bool haveKey_ = false;
    bool soundOn_ = true;
    bool paused_ = false;
    bool ctrlV_ = false;          // the Players prompt got Ctrl-V and waits for Ctrl-C ($4272)
    int players_ = 1;
    AttractMemory mem_;           // what the attract game starts from
    int cartoon_ = 0;
    std::int64_t cartoonElapsed_ = 0, cartoonLength_ = 0;
    int keyIndex_ = 0;
    std::int64_t bootT0_ = -1;
    std::int64_t waitClock_ = 0;
    std::array<std::uint8_t, 256> waitZp_{};
    std::int64_t busy_ = 0;       // cycles still to pass before the current prompt is complete (the code up to its key wait has run already)
    std::int64_t lastCheck_ = -1;
    std::size_t turnIndex_ = 0;
    std::int64_t spaceAt_ = -1, endAt_ = -1;
    std::size_t spaceTurn_ = 0;
};

}  // namespace pacman::core
