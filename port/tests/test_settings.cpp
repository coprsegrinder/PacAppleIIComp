// TASK-034-4: the settings file: defaults, round trip, tolerant reading, atomic writing.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "../src/app/settings.h"
#include "test_framework.h"

using namespace pacman;
using app::DisplayMode;
using app::Settings;
using app::SettingsStore;

namespace {

std::filesystem::path tempDir(const char* name) {
    const std::filesystem::path d = std::filesystem::temp_directory_path() / (std::string("pacman_settings_test_") + name);
    std::filesystem::remove_all(d);
    std::filesystem::create_directories(d);
    return d;
}

std::string slurp(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream o;
    o << f.rdbuf();
    return o.str();
}

void spit(const std::filesystem::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << text;
}

}  // namespace

TEST(settings_defaults_when_there_is_no_usable_file) {
    const Settings d;
    CHECK(d.displayMode == DisplayMode::Borderless && d.windowBorders && !d.hasWindowRect);   // first run: borderless full screen
    const auto dir = tempDir("defaults");
    bool existed = true;
    CHECK(SettingsStore((dir / "none.ini").wstring()).load(&existed) == d && !existed);   // missing
    spit(dir / "empty.ini", "");
    CHECK(SettingsStore((dir / "empty.ini").wstring()).load(&existed) == d && existed);
    spit(dir / "garbage.ini", std::string("\x01\x02\xFF\xFE not a settings file \r\n=\n==\nkey\n"));
    CHECK(SettingsStore((dir / "garbage.ini").wstring()).load() == d);
    CHECK(SettingsStore((dir / "a" / "b" / "c.ini").wstring()).load() == d);   // folder that does not exist
    std::filesystem::remove_all(dir);
}

TEST(settings_round_trip_and_tolerant_reading) {
    Settings s;
    s.displayMode = DisplayMode::Windowed;
    s.windowBorders = false;
    s.hasWindowRect = true;
    s.windowX = -1200;   // a monitor to the left of the main one
    s.windowY = 90;
    s.windowW = 1120;
    s.windowH = 768;
    CHECK(SettingsStore::parse(SettingsStore::format(s)) == s);
    for (DisplayMode m : {DisplayMode::Borderless, DisplayMode::Windowed}) {
        Settings t;
        t.displayMode = m;
        CHECK(SettingsStore::parse(SettingsStore::format(t)).displayMode == m);
    }
    // an edited, older or newer file: comments, spaces, capitals, CRLF, unknown keys
    const Settings p = SettingsStore::parse("# mine\r\n  Display_Mode =  WINDOWED \r\nfuture_key=7\r\nwindow_borders = off\r\nfullscreen_kind=fullscreen\r\n\r\nversion=99\r\n");
    CHECK(p.displayMode == DisplayMode::Windowed && !p.windowBorders && !p.hasWindowRect);   // (an old fullscreen_kind line is ignored)
    // bad values keep the default of that key and do not disturb the others
    const Settings q = SettingsStore::parse("display_mode=sideways\nfullscreen_kind=windowed\nwindow_borders=maybe\nwindow_x=12\nwindow_y=ab\nwindow_w=300\nwindow_h=200\n");
    CHECK(q.displayMode == DisplayMode::Borderless && q.windowBorders && !q.hasWindowRect);   // the rectangle needs all four good numbers
    CHECK(SettingsStore::parse("display_mode=fullscreen\n").displayMode == DisplayMode::Borderless);   // the removed exclusive mode of an older file: borderless full screen
    CHECK(!SettingsStore::parse("window_x=1\nwindow_y=1\nwindow_w=0\nwindow_h=5\n").hasWindowRect);
    CHECK(!SettingsStore::parse("window_x=1\nwindow_y=1\nwindow_w=99999\nwindow_h=5\n").hasWindowRect);
    CHECK(!SettingsStore::parse("window_x=99999999999\nwindow_y=1\nwindow_w=10\nwindow_h=5\n").hasWindowRect);
    CHECK(SettingsStore::parse("window_x=-5\nwindow_y=-6\nwindow_w=10\nwindow_h=5\n").hasWindowRect);
}

TEST(settings_are_written_atomically_and_a_failure_leaves_the_old_file_alone) {
    const auto dir = tempDir("atomic");
    const SettingsStore store((dir / "deep" / "er" / "settings.ini").wstring());   // the folders are made
    Settings a;
    a.displayMode = DisplayMode::Windowed;
    CHECK(store.save(a));
    CHECK(store.load() == a);
    CHECK(!std::filesystem::exists(dir / "deep" / "er" / "settings.ini.tmp"));   // no temporary file left
    Settings b = a;
    b.displayMode = DisplayMode::Borderless;
    b.windowBorders = false;
    CHECK(store.save(b) && store.load() == b);   // replaced
    // the temporary file cannot be created (something else of that name is in the way): save fails, the file is as it was
    std::filesystem::create_directories(dir / "deep" / "er" / "settings.ini.tmp");
    const std::string before = slurp(dir / "deep" / "er" / "settings.ini");
    CHECK(!store.save(a));
    CHECK(slurp(dir / "deep" / "er" / "settings.ini") == before && store.load() == b);
    std::filesystem::remove_all(dir / "deep" / "er" / "settings.ini.tmp");
    // the target is a folder: the move fails, the temporary file is cleaned up
    std::filesystem::create_directories(dir / "folder.ini");
    const SettingsStore bad((dir / "folder.ini").wstring());
    CHECK(!bad.save(a));
    CHECK(!std::filesystem::exists(dir / "folder.ini.tmp"));
    // a path that cannot exist at all
    CHECK(!SettingsStore(L"Z:\\no\\such\\drive\\settings.ini").save(a));
    // a read-only file: the move over it fails and it stays as it was
    spit(dir / "ro.ini", "display_mode=windowed\n");
    SetFileAttributesW((dir / "ro.ini").c_str(), FILE_ATTRIBUTE_READONLY);
    const SettingsStore ro((dir / "ro.ini").wstring());
    CHECK(!ro.save(b));
    CHECK(slurp(dir / "ro.ini") == "display_mode=windowed\n" && !std::filesystem::exists(dir / "ro.ini.tmp"));
    SetFileAttributesW((dir / "ro.ini").c_str(), FILE_ATTRIBUTE_NORMAL);
    std::filesystem::remove_all(dir);
}

TEST(settings_default_path_is_in_the_user_profile_and_can_be_redirected) {
    _putenv("PACMAN_SETTINGS=");
    const std::wstring normal = SettingsStore::defaultPath();
    CHECK(normal.find(L"PacmanAppleIIPort") != std::wstring::npos && normal.find(L"settings.ini") != std::wstring::npos);
    CHECK(normal.find(L"Program Files") == std::wstring::npos);   // somewhere the program may write
    _putenv("PACMAN_SETTINGS=C:\\somewhere\\else.ini");
    CHECK(SettingsStore::defaultPath() == L"C:\\somewhere\\else.ini");
    _putenv("PACMAN_SETTINGS=");   // removes it again
}
