#include "pausemenu.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

#include "../core/achievements.h"

namespace pacman::app {

namespace {

constexpr int kItemCol = 2, kItemWidth = 36;   // text columns of an entry's band (centred in the 40-column text page)
constexpr int kTitleRow = 4;
constexpr int kNoteRow = 6;                    // the line under the title of the confirmation page
constexpr int kBordersNoteRow = 17;            // under the entries of the graphics page
constexpr int kHintRow = 19;


// The symbols of the controls page (characters 1..11 in the page's texts), 7 x 8 dots each, the leftmost dot is bit 6.
constexpr int kGlyphCount = 14;
constexpr std::uint8_t kGlyph[kGlyphCount][8] = {
    {0b1000001, 0b0100010, 0b0010100, 0b0001000, 0b0010100, 0b0100010, 0b1000001, 0},   // 1  cross (PlayStation)
    {0b0011100, 0b0100010, 0b1000001, 0b1000001, 0b1000001, 0b0100010, 0b0011100, 0},   // 2  circle
    {0, 0b0111110, 0b0100010, 0b0100010, 0b0100010, 0b0100010, 0b0111110, 0},           // 3  square
    {0b0001000, 0b0010100, 0b0010100, 0b0100010, 0b0100010, 0b1111111, 0, 0},           // 4  triangle
    {0b0001000, 0b0011100, 0b0101010, 0b0001000, 0b0001000, 0b0001000, 0, 0},           // 5  arrow up
    {0, 0, 0b0001000, 0b0001000, 0b0001000, 0b0101010, 0b0011100, 0b0001000},           // 6  arrow down
    {0, 0b0001000, 0b0011000, 0b0111111, 0b0011000, 0b0001000, 0, 0},                   // 7  arrow left
    {0, 0b0001000, 0b0001100, 0b0111111, 0b0001100, 0b0001000, 0, 0},                   // 8  arrow right
    {0b0011100, 0b0010100, 0b1110111, 0b1000001, 0b1110111, 0b0010100, 0b0011100, 0},   // 9  D-pad
    {0b0011100, 0b0100010, 0b1000001, 0b1001001, 0b1000001, 0b0100010, 0b0011100, 0},   // 10 stick
    {0, 0b1111111, 0, 0b1111111, 0, 0b1111111, 0, 0},                                    // 11 menu button (three bars)
    {0b1111111, 0b1011101, 0b0111110, 0b0011100, 0b0001000, 0b0001000, 0b0011100, 0b0111110},   // 12 trophy
    {0, 0b0000001, 0b0000010, 0b1000100, 0b0101000, 0b0010000, 0, 0},                   // 13 check mark
    {0b0011100, 0b0100010, 0b0100010, 0b1111111, 0b1111111, 0b1110111, 0b1111111, 0},   // 14 padlock
};

}  // namespace


const char* PauseMenu::controlDeviceName(int device) noexcept {
    switch (device) {
    case 0: return "KEYBOARD";
    case 1: return "XBOX CONTROLLER";
    default: return "DUALSHOCK 4 / DUALSENSE";
    }
}

// (the symbols: \x01 cross, \x02 circle, \x03 square, \x04 triangle, \x05 up, \x06 down, \x07 left, \x08 right arrows, \x09 D-pad, \x0A stick, \x0B menu button)
std::vector<std::pair<std::string, std::string>> PauseMenu::controlRows(int device) {
    if (device == 0)
        return {{"MOVE", "\x05\x06\x07\x08  OR  A Z"}, {"START GAME", "SPACE"}, {"PAUSE MENU", "ESC"}, {"MENU: SELECT", "ENTER"}, {"MENU: BACK", "ESC"}, {"SOUND ON / OFF", "CTRL S"}, {"FULL SCREEN", "F11"}};
    if (device == 1)
        return {{"MOVE", "\x09  OR  \x0A"}, {"START GAME", "A"}, {"PAUSE MENU", "\x0B  MENU"}, {"MENU: SELECT", "A"}, {"MENU: BACK", "B"}};
    return {{"MOVE", "\x09  OR  \x0A"}, {"START GAME", "\x01"}, {"PAUSE MENU", "\x0B  OPTIONS"}, {"MENU: SELECT", "\x01"}, {"MENU: BACK", "\x02"}};
}

void PauseMenu::setInUseDevice(int device) {
    device = (std::max)(0, (std::min)(kControlDevices - 1, device));
    if (device == inUseDevice_) return;
    inUseDevice_ = device;
    if (page_ == Page::Controls) controlsDevice_ = device;   // the page follows the device the player picks up
}

void PauseMenu::keepSelectionVisible() noexcept {
    if (page_ != Page::Achievements) return;
    if (selected_ < scroll_) scroll_ = selected_;
    if (selected_ >= scroll_ + kListRows) scroll_ = selected_ - kListRows + 1;
    scroll_ = (std::max)(0, (std::min)(scroll_, kAchievementEntries - kListRows));
}

std::vector<std::string> PauseMenu::wrapUpper(const std::string& text, std::size_t width) {
    std::vector<std::string> lines;
    std::string line, word;
    const auto flushWord = [&] {
        if (word.empty()) return;
        if (!line.empty() && line.size() + 1 + word.size() > width) {
            lines.push_back(line);
            line.clear();
        }
        if (!line.empty()) line += ' ';
        line += word;
        word.clear();
    };
    for (char c : text) {
        if (c == ' ') flushWord();
        else word += static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

void PauseMenu::renderToast(const core::CharacterRom& rom, int achievement, std::vector<std::uint8_t>& pixels) {
    pixels.assign(static_cast<std::size_t>(core::kTextWidth) * core::kTextHeight, 0);
    if (achievement < 0 || achievement >= core::kAchievementCount) return;
    const core::AchievementDef& d = core::kAchievementDefs[static_cast<std::size_t>(achievement)];
    core::TextScreen screen;
    screen.clear(0xA0);
    const auto upper = [](std::string s) {
        for (char& c : s)
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
        return s;
    };
    const auto put = [&](int col, int row, const std::string& s) {
        screen.setCursor(col, row);
        for (char c : s) screen.put(static_cast<std::uint8_t>(c | 0x80));
    };
    const std::string head = "ACHIEVEMENT UNLOCKED";
    std::string name = upper(d.title);
    const std::string pts = "  " + std::to_string(d.points) + " PTS";
    if (name.size() + pts.size() > 30) name.resize(30 - pts.size());
    const int headCol = 3 + (32 - static_cast<int>(head.size())) / 2 + 2;
    put(headCol, 1, head);
    put(3 + (32 - static_cast<int>(name.size() + pts.size())) / 2 + 2, 2, name + pts);
    std::vector<std::uint8_t> text;
    core::renderText(screen, rom, false, -1, -1, text);
    for (std::size_t i = 0; i < pixels.size() && i < text.size(); ++i) pixels[i] = text[i] ? 1 : 0;
    const auto glyph = [&](int col, int row, int code) {   // the trophy at both ends of the first line
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 7; ++c)
                if ((kGlyph[code - 1][r] >> (6 - c)) & 1) pixels[static_cast<std::size_t>(row * 8 + r) * core::kTextWidth + static_cast<std::size_t>(col * 7 + c)] = 1;
    };
    glyph(headCol - 2, 1, 12);
    glyph(headCol + static_cast<int>(head.size()) + 1, 1, 12);
    for (int x = kToastX; x < kToastX + kToastW; ++x) {   // the frame
        pixels[static_cast<std::size_t>(kToastY) * core::kTextWidth + static_cast<std::size_t>(x)] = 1;
        pixels[static_cast<std::size_t>(kToastY + kToastH - 1) * core::kTextWidth + static_cast<std::size_t>(x)] = 1;
    }
    for (int y = kToastY; y < kToastY + kToastH; ++y) {
        pixels[static_cast<std::size_t>(y) * core::kTextWidth + static_cast<std::size_t>(kToastX)] = 1;
        pixels[static_cast<std::size_t>(y) * core::kTextWidth + static_cast<std::size_t>(kToastX + kToastW - 1)] = 1;
    }
}

int PauseMenu::itemCount() const noexcept {
    switch (page_) {
    case Page::Main: return 5;       // Resume, Graphics Settings, Controls, Achievements, Quit Game
    case Page::Achievements: return kAchievementEntries;   // the 22 achievements, Credits, Reset Progress, Back
    case Page::Credits: return 1;    // Back
    case Page::ResetConfirm: return 2;   // Confirm, Cancel
    case Page::Controls: return 2;   // Device, Back
    case Page::Graphics: return 7;   // Display Mode, Resolution, Window Borders, Background, Colour Mode, CRT Filter, Back
    case Page::Crt: return 11;       // Preset, the nine options, Back
    case Page::Confirm: return 2;    // Confirm, Cancel
    case Page::Closed: break;
    }
    return 0;
}

int PauseMenu::rowStep() const noexcept { return (page_ == Page::Graphics || page_ == Page::Crt || page_ == Page::Controls || page_ == Page::Achievements || page_ == Page::Credits) ? 1 : 2; }
int PauseMenu::firstRow() const noexcept { return page_ == Page::Graphics ? 8 : page_ == Page::Crt ? 6 : 9; }

PauseMenu::Action PauseMenu::crtStep(int dir) {
    if (selected_ == 0) return dir > 0 ? Action::NextCrtPreset : Action::PreviousCrtPreset;
    if (selected_ >= 1 && selected_ <= kCrtOptionCount) {
        crtOption_ = static_cast<CrtOption>(selected_ - 1);
        crtDir_ = dir;
        return Action::CrtOptionStep;
    }
    return Action::None;
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
        } else if (selected_ == 2) {
            page_ = Page::Controls;
            selected_ = 0;
            controlsDevice_ = inUseDevice_;
        } else if (selected_ == 3) {
            page_ = Page::Achievements;
            selected_ = 0;
            scroll_ = 0;
        } else {
            page_ = Page::Confirm;
            selected_ = 1;   // the safe answer is selected first: Enter twice does not quit by accident
        }
        return Action::None;
    case Page::Graphics:
        if (selected_ == 0) return Action::NextDisplayMode;
        if (selected_ == 1) return Action::NextResolution;
        if (selected_ == 2) return Action::ToggleBorders;
        if (selected_ == 3) return Action::NextBackground;
        if (selected_ == 4) return Action::NextColourMode;
        if (selected_ == 5) {
            page_ = Page::Crt;
            selected_ = 0;
            return Action::None;
        }
        page_ = Page::Main;
        selected_ = 1;
        return Action::None;
    case Page::Crt:
        if (selected_ == kCrtOptionCount + 1) {
            page_ = Page::Graphics;
            selected_ = 5;
            return Action::None;
        }
        return crtStep(1);
    case Page::Controls:
        if (selected_ == 0) {
            controlsDevice_ = (controlsDevice_ + 1) % kControlDevices;
            return Action::None;
        }
        page_ = Page::Main;
        selected_ = 2;
        return Action::None;
    case Page::Achievements:
        if (selected_ == kAchievementEntries - 3) {   // Credits
            page_ = Page::Credits;
            selected_ = 0;
        } else if (selected_ == kAchievementEntries - 2) {   // Reset Progress: asks first, the safe answer is selected
            page_ = Page::ResetConfirm;
            selected_ = 1;
        } else if (selected_ == kAchievementEntries - 1) {   // Back
            page_ = Page::Main;
            selected_ = 3;
        }
        return Action::None;
    case Page::Credits:
        page_ = Page::Achievements;
        selected_ = kAchievementEntries - 3;
        keepSelectionVisible();
        return Action::None;
    case Page::ResetConfirm: {
        const bool reset = selected_ == 0;
        page_ = Page::Achievements;
        selected_ = kAchievementEntries - 2;
        keepSelectionVisible();
        return reset ? Action::ResetAchievements : Action::None;
    }
    case Page::Confirm:
        if (selected_ == 0) return Action::Quit;
        page_ = Page::Main;
        selected_ = 4;
        return Action::None;
    case Page::Closed: break;
    }
    return Action::None;
}

