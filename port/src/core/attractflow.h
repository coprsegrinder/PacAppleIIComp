// TASK-033-4e / 4e2: the loop of the original's top level after the boot menus: the attract turns one after the other, the keyboard checks that lead out of them (Space), the
// text-mode prompts "HOW MANY PLAYERS? (1 - 5)" and "PRESS <RETURN> TO PLAY / OR <SPACE> TO GO BACK TO DEMO." ($4144-$4272), the hidden menu (Ctrl-V, Ctrl-C: "RANK OR
// CARTOON?", the cartoon number or the rank number, $4147-$41DB), and the way back to the demo or on to a game ($52FC).
//
// What is exact (V, against the oracle, tests/test_flow.cpp): at which cycle of the turn a pressed Space is taken (the original looks at the keyboard only at certain places: after
// every line of the logo and in the key poll $6F5E), the text on the screens, and the cycles the game's own instructions spend between the places (the clear of the text
// page, the echo of the digit, the jump back to the title, the rank arithmetic, the intermission). What is not exact (I): the time the monitor ROM needs for the text output and the
// key wait (the oracle's stand-ins for it take no cycles, and the ROM version in the "Apple II roms" folder is not the one of the original machine).
// The line input of the hidden menu ($FD6F) follows the listing of the monitor ROM (341-0004: $FD35-$FD8E), not the oracle's stand-in, which prints a prompt character that the
// original does not print (the entry $FD6F lies behind the prompt output of GETLN).
#pragma once

#include <array>
#include <cstdint>

#include "attract.h"
#include "textscreen.h"

namespace pacman::core {

// Apple II key codes (7-bit ASCII with bit 7 set), as the original compares them
inline constexpr std::uint8_t kKeySpace = 0xA0, kKeyReturn = 0x8D, kKeyEsc = 0x9B, kKeyCtrlS = 0x93, kKeyCtrlV = 0x96, kKeyCtrlC = 0x83, kKeyCtrlX = 0x98, kKeyBackspace = 0x88;

class AttractFlow {
public:
    enum class Mode : std::uint8_t {
        Attract,        // an attract turn is running (the title, the chase, the demo game)
        Players,        // text screen: "HOW MANY PLAYERS?"
        PressReturn,    // text screen: "PRESS <RETURN> TO PLAY"
        RankOrCartoon,  // hidden menu, text screen: "RANK OR CARTOON?" with a line input
        CartoonNumber,  // hidden menu: "CARTOON NUMBER? (1-3)" with a line input
        RankNumber,     // hidden menu: "RANK NUMBER? (1-21)" with a line input
        Cartoon,        // hidden menu: an intermission plays (cartoonNumber(), cartoonElapsed()); it ends by itself and the players prompt follows
        Game,           // Return was pressed: a real game starts (the caller starts it with players() and startLevel())
    };

    bool start(const datapack::Pack& pack);                 // the first turn, at its cycle 0 ($52BB)
    // A key goes into the keyboard latch (one key; a second key replaces it, as the Apple II keyboard does). `code` is an Apple II key code (kKey...), digits 0xB0-0xB9.
    void keyDown(std::uint8_t code) noexcept { latch_ = code; haveKey_ = true; }
    void advance(std::int64_t cycles);
    // After GAME OVER (the caller waits the 4,868,255 cycles of deathtiming.h kGameOverToAttractStart first): the original runs `$52BB` = a new turn from its cycle 0.
    void returnFromGame() { mode_ = Mode::Attract; paused_ = false; newTurn(); }

    Mode mode() const noexcept { return mode_; }
    int players() const noexcept { return players_; }
    int startLevel() const noexcept { return rank_; }        // `$54E1`: 1 at the start, set by the rank number of the hidden menu (the first level of the next game)
    int cartoonNumber() const noexcept { return cartoon_; }  // 1-3 while mode() == Cartoon
    std::int64_t cartoonElapsed() const noexcept { return cartoonElapsed_; }
    bool soundOn() const noexcept { return soundOn_; }       // Ctrl-S in the key poll toggles it ($6F7F: it patches the speaker instructions)
    bool paused() const noexcept { return paused_; }          // Esc in the key poll: waits for the next key
    AttractTurn& turn() noexcept { return turn_; }
    const AttractTurn& turn() const noexcept { return turn_; }
    std::size_t turnIndex() const noexcept { return turnIndex_; }   // 0 = the first turn after the boot menus
    bool prompting() const noexcept { return mode_ == Mode::Players || mode_ == Mode::PressReturn || isLineMode(); }
    bool textReady() const noexcept { return prompting() && wait_ <= 0; }   // the prompt is complete and waits for a key; only then is the text screen shown (until then the last picture stays)
    bool showsText() const noexcept { return textReady(); }
    const TextScreen& text() const noexcept { return text_; }
    // where the monitor's cursor (a flashing block while RDKEY waits) stands
    int cursorCol() const noexcept { return text_.col(); }
    int cursorRow() const noexcept { return text_.row(); }
    // The cycle (of the turn that was running) at which the original took the Space (the `JMP $4144`'s address `$6FAE`), and the cycles of the prompts' key waits from that turn's start
    std::int64_t spaceTakenAt() const noexcept { return spaceAt_; }
    std::size_t spaceTakenInTurn() const noexcept { return spaceTurn_; }
    std::int64_t playersWaitAt() const noexcept { return waitAt_[0]; }
    std::int64_t pressWaitAt() const noexcept { return waitAt_[1]; }
    std::int64_t endAt() const noexcept { return endAt_; }   // the cycle of `$43D2` (Space) or `$52FC` (Return), same base
    // The game's own cycles since `$6FAE` at which the current key wait of a prompt is reached (counted only while the prompts run; the first wait is kTakenToPlayersWait)
    std::int64_t promptClock() const noexcept { return promptClock_; }
    // The line typed in the hidden menu so far / when it was ended (the original's buffer `$0200`: key codes, upper case, ending with $8D)
    const std::array<std::uint8_t, 256>& lineBuffer() const noexcept { return line_; }

