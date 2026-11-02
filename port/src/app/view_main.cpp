// pacman_view: shows a built-in synthetic test page (no original data) in the GDI page window. Usage:
//   pacman_view [--pack file.pmpk] [--scale N] [--seconds N] [--shot out.bmp]
// Without --pack: the synthetic test page. With --pack (a local data pack built by pacman_pack from your own verified disk image): the real
// maze walls of the normal game (table $5F69, colour mode), drawn by the C++ hires layer. Exit code 3 if the pack is missing or refused.
// --shot writes the window's own client area to a BMP and exits (used by tests/test_window_shot.py).
#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../datapack/datapack.h"
#include "../verify/verify.h"

#include "../hires/hires.h"
#include "pagewindow.h"

namespace {

void light(pacman::hires::Page& p, int x, int y, bool palette1) {
    const std::size_t off = pacman::hires::rowOffset(y) + static_cast<std::size_t>(x / 7);
    auto b = p.byte(off);
    b = static_cast<std::uint8_t>((b & 0x7F) | (1u << (x % 7)) | (palette1 ? 0x80 : 0));
    p.set(off, b);
}

// Layout (docs/APP_WINDOW.md): rows 0-31 palette 0 isolated pixels (even rows: x%4==0 violet; odd rows: x%4==1 green);
// rows 32-63 palette 1 (even rows blue, odd rows orange); rows 64-95 white block x 100..139;
// frame: rows 100 and 191 full width (white, adjacent pixels); columns 0 and 279 rows 101..190 (isolated: violet / green). Everything else black.
pacman::hires::Page testPage() {
    pacman::hires::Page p;
    p.clear(0);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 280; x += 4) light(p, x + ((y & 1) ? 1 : 0), y, false);
    for (int y = 32; y < 64; ++y)
        for (int x = 0; x < 280; x += 4) light(p, x + ((y & 1) ? 1 : 0), y, true);
    for (int y = 64; y < 96; ++y)
        for (int x = 100; x < 140; ++x) light(p, x, y, false);
    for (int x = 0; x < 280; ++x) {
        light(p, x, 100, false);
        light(p, x, 191, false);
    }
    for (int y = 100; y < 192; ++y) {
        light(p, 0, y, false);
        light(p, 279, y, false);
    }
    return p;
}

// Draws the real maze from the data pack; returns false (and says why) if the pack cannot be used.
bool mazePage(const std::wstring& packPath, pacman::hires::Page& page) {
    std::ifstream file(std::filesystem::path(packPath), std::ios::binary);
    if (!file) {
        std::puts("cannot read the data pack");
        return false;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto loaded = pacman::datapack::loadPack(bytes, pacman::verify::referenceExpected());
    if (!loaded.ok) {
        std::puts(("data pack refused: " + loaded.reason).c_str());
        return false;
    }
    const auto table = loaded.pack.find(0x5F69, pacman::hires::kWallTableBytes);
    if (table.empty() || !pacman::hires::drawWalls(page, table, true)) {
        std::puts("wall table missing from the pack");
        return false;
    }
    return true;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    pacman::app::WindowOptions o;
    std::wstring pack;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--scale" && i + 1 < argc) o.scale = _wtoi(argv[++i]);
        else if (a == L"--seconds" && i + 1 < argc) o.seconds = _wtoi(argv[++i]);
        else if (a == L"--shot" && i + 1 < argc) o.screenshotPath = argv[++i];
        else if (a == L"--pack" && i + 1 < argc) pack = argv[++i];
        else return 64;
    }
    if (pack.empty()) return pacman::app::showPage(testPage(), o);
    pacman::hires::Page page;  // cleared to $80 like the original's page clear, then the walls
    if (!mazePage(pack, page)) return 3;
    return pacman::app::showPage(page, o);
}
