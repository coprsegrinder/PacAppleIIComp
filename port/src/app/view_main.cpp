// pacman_view: shows a built-in synthetic test page (no original data) in the GDI page window. Usage:
//   pacman_view [--pack file.pmpk [--page maze|text]] [--scale N] [--seconds N] [--shot out.bmp]
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
#include "../hires/replay.h"
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

// Loads the local data pack; returns false (and says why) if it cannot be used.
bool loadPackFile(const std::wstring& packPath, pacman::datapack::LoadResult& loaded) {
    std::ifstream file(std::filesystem::path(packPath), std::ios::binary);
    if (!file) {
        std::puts("cannot read the data pack");
        return false;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    loaded = pacman::datapack::loadPack(bytes, pacman::verify::referenceExpected());
    if (!loaded.ok) {
        std::puts(("data pack refused: " + loaded.reason).c_str());
        return false;
    }
    return true;
}

// The 12 attract-screen strings ($434F-$43D1) printed with the original text printer, all on one page.
bool textPage(const pacman::datapack::Pack& pack, pacman::hires::Page& page) {
    const auto glyphs = pack.find(0x4553, pacman::hires::kGlyphTableBytes);
    const auto strings = pack.find(0x434F, 0x43D2 - 0x434F);
    if (glyphs.empty() || strings.empty()) {
        std::puts("text data missing from the pack");
        return false;
    }
    std::size_t pos = 0;
    while (pos < strings.size()) {
        const std::size_t used = pacman::hires::drawString(page, glyphs, strings.subspan(pos));
        if (used == 0) return false;
        pos += used;
    }
    return true;
}

// Replays a recorded drawing script (tools/capture_script.py) and shows the page it ends on.
bool scriptPage(const pacman::datapack::Pack& pack, const std::wstring& scriptPath, std::uint32_t calls, pacman::hires::Page& page) {
    std::ifstream file(std::filesystem::path(scriptPath), std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    pacman::hires::ReplayStats stats;
    if (!file || !pacman::hires::replayScript(pack, bytes, page, stats, calls)) {
        std::puts("cannot replay the script");
        return false;
    }
    return true;
}

// Draws the real maze with its dots.
bool mazePage(const pacman::datapack::Pack& pack, pacman::hires::Page& page) {
    const auto& loaded = pack;
    const auto table = loaded.find(0x5F69, pacman::hires::kWallTableBytes);
    if (table.empty() || !pacman::hires::drawWalls(page, table, true)) {
        std::puts("wall table missing from the pack");
        return false;
    }
    // the dots and energizers of a fresh game: the pristine map copy at $65BA (the game copies it to $623A at game start, $5331)
    const auto map = loaded.find(0x65BA, pacman::hires::kMazeMapBytes);
    const auto dot = loaded.find(0x61A8, pacman::hires::kDotTableBytes);
    const auto tile = loaded.find(0x61CA, pacman::hires::kTileTableBytes);
    if (map.empty() || dot.empty() || tile.empty()) {
        std::puts("dot data missing from the pack");
        return false;
    }
    pacman::hires::drawDots(page, map, dot, tile);
    return true;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    pacman::app::WindowOptions o;
    std::wstring pack;
    bool text = false;
    std::wstring script;
    std::uint32_t calls = 0;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--scale" && i + 1 < argc) o.scale = _wtoi(argv[++i]);
        else if (a == L"--seconds" && i + 1 < argc) o.seconds = _wtoi(argv[++i]);
        else if (a == L"--shot" && i + 1 < argc) o.screenshotPath = argv[++i];
        else if (a == L"--page" && i + 1 < argc) {
            const std::wstring w = argv[++i];
            if (w == L"text") text = true;
            else if (w != L"maze") return 64;
        } else if (a == L"--calls" && i + 1 < argc) {
            calls = static_cast<std::uint32_t>(_wtoi(argv[++i]));
        } else if (a == L"--script" && i + 1 < argc) {
            script = argv[++i];
        } else if (a == L"--pack" && i + 1 < argc) pack = argv[++i];
        else return 64;
    }
    if (pack.empty()) return pacman::app::showPage(testPage(), o);
    pacman::hires::Page page;  // cleared to $80 like the original's page clear, then the walls
    pacman::datapack::LoadResult loaded;
    if (!loadPackFile(pack, loaded)) return 3;
    const bool ok = !script.empty() ? scriptPage(loaded.pack, script, calls, page) : (text ? textPage(loaded.pack, page) : mazePage(loaded.pack, page));
    if (!ok) return 3;
    return pacman::app::showPage(page, o);
}
