// TASK-034-4 (modernization): the program's settings and the file they are kept in. Plain text, one `key=value` per line (comments start with #); unknown keys are ignored and bad
// values fall back to the defaults, so an old, newer or damaged file can never stop the program from starting. Written to a temporary file and renamed over the old one, so a crash or a
// full disk can never leave a half-written settings file. Later sections add their keys here (resolution, background, colour mode, CRT filter, HDR, FPS, VSync).
#pragma once

#include <string>
#include <utility>

namespace pacman::app {

enum class DisplayMode { Borderless, Windowed };   // Borderless Fullscreen (the default) and Windowed; the exclusive "Fullscreen" of the first design was removed on the user's decision (2026-11-04)

const char* displayModeKey(DisplayMode m) noexcept;     // "borderless" / "windowed" (the file's words; an older file's "fullscreen" is read as borderless)
const char* displayModeLabel(DisplayMode m) noexcept;   // "BORDERLESS FULLSCREEN" / "WINDOWED" (the menu's words)

struct Settings {
    DisplayMode displayMode = DisplayMode::Borderless;      // the default: borderless fullscreen at the desktop's own resolution
    bool windowBorders = true;                              // windowed mode: title bar and frame
    bool hasWindowRect = false;                             // the picture area of the window as the user left it (screen position and size in pixels)
    int windowX = 0, windowY = 0, windowW = 0, windowH = 0;

    bool operator==(const Settings&) const = default;
};

class SettingsStore {
public:
    explicit SettingsStore(std::wstring path) : path_(std::move(path)) {}
    static std::wstring defaultPath();                       // %LOCALAPPDATA%\PacmanAppleIIPort\settings.ini (the environment variable PACMAN_SETTINGS names another file)
    const std::wstring& path() const noexcept { return path_; }
    Settings load(bool* fileExisted = nullptr) const;        // never fails: missing, empty, damaged or unreadable file = the defaults
    bool save(const Settings& s) const;                      // false if it could not be written; the old file is then still there, unchanged
    static std::string format(const Settings& s);            // the file's text
    static Settings parse(const std::string& text);          // the inverse; tolerant

private:
    std::wstring path_;
};

}  // namespace pacman::app
