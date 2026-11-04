// TASK-034-3 (modernization): the pause menu. A small state machine (pages, selection, keyboard and mouse) that draws itself in the Apple II character ROM font on the 280 x 192 picture,
// so it looks like part of the machine and works at every window size and in every display mode. It knows nothing about windows or the game: the window feeds it keys and picture
// coordinates and acts on the Action it returns; the game threads stop while the menu is open (pausegate.h).
#pragma once

#include <cstdint>
#include <vector>

#include "../core/textscreen.h"

namespace pacman::app {

class PauseMenu {
public:
    enum class Page { Closed, Main, Graphics, Confirm };
    enum class Key { Up, Down, Select, Back };
    enum class Action { None, Resume, Quit, ToggleBorders };

    // The panel (picture pixels). Everything of the menu is inside it.
    static constexpr int kPanelX = 40, kPanelY = 44, kPanelW = 200, kPanelH = 112;

    bool open() const noexcept { return page_ != Page::Closed; }
    Page page() const noexcept { return page_; }
    int selected() const noexcept { return selected_; }
    int itemCount() const noexcept;

    void show();                                   // opens the main page with "Resume" selected
    void hide();
    Action key(Key k);                             // keyboard
    Action pointerMove(int px, int py);            // mouse over the picture (picture coordinates 0..279 / 0..191, anything else = outside): selects the item under it
    Action pointerClick(int px, int py);           // left click: selects and activates the item under it; a click elsewhere does nothing
    int itemAt(int px, int py) const noexcept;     // index of the item at this picture position, or -1

    // 280 x 192 bytes, 1 = a lit pixel of the menu (text and frame). `borders` is the state shown in the graphics page.
    void render(const core::CharacterRom& rom, bool borders, std::vector<std::uint8_t>& pixels) const;

private:
    Action activate();
    Page page_ = Page::Closed;
    int selected_ = 0;
};

}  // namespace pacman::app
