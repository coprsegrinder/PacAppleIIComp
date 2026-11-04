#include "pausemenu.h"

#include <cstring>
#include <string>

namespace pacman::app {

namespace {

constexpr int kItemCol = 8, kItemWidth = 24;   // text columns of the item band (centred in the 40-column text page)
constexpr int kTitleRow = 7;
constexpr int kFirstItemRow = 11;              // items are two text rows apart

int rowOfItem(int index) { return kFirstItemRow + 2 * index; }

}  // namespace

int PauseMenu::itemCount() const noexcept {
    switch (page_) {
    case Page::Main: return 3;
    case Page::Graphics: return 2;
    case Page::Confirm: return 2;
    case Page::Closed: break;
    }
    return 0;
}

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
        if (selected_ == 0) return Action::ToggleBorders;
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

int PauseMenu::itemAt(int px, int py) const noexcept {
    if (!open() || px < kItemCol * 7 || px >= (kItemCol + kItemWidth) * 7) return -1;
    for (int i = 0; i < itemCount(); ++i) {
        const int top = rowOfItem(i) * 8 - 4;   // the row of text and half of the gap above and below it
        if (py >= top && py < top + 16) return i;
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

void PauseMenu::render(const core::CharacterRom& rom, bool borders, std::vector<std::uint8_t>& pixels) const {
    pixels.assign(static_cast<std::size_t>(core::kTextWidth) * core::kTextHeight, 0);
    if (!open()) return;
    core::TextScreen screen;
    screen.clear(0xA0);
    const auto put = [&](int col, int row, const char* s, bool inverse) {
        screen.setCursor(col, row);
        for (; *s; ++s) screen.put(inverse ? static_cast<std::uint8_t>(*s & 0x3F) : static_cast<std::uint8_t>(*s | 0x80));
    };
    const auto centred = [&](int row, const char* s, bool inverse) {
        const int len = static_cast<int>(std::strlen(s));
        if (inverse) put(kItemCol, row, std::string(static_cast<std::size_t>(kItemWidth), ' ').c_str(), true);   // the whole band
        put(kItemCol + (kItemWidth - len) / 2, row, s, inverse);
    };
    const char* title = "PAUSED";
    const char* items[3] = {nullptr, nullptr, nullptr};
    const char* note = nullptr;
    std::string border;
    switch (page_) {
    case Page::Main:
        items[0] = "RESUME";
        items[1] = "GRAPHICS SETTINGS";
        items[2] = "QUIT GAME";
        break;
    case Page::Graphics:
        title = "GRAPHICS SETTINGS";
        border = borders ? "WINDOW BORDERS: ON" : "WINDOW BORDERS: OFF";
        items[0] = border.c_str();
        items[1] = "BACK";
        break;
    case Page::Confirm:
        title = "QUIT GAME?";
        note = "THE GAME WILL CLOSE";
        items[0] = "CONFIRM";
        items[1] = "CANCEL";
        break;
    case Page::Closed: break;
    }
    centred(kTitleRow, title, false);
    if (note) centred(kTitleRow + 2, note, false);
    for (int i = 0; i < itemCount(); ++i) centred(rowOfItem(i), items[i], i == selected_);
    centred(17, "UP DOWN ENTER  ESC BACK", false);
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
