// The game window (TASK-028k, design in docs/WINDOW_DESIGN.md): shows the newest hi-res page at an integer scale (GDI), switches to full screen,
// changes scale at run time, turns Windows key events into Apple II keyboard codes, and handles the port's own hotkeys.
// Threading: setPage() and popKey() may be called from any thread (the emulation thread); everything else belongs to the thread that created the window.
#pragma once

#include <windows.h>

#include <cstdint>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "../hires/hires.h"
#include "background.h"
#include "d3drenderer.h"
#include "display_layout.h"
#include "pausemenu.h"
#include "settings.h"

namespace pacman::app {

struct GameWindowOptions {
    int scale = 3;         // whole-number scale 1..8 for the windowed mode
    bool visible = true;   // false: create hidden (used by tests)
    bool muted = false;    // start with the sound off (--mute: for tests that run the real program)
    bool menuAllowed = true;   // the pause menu can be opened; the program starts with false and allows it while a game is running
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
    void setImage(std::span<const std::uint32_t> argb);   // 280 x 192 ARGB pixels (the lo-res page, the text page): shown instead of the hi-res page until the next setPage
    std::optional<std::uint8_t> popKey();             // next Apple II key code (7-bit ASCII with bit 7 set) or nothing

    // --- window thread ---
    int scale() const noexcept { return scale_; }
    bool fullscreen() const noexcept { return fullscreen_; }
    bool muted() const noexcept { return muted_; }
    int effectiveScale() const noexcept;               // the scale actually drawn (largest whole number that fits when in full screen)
    bool setScale(int scale);                          // 1..8; applies at once in windowed mode, remembered in full screen
    // Windowed mode (TASK-034-2): the window can be resized freely; the picture keeps its 280:192 shape, whole-number scale, centred, bars around it (display_layout.h).
    // setBorders(false) removes the title bar and frame; the window is still resizable at its edges and can be moved by dragging the picture. The picture does not move or change size.
    // The pause menu (TASK-034-3): Esc opens it (the original Esc pause is replaced), the keyboard and the mouse drive it, the game threads stand still while menuOpen() is true.
    bool menuOpen() const noexcept { return menuOpen_.load(); }   // any thread
    void setMenuAllowed(bool on) noexcept { menuAllowed_ = on; }   // any thread: false = Esc does nothing (boot menus, attract loop, prompts); the program allows the menu from the start of a game to its end
    void setMenuFont(const core::CharacterRom& rom) { font_ = rom; }   // before the window is used: the Apple II character ROM the menu is drawn with
    bool borders() const noexcept { return borders_; }
    void setBorders(bool on);
    // Display mode (TASK-034-4): Windowed, Borderless Fullscreen (a borderless window over the monitor at its own resolution) or exclusive Fullscreen (the swap chain owns the monitor).
    // None of them touches the game or its clock. Exclusive full screen that Windows or the display refuses falls back to borderless full screen (false is returned, notice() says so).
    DisplayMode displayMode() const noexcept { return mode_; }
    bool setDisplayMode(DisplayMode m);
    bool exclusiveActive() const { return d3d_ && d3d_->exclusive(); }
    void setLogFile(const std::wstring& path) { logPath_ = path; }   // appends a line for every display mode change (what was asked, what is, why exclusive was refused)
    const std::string& exclusiveFailure() const { static const std::string none; return d3d_ ? d3d_->failure() : none; }   // why exclusive full screen was refused (tests, log)
    const std::string& notice() const noexcept { return notice_; }   // shown in the graphics page, e.g. why Fullscreen became Borderless
    // Settings: starts with these (display mode, frame, window position and size), and from then on writes every change of them to the store (atomic file); the window's last
    // windowed place is written when the window closes.
    void attachSettings(SettingsStore* store, const Settings& initial);
    const Settings& settings();                         // the settings as they are now
    void setFullscreen(bool on);                       // never touches the game: only the window style, size and the swap chain change
    void setBackground(Background b);                  // what fills the bars around the picture (window thread)
    Background background() const noexcept { return background_; }
    // Port hotkeys; returns true if the key was one of them (and so must not reach the game). ctrl/alt: modifier state.
    bool handleHotkey(unsigned vk, bool ctrl, bool alt);
    int run();                                         // message loop until the window closes; returns the exit code
    void close();
    unsigned refreshHz() const noexcept { return refreshHz_; }
    bool usesDirect3d() const noexcept { return d3d_ != nullptr; }   // false: the GDI fallback draws (no usable Direct3D 11 device, or PACMAN_GDI=1)
    bool captureFrame(std::vector<std::uint32_t>& argb, int& w, int& h);   // tests: draws the current picture with the active renderer and returns the client area (Direct3D only)

private:
    static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void pushKey(std::uint8_t code);
    void applyWindowedSize();
    LONG windowedStyle() const;                        // the style of the windowed mode for the current borders_ setting (without WS_VISIBLE)
    RECT outerRect(int clientX, int clientY, int clientW, int clientH) const;   // the window rectangle that has this client area, in the current windowed style
    LRESULT hitTest(LPARAM lp);                        // borderless windowed mode: resize edges, and the picture moves the window
    void applyFullscreenWindow(bool on);               // the borderless window over the monitor (what Borderless Fullscreen is, and the base of Fullscreen)
    void rememberWindowRect();                         // windowed mode: where the picture area is now (kept for the settings file)
    void saveSettings();
    void fitToMonitor();                               // full screen: cover the monitor the window is on (again after a display change)
    UINT dpi() const;
    void paint();
    void menuAction(PauseMenu::Action a);
    void menuChanged();                                // repaint, and tell the other threads
    bool menuKey(unsigned vk);                         // true if the menu used the key
    bool pictureCoords(LPARAM lp, int& px, int& py) const;   // client position -> picture pixel; false outside the picture
    void currentPixels(std::uint32_t* out);   // the 280 x 192 picture to show (page or image), marks it as painted

    HWND hwnd_ = nullptr;
    int scale_ = 3;
    bool fullscreen_ = false;                 // the window covers its monitor (Borderless Fullscreen and Fullscreen)
    DisplayMode mode_ = DisplayMode::Windowed;
    DisplayMode fullscreenKind_ = DisplayMode::Borderless;   // where F11 / Alt+Enter goes from the window
    DisplayMode wantedMode_ = DisplayMode::Windowed;          // what is written to the settings (what the user chose; see setDisplayMode)
    std::string notice_;
    std::wstring logPath_;
    void logMode(const char* what);
    SettingsStore* store_ = nullptr;
    Settings settings_;
    bool borders_ = true;               // windowed mode only: title bar and frame
    bool scalePending_ = false;         // setScale() was called in full screen: applies when leaving it
    Background background_ = Background::Black;
    PauseMenu menu_;
    core::CharacterRom font_;
    std::atomic<bool> menuOpen_{false};
    std::atomic<bool> menuAllowed_{true};
    bool muted_ = false;
    unsigned refreshHz_ = 60;
    std::unique_ptr<D3dRenderer> d3d_;
    WINDOWPLACEMENT saved_{};
    POINT savedClientOrigin_{};         // windowed client area before full screen (screen coordinates) and its size
    SIZE savedClientSize_{};
    bool savedZoomed_ = false;
    LONG savedStyle_ = 0;

    std::mutex mutex_;                  // guards page_, pageVersion_, keys_
    hires::Page page_;
    std::vector<std::uint32_t> image_;
    bool useImage_ = false;
    unsigned long pageVersion_ = 1;
    std::deque<std::uint8_t> keys_;
    unsigned long paintedVersion_ = 0;  // window thread only
};

}  // namespace pacman::app