    // Cycles of the game's own instructions between the places (V, from the oracle)
    static constexpr std::int64_t kTakenToPlayersWait = 11'288;   // $6FAE -> the first RDKEY ($4233): text on, clear of the page, "HOW MANY PLAYERS?"
    static constexpr std::int64_t kDigitToPressWait = 1'830;      // the digit's echo and the two lines of the second prompt -> $4261
    static constexpr std::int64_t kSpaceToTurn = 30;              // $4261 key -> $43D2 (19) -> $52BB (11)
    static constexpr std::int64_t kReturnToGame = 24;             // $4261 key -> $52FC
    // the hidden menu (V: tools/capture_hidden.py, 13 runs): each is the difference of the cycles between two key waits (the RDKEY trap of the oracle is included in both)
    static constexpr std::int64_t kCtrlVToSecondKey = 17;         // Ctrl-V at $4233 -> the RDKEY at $4272
    static constexpr std::int64_t kOtherKeyBack = 17;             // another key than Ctrl-C there -> the RDKEY at $4233 again
    static constexpr std::int64_t kCtrlCToLine = 10'929;          // Ctrl-C -> $4147: text on, clear, "RANK OR CARTOON? " -> the line input
    static constexpr std::int64_t kLineToCartoonPrompt = 744;     // the line ends with C...: "CARTOON NUMBER? (1-3) " -> the line input
    static constexpr std::int64_t kLineToRankPrompt = 684;        // any other line: "RANK NUMBER? (1-21) " -> the line input
    static constexpr std::int64_t kCartoonLineToStart = 32;       // the cartoon number's line ends -> `JSR $8155`
    static constexpr std::int64_t kCartoonGolden = 11'326;        // the intermission of the hidden menu lasts kCartoonCycles[n] minus this (the golden figures count from an earlier place)
    static constexpr std::int64_t kCartoonEndToPlayersWait = 11'288;   // `$8155` returns -> `$4144` -> `$4217` -> the key wait (same as $6FAE -> wait)

private:
    bool isLineMode() const noexcept { return mode_ == Mode::RankOrCartoon || mode_ == Mode::CartoonNumber || mode_ == Mode::RankNumber; }
    void takeKeyAtCheck(const KeyCheck& c);
    void enterPlayers(std::int64_t at);
    void showPlayersText();
    void showLinePrompt(int which);
    void processPromptKey();
    void lineKey(std::uint8_t k);
    void lineRestart();
    void lineDone();
    void newTurn();
    const datapack::Pack* pack_ = nullptr;
    AttractTurn turn_;
    TextScreen text_;
    Mode mode_ = Mode::Attract;
    std::uint8_t latch_ = 0;
    bool haveKey_ = false;
    bool soundOn_ = true;
    bool paused_ = false;
    bool ctrlV_ = false;          // the Players prompt got Ctrl-V and waits for Ctrl-C
    int players_ = 1;
    int rank_ = 1;
    int cartoon_ = 0;
    std::int64_t cartoonElapsed_ = 0, cartoonLength_ = 0;
    std::int64_t wait_ = 0;       // cycles still to pass before the current prompt is complete
    std::int64_t lastCheck_ = -1;
    std::int64_t promptClock_ = 0;
    std::size_t turnIndex_ = 0;
    std::int64_t spaceAt_ = -1, endAt_ = -1;
    std::size_t spaceTurn_ = 0;
    std::int64_t waitAt_[2] = {-1, -1};
    // the line input of the hidden menu ($FD6F)
    std::array<std::uint8_t, 256> line_{};
    unsigned lineX_ = 0;          // X: the index of the next character
    bool escPending_ = false;
    std::uint8_t prompt_ = 0xDD;  // $33: the monitor's prompt character, printed when a line is cancelled (I: ']' as the stand-in assumes; not visible otherwise)
};

}  // namespace pacman::core
