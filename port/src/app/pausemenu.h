// TASK-034-3 (modernization): the pause menu. A small state machine (pages, selection, keyboard and mouse) that draws itself in the Apple II character ROM font on the 280 x 192 picture,
// so it looks like part of the machine and works at every window size and in every display mode. It knows nothing about windows or the game: the window feeds it keys and picture
// coordinates and acts on the Action it returns; the game threads stop while the menu is open (pausegate.h). The graphics page grows with the sections of the modernization (TASK-034-4:
// display mode).
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "../core/textscreen.h"
#include "settings.h"

namespace pacman::app {

class PauseMenu {
public:
    enum class Page { Closed, Main, Graphics, Crt, Controls, Achievements, Credits, ResetConfirm, Confirm };
    enum class Key { Up, Down, Left, Right, Select, Back };
    enum class Action { None, Resume, Quit, ToggleBorders, NextDisplayMode, PreviousDisplayMode, NextResolution, PreviousResolution, NextBackground, PreviousBackground, NextColourMode, PreviousColourMode, NextVibration, PreviousVibration, NextCrtPreset, PreviousCrtPreset, CrtOptionStep, ResetAchievements };
    struct Point { int x, y; };
    // What the pages show (the program's present state).
    struct View {
        DisplayMode displayMode = DisplayMode::Borderless;
        bool borders = true;
        Background background = Background::Black;
        ColourMode colourMode = ColourMode::Original;
        VibrationLevel vibration = VibrationLevel::Medium;
        CrtFilter crt;
        FpsMode fps = FpsMode::Native;
        std::uint32_t achievements = 0;   // the unlocked achievements (bit i = core::kAchievementDefs[i]) for the achievements page
        std::string resolution = "1920X1080 (16:9)";   // the Resolution entry's value: a listed size, CUSTOM WxH, or the desktop WxH in Borderless Fullscreen
    };

    // The panel (picture pixels). Everything of the menu is inside it.
    static constexpr int kPanelX = 10, kPanelY = 20, kPanelW = 260, kPanelH = 152;

    bool open() const noexcept { return page_ != Page::Closed; }
    Page page() const noexcept { return page_; }
    int selected() const noexcept { return selected_; }
    int itemCount() const noexcept;
    CrtOption crtOption() const noexcept { return crtOption_; }   // after Action::CrtOptionStep: which option, and which way (+1 / -1)
    int crtDir() const noexcept { return crtDir_; }

    // The controls page: devices 0 = keyboard, 1 = Xbox (XInput) controller, 2 = PlayStation controller (DualShock 4 / DualSense). The window tells which one is in use (the last input came from it);
    // the page opens on it and follows it when the player picks up another device. Left / Right browse the others.
    static constexpr int kControlDevices = 3;
    void setInUseDevice(int device);
    int controlsDevice() const noexcept { return controlsDevice_; }
    int inUseDevice() const noexcept { return inUseDevice_; }
    // the rows of the controls page for a device: the action and its keys or buttons (symbols are the characters 1..11, drawn by the menu: see kGlyph)
    static std::vector<std::pair<std::string, std::string>> controlRows(int device);
    static const char* controlDeviceName(int device) noexcept;

    // The achievements page: a list of the 22 achievements (the first visible rows scroll with the selection) followed by Credits, Reset Progress and Back.
    static constexpr int kListRows = 8;
    static constexpr int kAchievementEntries = 22 + 3;
    int scrollTop() const noexcept { return scroll_; }
    // the toast that announces an unlocked achievement: a box at the top of the picture (picture pixels, 1 = lit) in the text font
    static constexpr int kToastX = 20, kToastY = 4, kToastW = 240, kToastH = 24;
    static void renderToast(const core::CharacterRom& rom, int achievement, std::vector<std::uint8_t>& pixels);
    // the description of an achievement as the page shows it: uppercase, in lines of at most `width` characters
    static std::vector<std::string> wrapUpper(const std::string& text, std::size_t width);

    void show();                                   // opens the main page with "Resume" selected
    void hide();
    Action key(Key k);                             // keyboard
    Action pointerMove(int px, int py);            // mouse over the picture (picture coordinates 0..279 / 0..191, anything else = outside): selects the item under it
    Action pointerClick(int px, int py);           // left click: selects and activates the item under it; a click elsewhere does nothing
    int itemAt(int px, int py) const noexcept;     // index of the item at this picture position, or -1
    Point itemCenter(int index) const noexcept;    // where the item is on the picture (for the mouse tests)

    // 280 x 192 bytes, 1 = a lit pixel of the menu (text and frame).
    void render(const core::CharacterRom& rom, const View& view, std::vector<std::uint8_t>& pixels) const;

private:
    Action activate();
    int rowStep() const noexcept;                  // text rows between two entries (2 on the short pages, 1 on the graphics page, which will hold many)
    int firstRow() const noexcept;
    int rowOfItem(int index) const noexcept;       // the text row of an entry (the controls page has its two entries apart; -1 = an entry of the achievements page that is scrolled out)
    void keepSelectionVisible() noexcept;
    int scroll_ = 0;
    Page page_ = Page::Closed;
    int selected_ = 0;
    int inUseDevice_ = 0, controlsDevice_ = 0;
    CrtOption crtOption_ = CrtOption::Scanlines;
    int crtDir_ = 1;
    Action crtStep(int dir);                       // the CRT page entry selected, changed one degree
};

}  // namespace pacman::app
