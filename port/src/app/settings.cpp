#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "settings.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace pacman::app {

const char* displayModeKey(DisplayMode m) noexcept {
    switch (m) {
    case DisplayMode::Borderless: return "borderless";
    case DisplayMode::Windowed: return "windowed";
    }
    return "borderless";
}

const char* displayModeLabel(DisplayMode m) noexcept {
    switch (m) {
    case DisplayMode::Borderless: return "BORDERLESS FULLSCREEN";
    case DisplayMode::Windowed: return "WINDOWED";
    }
    return "BORDERLESS FULLSCREEN";
}

namespace {

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

bool parseMode(const std::string& v, DisplayMode& out) {
    if (v == "fullscreen") {   // the exclusive full screen mode of an earlier version of the program no longer exists: its users get the borderless one
        out = DisplayMode::Borderless;
        return true;
    }
    for (DisplayMode m : {DisplayMode::Borderless, DisplayMode::Windowed})
        if (v == displayModeKey(m)) {
            out = m;
            return true;
        }
    return false;
}

bool parseInt(const std::string& v, int& out) {
    if (v.empty() || v.size() > 9) return false;
    std::size_t i = v[0] == '-' ? 1 : 0;
    if (i == v.size()) return false;
    for (std::size_t k = i; k < v.size(); ++k)
        if (v[k] < '0' || v[k] > '9') return false;
    out = std::atoi(v.c_str());
    return true;
}

bool parseBool(const std::string& v, bool& out) {
    if (v == "1" || v == "true" || v == "yes" || v == "on") {
        out = true;
        return true;
    }
    if (v == "0" || v == "false" || v == "no" || v == "off") {
        out = false;
        return true;
    }
    return false;
}

void makeDirectories(const std::wstring& file) {
    for (std::size_t i = 3; i < file.size(); ++i)   // from after "C:\"
        if (file[i] == L'\\' || file[i] == L'/') CreateDirectoryW(file.substr(0, i).c_str(), nullptr);   // an existing folder is fine
}

}  // namespace

std::wstring SettingsStore::defaultPath() {
    wchar_t env[MAX_PATH * 2] = {};
    if (GetEnvironmentVariableW(L"PACMAN_SETTINGS", env, MAX_PATH * 2) > 0) return env;
    wchar_t base[MAX_PATH * 2] = {};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH * 2) == 0) return L"pacman_settings.ini";   // no profile folder: next to the program
    return std::wstring(base) + L"\\PacmanAppleIIPort\\settings.ini";
}

std::string SettingsStore::format(const Settings& s) {
    std::ostringstream o;
    o << "# Pac-Man (Apple II port) settings. Written by the program; you may edit it while the program is closed.\n";
    o << "version=1\n";
    o << "display_mode=" << displayModeKey(s.displayMode) << "\n";
    o << "window_borders=" << (s.windowBorders ? 1 : 0) << "\n";
    o << "background=" << backgroundKey(s.background) << "\n";
    o << "colour_mode=" << colourModeKey(s.colourMode) << "\n";
    o << "vibration=" << vibrationKey(s.vibration) << "\n";
    o << "lightbar=" << lightbarKey(s.lightbar) << "\n";
    for (int i = 0; i < kCrtOptionCount; ++i) o << crtOptionKey(static_cast<CrtOption>(i)) << "=" << crtGet(s.crt, static_cast<CrtOption>(i)) << "\n";
    if (s.desktopScale != 0 || s.desktopX != 0.0f || s.desktopY != 0.0f) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "desktop_x=%.2f\ndesktop_y=%.2f\n", static_cast<double>(s.desktopX), static_cast<double>(s.desktopY));
        o << "desktop_scale=" << s.desktopScale << "\n" << buf;
    }
    if (s.hasWindowRect) {
        o << "window_x=" << s.windowX << "\n";
        o << "window_y=" << s.windowY << "\n";
        o << "window_w=" << s.windowW << "\n";
        o << "window_h=" << s.windowH << "\n";
    }
    return o.str();
}

Settings SettingsStore::parse(const std::string& text) {
    Settings s;
    int x = 0, y = 0, w = 0, h = 0;
    bool haveX = false, haveY = false, haveW = false, haveH = false;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::string value = trim(line.substr(eq + 1));
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });   // BORDERLESS = borderless
        DisplayMode m;
        bool b;
        if (key == "display_mode") {
            if (parseMode(value, m)) s.displayMode = m;
        } else if (key == "window_borders") {
            if (parseBool(value, b)) s.windowBorders = b;
        } else if (key == "colour_mode") {
            ColourMode cm;
            if (parseColourMode(value, cm)) s.colourMode = cm;
        } else if (key == "vibration") {
            VibrationLevel vl;
            if (parseVibration(value, vl)) s.vibration = vl;
        } else if (key == "lightbar") {
            LightbarLevel ll;
            if (parseLightbar(value, ll)) s.lightbar = ll;
        } else if (key == "background") {
            Background bg;
            if (parseBackground(value, bg)) s.background = bg;
        } else if (key == "fps") {   // (removed setting: an old file may still have it; it is ignored)
        } else if (key.rfind("crt_", 0) == 0) {
            int n;
            for (int i = 0; i < kCrtOptionCount; ++i) {
                const CrtOption opt = static_cast<CrtOption>(i);
                if (key == crtOptionKey(opt) && parseInt(value, n) && n >= 0 && n < crtLevelCount(opt)) crtSet(s.crt, opt, n);
            }
        } else if (key == "desktop_scale") {
            int n;
            if (parseInt(value, n) && n >= 0 && n <= 64) s.desktopScale = n;
        } else if (key == "desktop_x" || key == "desktop_y") {
            char* end = nullptr;
            const double d = std::strtod(value.c_str(), &end);
            if (end != value.c_str() && *end == 0 && d > -2000.0 && d < 2000.0) (key == "desktop_x" ? s.desktopX : s.desktopY) = static_cast<float>(d);
        } else if (key == "window_x") {
            haveX = parseInt(value, x);
        } else if (key == "window_y") {
            haveY = parseInt(value, y);
        } else if (key == "window_w") {
            haveW = parseInt(value, w);
        } else if (key == "window_h") {
            haveH = parseInt(value, h);
        }   // anything else (an older or newer version's key): ignored
    }
    if (haveX && haveY && haveW && haveH && w >= 1 && h >= 1 && w <= 20000 && h <= 20000) {
        s.hasWindowRect = true;
        s.windowX = std::clamp(x, -30000, 30000);
        s.windowY = std::clamp(y, -30000, 30000);
        s.windowW = w;
        s.windowH = h;
    }
    return s;
}

Settings SettingsStore::load(bool* fileExisted) const {
    std::ifstream f(std::filesystem::path(path_), std::ios::binary);
    if (fileExisted) *fileExisted = f.good();
    if (!f) return Settings{};
    std::ostringstream all;
    all << f.rdbuf();
    return parse(all.str());
}

bool SettingsStore::save(const Settings& s) const {
    const std::wstring tmp = path_ + L".tmp";
    makeDirectories(path_);
    {
        std::ofstream f(std::filesystem::path(tmp), std::ios::binary | std::ios::trunc);
        if (!f) return false;
        const std::string text = format(s);
        f.write(text.data(), static_cast<std::streamsize>(text.size()));
        f.flush();
        if (!f) {
            f.close();
            DeleteFileW(tmp.c_str());
            return false;
        }
    }
    if (!MoveFileExW(tmp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());   // the old file stays as it was
        return false;
    }
    return true;
}

}  // namespace pacman::app
