#include "pausemenu.h"

#include <cstring>
#include <string>

namespace pacman::app {

namespace {

constexpr int kItemCol = 2, kItemWidth = 36;   // text columns of an entry's band (centred in the 40-column text page)
constexpr int kTitleRow = 4;
constexpr int kNoteRow = 6;                    // the line under the title of the confirmation page
constexpr int kHintRow = 19;

}  // namespace

int PauseMenu::itemCount() const noexcept {
    switch (page_) {
    case Page::Main: return 3;       // Resume, Graphics Settings, Quit Game
    case Page::Graphics: return 4;   // Display Mode, Resolution, Window Borders, Back
    case Page::Confirm: return 2;    // Confirm, Cancel
    case Page::Closed: break;
    }
    return 0;
}

int PauseMenu::rowStep() const noexcept { return page_ == Page::Graphics ? 1 : 2; }
int PauseMenu::firstRow() const noexcept { return page_ == Page::Graphics ? 8 : 9; }

void PauseMenu::show() {
    page_ = Page::Main;
    selected_ = 0;
}

void PauseMenu::hide() {
    page_ = Page::Closed;
    selected_ = 0;
}

PauseMenu::Action PauseMenu::activate() {
    switch (page_) {
    case Page::Main:
        if (selected_ == 0) {
            hide();
            return Action::Resume;
        }
        if (selected_ == 1) {
            page_ = Page::Graphics;
            selected_ = 0;
        } else {
            page_ = Page::Confirm;
            selected_ = 1;   // the safe answer is selected first: Enter twice does not quit by accident
        }
        return Action::None;
    case Page::Graphics:
        if (selected_ == 0) return Action::NextDisplayMode;
        if (selected_ == 1) return Action::NextResolution;
        if (selected_ == 2) return Action::ToggleBorders;
        page_ = Page::Main;
        selected_ = 1;
        return Action::None;
    case Page::Confirm:
        if (selected_ == 0) return Action::Quit;
        page_ = Page::Main;
        selected_ = 2;
        return Action::None;
    case Page::Closed: break;
    }
    return Action::None;
}

PauseMenu::Action PauseMenu::key(Key k) {
    if (!open()) return Action::None;
    const int n = itemCount();
    switch (k) {
    case Key::Up: selected_ = (selected_ + n - 1) % n; return Action::None;
    case Key::Down: selected_ = (selected_ + 1) % n; return Action::None;
    case Key::Left:
    case Key::Right:   // on the graphics page the entries are settings: left and right change them
        if (page_ == Page::Graphics && selected_ == 0) return k == Key::Left ? Action::PreviousDisplayMode : Action::NextDisplayMode;
        if (page_ == Page::Graphics && selected_ == 1) return k == Key::Left ? Action::PreviousResolution : Action::NextResolution;
        if (page_ == Page::Graphics && selected_ == 2) return Action::ToggleBorders;
        return Action::None;
    case Key::Select: return activate();
    case Key::Back:
        if (page_ == Page::Main) {
            hide();
            return Action::Resume;
        }
        selected_ = page_ == Page::Graphics ? 1 : 2;   // back to the main page, on the entry that led here
        page_ = Page::Main;
        return Action::None;
    }
    return Action::None;
}

PauseMenu::Point PauseMenu::itemCenter(int index) const noexcept {
    return Point{kPanelX + kPanelW / 2, (firstRow() + rowStep() * index) * 8 + 4};
}

int PauseMenu::itemAt(int px, int py) const noexcept {
    if (!open() || px < kItemCol * 7 || px >= (kItemCol + kItemWidth) * 7) return -1;
    const int slack = rowStep() == 2 ? 4 : 0;   // the short pages have a free row between the entries: it belongs half to each
    for (int i = 0; i < itemCount(); ++i) {
        const int top = (firstRow() + rowStep() * i) * 8 - slack;
        if (py >= top && py < top + 8 + 2 * slack) return i;
    }
    return -1;
}

PauseMenu::Action PauseMenu::pointerMove(int px, int py) {
    const int i = itemAt(px, py);
    if (i >= 0) selected_ = i;
    return Action::None;
}

PauseMenu::Action PauseMenu::pointerClick(int px, int py) {
    const int i = itemAt(px, py);
    if (i < 0) return Action::None;
    selected_ = i;
    return activate();
}

void PauseMenu::render(const core::CharacterRom& rom, const View& view, std::vector<std::uint8_t>& pixels) const {
    pixels.assign(static_cast<std::size_t>(core::kTextWidth) * core::kTextHeight, 0);
    if (!open()) return;
    core::TextScreen screen;
    screen.clear(0xA0);
    const auto put = [&](int col, int row, const std::string& s, bool inverse) {
        screen.setCursor(col, row);
        for (char c : s) screen.put(inverse ? static_cast<std::uint8_t>(c & 0x3F) : static_cast<std::uint8_t>(c | 0x80));
    };
    const auto centred = [&](int row, const std::string& s, bool inverse) {
        const int len = static_cast<int>(s.size());
        if (inverse) put(kItemCol, row, std::string(static_cast<std::size_t>(kItemWidth), ' '), true);   // the whole band
        put(kItemCol + (kItemWidth - len) / 2, row, s, inverse);
    };
    std::string title = "PAUSED", note, hint = "UP DOWN ENTER  ESC BACK";
    std::vector<std::string> items;
    switch (page_) {
    case Page::Main:
        items = {"RESUME", "GRAPHICS SETTINGS", "QUIT GAME"};
        break;
    case Page::Graphics:
        title = "GRAPHICS SETTINGS";
        items = {std::string("DISPLAY MODE: ") + displayModeLabel(view.displayMode), "RESOLUTION: " + view.resolution, view.borders ? "WINDOW BORDERS: ON" : "WINDOW BORDERS: OFF", "BACK"};
        hint = "UP DOWN  LEFT RIGHT CHANGE  ESC BACK";
        break;
    case Page::Confirm:
        title = "QUIT GAME?";
        note = "THE GAME WILL CLOSE";
        items = {"CONFIRM", "CANCEL"};
        break;
    case Page::Closed: break;
    }
    centred(kTitleRow, title, false);
    if (!note.empty()) centred(kNoteRow, note, false);
    for (int i = 0; i < itemCount(); ++i) centred(firstRow() + rowStep() * i, items[static_cast<std::size_t>(i)], i == selected_);
    centred(kHintRow, hint, false);
    std::vector<std::uint8_t> text;
    core::renderText(screen, rom, false, -1, -1, text);
    for (std::size_t i = 0; i < pixels.size() && i < text.size(); ++i) pixels[i] = text[i] ? 1 : 0;
    // the frame of the panel, one pixel wide, just inside its edge
    for (int x = kPanelX; x < kPanelX + kPanelW; ++x) {
        pixels[static_cast<std::size_t>(kPanelY) * core::kTextWidth + x] = 1;
        pixels[static_cast<std::size_t>(kPanelY + kPanelH - 1) * core::kTextWidth + x] = 1;
    }
    for (int y = kPanelY; y < kPanelY + kPanelH; ++y) {
        pixels[static_cast<std::size_t>(y) * core::kTextWidth + kPanelX] = 1;
        pixels[static_cast<std::size_t>(y) * core::kTextWidth + kPanelX + kPanelW - 1] = 1;
    }
}

}  // namespace pacman::app