PauseMenu::Action PauseMenu::key(Key k) {
    if (!open()) return Action::None;
    const int n = itemCount();
    switch (k) {
    case Key::Up: selected_ = (selected_ + n - 1) % n; keepSelectionVisible(); return Action::None;
    case Key::Down: selected_ = (selected_ + 1) % n; keepSelectionVisible(); return Action::None;
    case Key::Left:
    case Key::Right:   // on the graphics page the entries are settings: left and right change them
        if (page_ == Page::Graphics && selected_ == 0) return k == Key::Left ? Action::PreviousDisplayMode : Action::NextDisplayMode;
        if (page_ == Page::Graphics && selected_ == 1) return k == Key::Left ? Action::PreviousResolution : Action::NextResolution;
        if (page_ == Page::Graphics && selected_ == 2) return Action::ToggleBorders;
        if (page_ == Page::Graphics && selected_ == 3) return k == Key::Left ? Action::PreviousBackground : Action::NextBackground;
        if (page_ == Page::Graphics && selected_ == 4) return k == Key::Left ? Action::PreviousColourMode : Action::NextColourMode;
        if (page_ == Page::Graphics && selected_ == 5) return k == Key::Left ? Action::PreviousCrtPreset : Action::NextCrtPreset;
        if (page_ == Page::Crt) return crtStep(k == Key::Left ? -1 : 1);
        if (page_ == Page::Controls && selected_ == 0) controlsDevice_ = (controlsDevice_ + kControlDevices + (k == Key::Left ? -1 : 1)) % kControlDevices;
        return Action::None;
    case Key::Select: return activate();
    case Key::Back:
        if (page_ == Page::Main) {
            hide();
            return Action::Resume;
        }
        if (page_ == Page::Crt) {   // back to the graphics page, on the entry that led here
            page_ = Page::Graphics;
            selected_ = 5;
            return Action::None;
        }
        if (page_ == Page::Credits || page_ == Page::ResetConfirm) {   // back to the achievements page, on the entry that led here
            selected_ = page_ == Page::Credits ? kAchievementEntries - 3 : kAchievementEntries - 2;
            page_ = Page::Achievements;
            keepSelectionVisible();
            return Action::None;
        }
        selected_ = page_ == Page::Graphics ? 1 : page_ == Page::Controls ? 2 : page_ == Page::Achievements ? 3 : 4;   // back to the main page, on the entry that led here
        page_ = Page::Main;
        return Action::None;
    }
    return Action::None;
}

