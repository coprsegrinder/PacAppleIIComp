// The game window (TASK-028k, design in docs/WINDOW_DESIGN.md): shows the newest hi-res page at an integer scale (GDI), switches to full screen,
// changes scale at run time, turns Windows key events into Apple II keyboard codes, and handles the port's own hotkeys.
// Threading: setPage() and popKey() may be called from any thread (the emulation thread); everything else belongs to the thread that created the window.
#pragma once

#include <windows.h>

#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>

#include "../hires/hires.h"

namespace pacman::app {

struct GameWindowOptions {
    int scale = 3;         // whole-number scale 1..8 for the windowed mode
    bool visible = true;   // false: create hidden (used by tests)
};

class GameWindow {
public:
    explicit GameWindow(const GameWindowOptions& options);
    ~GameWindow();
    GameWindow(const GameWindow&) = delete;
    GameWindow& operator=(const GameWindow&) = delete;

    bool ok() const noexcept { return hwnd_ != nullptr; }
    HWND hwnd() const noexcept { return hwnd_; }

    // --- any thread ---
    void setPage(const hires::Page& page);            // copies the 8 KB page under a lock; the window repaints at the next refresh tick
    std::optional<std::uint8_t> popKey();             // next Apple II key code (7-bit ASCII with bit 7 set) or nothing

    // --- window thread ---
    int scale() const noexcept { return scale_; }
    bool fullscreen() const noexcept { return fullscreen_; }
    bool muted() const noexcept { return muted_; }
    int effectiveScale() const noexcept;               // the scale actually drawn (largest whole number that fits when in full screen)
    bool setScale(int scale);                          // 1..8; applies at once in windowed mode, remembered in full screen
    void setFullscreen(bool on);
    // Port hotkeys; returns true if the key was one of them (and so must not reach the game). ctrl/alt: modifier state.
    bool handleHotkey(unsigned vk, bool ctrl, bool alt);
    int run();                                         // message loop until the window closes; returns the exit code
    void close();
    unsigned refreshHz() const noexcept { return refreshHz_; }

private:
    static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void pushKey(std::uint8_t code);
    void applyWindowedSize();
    void paint();

    HWND hwnd_ = nullptr;
    int scale_ = 3;
    bool fullscreen_ = false;
    bool muted_ = false;
    unsigned refreshHz_ = 60;
    WINDOWPLACEMENT saved_{};
    LONG savedStyle_ = 0;

    std::mutex mutex_;                  // guards page_, pageVersion_, keys_
    hires::Page page_;
    unsigned long pageVersion_ = 1;
    std::deque<std::uint8_t> keys_;
    unsigned long paintedVersion_ = 0;  // window thread only
};

}  // namespace pacman::app
