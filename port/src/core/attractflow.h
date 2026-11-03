// TASK-033-4e: the loop of the original's top level after the boot menus: the attract turns one after the other, the keyboard checks that lead out of them (Space), the text-mode
// prompts "HOW MANY PLAYERS? (1 - 5)" and "PRESS <RETURN> TO PLAY / OR <SPACE> TO GO BACK TO DEMO." ($4144-$4272), and the way back to the demo or on to a game ($52FC).
//
// What is exact (V, against the oracle, tests/test_flow.cpp): at which cycle of the turn a pressed Space is taken (the original looks at the keyboard only at certain places: after
// every line of the logo and in the key poll $6F5E), the text on the two prompt screens, and the cycles the game's own instructions spend between the places (the clear of the text
// page, the echo of the digit, the jump back to the title). What is not exact (I): the time the monitor ROM needs for the text output and the key wait (the oracle's stand-ins for
// it take no cycles, and the ROM version in the "Apple II roms" folder is not the one of the original machine).
#pragma once

#include <cstdint>

#include "attract.h"
#include "textscreen.h"

namespace pacman::core {

// Apple II key codes (7-bit ASCII with bit 7 set), as the original compares them
inline constexpr std::uint8_t kKeySpace = 0xA0, kKeyReturn = 0x8D, kKeyEsc = 0x9B, kKeyCtrlS = 0x93;

// The game-start fields of the original that the prompts fill in.
struct GameStart {
    int players = 1;   // $51 (1-5)
};

class AttractFlow {
public:
    enum class Mode : std::uint8_t {
        Attract,        // an attract turn is running (the title, the chase, the demo game)
        Players,        // text screen: "HOW MANY PLAYERS?"
        PressReturn,    // text screen: "PRESS <RETURN> TO PLAY"
        Game,           // Return was pressed: a real game starts (the caller starts it with players())
    };

    bool start(const datapack::Pack& pack);                 // the first turn, at its cycle 0 ($52BB)
    // A key goes into the keyboard latch (one key; a second key replaces it, as the Apple II keyboard does). `code` is an Apple II key code (kKey...), digits 0xB0-0xB9.
    void keyDown(std::uint8_t code) noexcept { latch_ = code; haveKey_ = true; }
    void advance(std::int64_t cycles);
    // After GAME OVER (the caller waits the 4,868,255 cycles of deathtiming.h kGameOverToAttractStart first): the original runs `$52BB` = a new turn from its cycle 0.
    void returnFromGame() { mode_ = Mode::Attract; paused_ = false; newTurn(); }

    Mode mode() const noexcept { return mode_; }
    int players() const noexcept { return players_; }
    bool soundOn() const noexcept { return soundOn_; }       // Ctrl-S in the key poll toggles it ($6F7F: it patches the speaker instructions)
    bool paused() const noexcept { return paused_; }          // Esc in the key poll: waits for the next key
    AttractTurn& turn() noexcept { return turn_; }
    const AttractTurn& turn() const noexcept { return turn_; }
    std::size_t turnIndex() const noexcept { return turnIndex_; }   // 0 = the first turn after the boot menus
    bool prompting() const noexcept { return mode_ == Mode::Players || mode_ == Mode::PressReturn; }
    bool textReady() const noexcept { return prompting() && wait_ <= 0; }   // the prompt is complete and waits for a key; only then is the text screen shown (until then the last picture stays)
    const TextScreen& text() const noexcept { return text_; }
    // The cycle (of the turn that was running) at which the original took the Space (the `JMP $4144`'s address `$6FAE`), and the cycles of the prompts' key waits from that turn's start
    std::int64_t spaceTakenAt() const noexcept { return spaceAt_; }
    std::size_t spaceTakenInTurn() const noexcept { return spaceTurn_; }
    std::int64_t playersWaitAt() const noexcept { return waitAt_[0]; }
    std::int64_t pressWaitAt() const noexcept { return waitAt_[1]; }
    std::int64_t endAt() const noexcept { return endAt_; }   // the cycle of `$43D2` (Space) or `$52FC` (Return), same base

    // Cycles of the game's own instructions between the places (V, from the oracle)
    static constexpr std::int64_t kTakenToPlayersWait = 11'288;   // $6FAE -> the first RDKEY ($4233): text on, clear of the page, "HOW MANY PLAYERS?"
    static constexpr std::int64_t kDigitToPressWait = 1'830;      // the digit's echo and the two lines of the second prompt -> $4261
    static constexpr std::int64_t kSpaceToTurn = 30;              // $4261 key -> $43D2 (19) -> $52BB (11)
    static constexpr std::int64_t kReturnToGame = 24;             // $4261 key -> $52FC

private:
    void takeKeyAtCheck(const KeyCheck& c);
    void enterPlayers(std::int64_t at);
    void processPromptKey();
    void newTurn();
    const datapack::Pack* pack_ = nullptr;
    AttractTurn turn_;
    TextScreen text_;
    Mode mode_ = Mode::Attract;
    std::uint8_t latch_ = 0;
    bool haveKey_ = false;
    bool soundOn_ = true;
    bool paused_ = false;
    int players_ = 1;
    std::int64_t wait_ = 0;       // cycles still to pass before the current prompt is complete
    std::int64_t lastCheck_ = -1;
    std::size_t turnIndex_ = 0;
    std::int64_t spaceAt_ = -1, endAt_ = -1;
    std::size_t spaceTurn_ = 0;
    std::int64_t waitAt_[2] = {-1, -1};
};

}  // namespace pacman::core