int PauseMenu::rowOfItem(int index) const noexcept {
    if (page_ == Page::Controls) return index == 0 ? 6 : 17;
    if (page_ == Page::Credits) return 17;
    if (page_ == Page::Achievements) return (index >= scroll_ && index < scroll_ + kListRows) ? 6 + (index - scroll_) : -1;
    return firstRow() + rowStep() * index;
}

PauseMenu::Point PauseMenu::itemCenter(int index) const noexcept {
    return Point{kPanelX + kPanelW / 2, rowOfItem(index) * 8 + 4};
}

int PauseMenu::itemAt(int px, int py) const noexcept {
    if (!open() || px < kItemCol * 7 || px >= (kItemCol + kItemWidth) * 7) return -1;
    const int slack = rowStep() == 2 ? 4 : 0;   // the short pages have a free row between the entries: it belongs half to each
    for (int i = 0; i < itemCount(); ++i) {
        if (rowOfItem(i) < 0) continue;   // scrolled out
        const int top = rowOfItem(i) * 8 - slack;
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
    struct GlyphAt { int col, row, code; bool inverse; };
    std::vector<GlyphAt> glyphs;
    const auto put = [&](int col, int row, const std::string& s, bool inverse) {
        screen.setCursor(col, row);
        int at = col;
        for (char c : s) {
            if (c >= 1 && c <= kGlyphCount) {   // a symbol: a blank in the text, drawn over it after the text is made
                glyphs.push_back({at, row, c, inverse});
                screen.put(static_cast<std::uint8_t>(' ' | 0x80));
            } else {
                screen.put(inverse ? static_cast<std::uint8_t>(c & 0x3F) : static_cast<std::uint8_t>(c | 0x80));
            }
            ++at;
        }
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
        items = {"RESUME", "GRAPHICS SETTINGS", "CONTROLS", "ACHIEVEMENTS", "QUIT GAME"};
        break;
    case Page::Achievements: {
        int n = 0, pts = 0;
        for (int i = 0; i < core::kAchievementCount; ++i)
            if ((view.achievements >> i) & 1) {
                ++n;
                pts += core::kAchievementDefs[static_cast<std::size_t>(i)].points;
            }
        title = "ACHIEVEMENTS  " + std::to_string(n) + "/22  " + std::to_string(pts) + "/190 PTS";
        hint = "UP DOWN  ENTER  ESC BACK";
        break;
    }
    case Page::Credits:
        title = "CREDITS";
        items = {"BACK"};
        hint = "ENTER OR ESC: BACK";
        break;
    case Page::ResetConfirm:
        title = "RESET ACHIEVEMENTS?";
        note = "ALL PROGRESS IS FORGOTTEN";
        items = {"CONFIRM", "CANCEL"};
        break;
    case Page::Controls:
        title = "CONTROLS";
        items = {std::string("DEVICE: < ") + controlDeviceName(controlsDevice_) + " >", "BACK"};
        hint = "LEFT RIGHT  OTHER DEVICES  ESC BACK";
        break;
    case Page::Graphics:
        title = "GRAPHICS SETTINGS";
        items = {std::string("DISPLAY MODE: ") + displayModeLabel(view.displayMode), "RESOLUTION: " + view.resolution, view.borders ? "WINDOW BORDERS: ON" : "WINDOW BORDERS: OFF", std::string("BACKGROUND: ") + backgroundLabel(view.background),
                 std::string("COLOUR MODE: ") + colourModeLabel(view.colourMode),
                 std::string("CRT FILTER: ") + crtPresetLabel(crtPresetOf(view.crt)) + " >", "BACK"};
        hint = "UP DOWN  LEFT RIGHT CHANGE  ESC BACK";
        break;
    case Page::Crt:
        title = "CRT FILTER";
        items.push_back(std::string("PRESET: ") + crtPresetLabel(crtPresetOf(view.crt)));
        for (int i = 0; i < kCrtOptionCount; ++i) {
            const CrtOption o = static_cast<CrtOption>(i);
            items.push_back(std::string(crtOptionLabel(o)) + ": " + crtLevelLabel(o, crtGet(view.crt, o)));
        }
        items.push_back("BACK");
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
    if (page_ == Page::Achievements) {
        for (int i = scroll_; i < (std::min)(itemCount(), scroll_ + kListRows); ++i) {
            const int row = rowOfItem(i);
            const bool sel = i == selected_;
            if (sel) put(kItemCol, row, std::string(static_cast<std::size_t>(kItemWidth), ' '), true);   // the band
            if (i < core::kAchievementCount) {   // a lock or a check mark, the title, the points at the right
                const core::AchievementDef& d = core::kAchievementDefs[static_cast<std::size_t>(i)];
                const bool got = ((view.achievements >> i) & 1) != 0;
                std::string t = wrapUpper(d.title, 99).empty() ? std::string() : wrapUpper(d.title, 99)[0];
                if (t.size() > 30) t.resize(30);   // (room: a symbol and a blank, the title, a blank, the points)
                std::string line(static_cast<std::size_t>(kItemWidth), ' ');
                line[0] = static_cast<char>(got ? 13 : 14);
                for (std::size_t k = 0; k < t.size(); ++k) line[2 + k] = t[k];
                const std::string pts = std::to_string(d.points);
                for (std::size_t k = 0; k < pts.size(); ++k) line[kItemWidth - pts.size() + k] = pts[k];
                put(kItemCol, row, line, sel);
            } else {
                const char* label = i == kAchievementEntries - 3 ? "CREDITS" : i == kAchievementEntries - 2 ? "RESET PROGRESS" : "BACK";
                centred(row, label, sel);
            }
        }
        if (selected_ < core::kAchievementCount) {   // what the selected achievement asks of the player (and whether it is done)
            const core::AchievementDef& d = core::kAchievementDefs[static_cast<std::size_t>(selected_)];
            const auto lines = wrapUpper(d.description, kItemWidth);
            for (std::size_t k = 0; k < lines.size() && k < 3; ++k) put(kItemCol, 15 + static_cast<int>(k), lines[k], false);
        }
        if (scroll_ > 0) put(kItemCol + kItemWidth - 1, 5, "^", false);   // more above / below
        if (scroll_ + kListRows < itemCount()) put(kItemCol + kItemWidth - 1, 14, "V", false);
    } else {
        for (int i = 0; i < itemCount(); ++i) centred(rowOfItem(i), items[static_cast<std::size_t>(i)], i == selected_);
    }
    if (page_ == Page::Credits) {
        const char* lines[] = {"ACHIEVEMENT SET: PAC-MAN (APPLE II)", "RETROACHIEVEMENTS.ORG, GAME 13932", "", "ACHIEVEMENTS BY KISUMEBUCKETS", "LOGIC BY XNAIVX", "", "THEY ARE KEPT IN THIS PROGRAM ONLY:", "NOTHING IS SENT ANYWHERE"};
        int row = 7;
        for (const char* l : lines) {
            if (*l) centred(row, l, false);
            ++row;
        }
    }
    if (page_ == Page::Controls) {
        if (controlsDevice_ == inUseDevice_) centred(7, "IN USE", false);   // the device the last input came from
        int row = 9;
        for (const auto& r : controlRows(controlsDevice_)) {
            put(5, row, r.first, false);
            put(22, row, r.second, false);
            ++row;
        }
    }
    if (page_ == Page::Graphics && selected_ == 2 && view.displayMode == DisplayMode::Borderless) centred(kBordersNoteRow, "BORDERS APPLY TO WINDOWED MODE", false);   // the entry works, but there is no frame to see in Borderless Fullscreen
    centred(kHintRow, hint, false);
    std::vector<std::uint8_t> text;
    core::renderText(screen, rom, false, -1, -1, text);
    for (std::size_t i = 0; i < pixels.size() && i < text.size(); ++i) pixels[i] = text[i] ? 1 : 0;
    for (const GlyphAt& g : glyphs)
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 7; ++c)
                if ((kGlyph[g.code - 1][r] >> (6 - c)) & 1) pixels[static_cast<std::size_t>(g.row * 8 + r) * core::kTextWidth + static_cast<std::size_t>(g.col * 7 + c)] = g.inverse ? 0 : 1;   // (on the band of the selected entry the symbol is dark)
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
