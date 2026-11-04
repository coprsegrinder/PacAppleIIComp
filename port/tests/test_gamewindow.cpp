// Tests of the game window (src/app/gamewindow): size and scale, full screen, hotkeys, key codes, page copy and repaint, a second thread feeding pages.
// They open real (mostly hidden) windows on the test machine's desktop and drive them with scripted messages; nothing is read from the keyboard.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <chrono>
#include <thread>
#include <vector>

#include "../src/app/gamewindow.h"
#include "../src/hires/hires.h"
#include "test_framework.h"

using namespace pacman;
using app::GameWindow;
using app::GameWindowOptions;

namespace {

void pump(int ms) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    MSG m;
    while (std::chrono::steady_clock::now() < end) {
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        Sleep(2);
    }
}

SIZE clientSize(HWND h) {
    RECT r{};
    GetClientRect(h, &r);
    return {r.right - r.left, r.bottom - r.top};
}

// Rows 0..3, the first two bytes (14 adjacent lit pixels): white on screen. The rest is black.
hires::Page whiteBlock() {
    hires::Page p;
    p.clear(0);
    for (int y = 0; y < 4; ++y)
        for (int c = 0; c < 2; ++c) p.set(hires::rowOffset(y) + static_cast<std::size_t>(c), 0x7F);
    return p;
}

// The window's client area through PrintWindow (this window only), as 0xRRGGBB values.
std::vector<unsigned> grab(HWND hwnd, int& w, int& h) {
    const SIZE cs = clientSize(hwnd);
    w = cs.cx;
    h = cs.cy;
    HDC wdc = GetDC(hwnd);
    HDC mem = CreateCompatibleDC(wdc);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(wdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    std::vector<unsigned> out;
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(mem, bmp);
        if (PrintWindow(hwnd, mem, PW_CLIENTONLY | PW_RENDERFULLCONTENT)) {
            const auto* p = static_cast<const unsigned*>(bits);
            out.assign(p, p + static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
            for (auto& v : out) v &= 0xFFFFFF;
        }
        SelectObject(mem, old);
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(hwnd, wdc);
    return out;
}

}  // namespace

TEST(gamewindow_size_and_scale) {
    GameWindow w(GameWindowOptions{3, false});
    CHECK(w.ok());
    if (!w.ok()) return;
    CHECK(w.scale() == 3);
    CHECK(clientSize(w.hwnd()).cx == 840 && clientSize(w.hwnd()).cy == 576);
    CHECK(w.setScale(5));
    CHECK(clientSize(w.hwnd()).cx == 1400 && clientSize(w.hwnd()).cy == 960);
    CHECK(!w.setScale(0));
    CHECK(!w.setScale(9));
    CHECK(w.scale() == 5);
    GameWindow clamped(GameWindowOptions{99, false});
    CHECK(clamped.scale() == 8);
}

TEST(gamewindow_hotkeys_change_scale_and_mute_and_are_not_game_keys) {
    GameWindow w(GameWindowOptions{3, false});
    CHECK(w.ok());
    if (!w.ok()) return;
    CHECK(w.handleHotkey(VK_OEM_PLUS, true, false));
    CHECK(w.scale() == 4);
    CHECK(w.handleHotkey(VK_OEM_MINUS, true, false));
    CHECK(w.handleHotkey(VK_SUBTRACT, true, false));
    CHECK(w.scale() == 2);
    CHECK(!w.handleHotkey(VK_OEM_PLUS, false, false));  // without Ctrl it is not ours
    CHECK(w.scale() == 2);
    for (int i = 0; i < 10; ++i) w.handleHotkey(VK_ADD, true, false);
    CHECK(w.scale() == 8);  // stops at the limit
    CHECK(!w.muted());
    CHECK(w.handleHotkey(VK_F9, false, false));
    CHECK(w.muted());
    CHECK(w.handleHotkey(VK_F9, false, false));
    CHECK(!w.muted());
    // keys the GAME uses must never be taken as hotkeys (Esc is the game's pause)
    for (unsigned vk : {unsigned(VK_ESCAPE), unsigned('A'), unsigned('Z'), unsigned(VK_LEFT), unsigned(VK_RIGHT), unsigned(VK_SPACE), unsigned(VK_RETURN),
                        unsigned('S'), unsigned('1'), unsigned('2')})
        CHECK(!w.handleHotkey(vk, false, false));
    CHECK(!w.handleHotkey('S', true, false));        // Ctrl-S is the game's own sound switch
    CHECK(!w.handleHotkey(VK_F4, false, true));      // Alt+F4 is left to Windows
}

TEST(gamewindow_fullscreen_toggle_and_restore) {
    GameWindow w(GameWindowOptions{3, false});
    CHECK(w.ok());
    if (!w.ok()) return;
    CHECK(w.handleHotkey(VK_F11, false, false));
    CHECK(w.fullscreen());
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(w.hwnd(), MONITOR_DEFAULTTONEAREST), &mi);
    RECT r{};
    GetWindowRect(w.hwnd(), &r);
    CHECK(r.left == mi.rcMonitor.left && r.top == mi.rcMonitor.top && r.right == mi.rcMonitor.right && r.bottom == mi.rcMonitor.bottom);
    const SIZE cs = clientSize(w.hwnd());
    CHECK(w.effectiveScale() == std::max(1, std::min(static_cast<int>(cs.cx) / 280, static_cast<int>(cs.cy) / 192)));
    CHECK(w.setScale(2));  // remembered, not applied to the full-screen size
    CHECK(clientSize(w.hwnd()).cx == cs.cx);
    CHECK(w.handleHotkey(VK_RETURN, false, true));  // Alt+Enter leaves full screen
    CHECK(!w.fullscreen());
    CHECK(clientSize(w.hwnd()).cx == 560 && clientSize(w.hwnd()).cy == 384);  // scale 2 now
}

TEST(gamewindow_key_codes) {
    GameWindow w(GameWindowOptions{3, false});
    CHECK(w.ok());
    if (!w.ok()) return;
    CHECK(!w.popKey().has_value());
    SendMessageW(w.hwnd(), WM_CHAR, 'a', 0);  // lower case becomes capital A = $C1
    SendMessageW(w.hwnd(), WM_CHAR, 'Z', 0);  // $DA
    SendMessageW(w.hwnd(), WM_CHAR, 0x13, 0);  // Ctrl-S = $93
    SendMessageW(w.hwnd(), WM_CHAR, 0x1B, 0);  // Esc = $9B
    SendMessageW(w.hwnd(), WM_CHAR, ' ', 0);   // $A0
    SendMessageW(w.hwnd(), WM_CHAR, '1', 0);   // $B1
    SendMessageW(w.hwnd(), WM_CHAR, 0x0D, 0);  // Return = $8D
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_LEFT, 0);   // $88
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_RIGHT, 0);  // $95
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_UP, 0);     // $C1
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_DOWN, 0);   // $DA
    SendMessageW(w.hwnd(), WM_CHAR, 0xE9, 0);         // not 7-bit: ignored
    const unsigned expect[] = {0xC1, 0xDA, 0x93, 0x9B, 0xA0, 0xB1, 0x8D, 0x88, 0x95, 0xC1, 0xDA};
    for (unsigned e : expect) {
        const auto k = w.popKey();
        CHECK(k.has_value());
        if (k) CHECK(*k == e);
    }
    CHECK(!w.popKey().has_value());
    for (int i = 0; i < 200; ++i) SendMessageW(w.hwnd(), WM_CHAR, 'a', 0);  // the queue is bounded
    int n = 0;
    while (w.popKey()) ++n;
    CHECK(n == 64);
}

TEST(gamewindow_shows_the_page_and_repaints_when_it_changes) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    w.setPage(whiteBlock());
    pump(250);  // the refresh timer notices the new page and repaints
    int cw = 0, ch = 0;
    auto px = grab(w.hwnd(), cw, ch);
    CHECK(px.size() == static_cast<std::size_t>(840 * 576));
    if (px.size() != static_cast<std::size_t>(840 * 576)) return;
    CHECK(px[(0 * 3 + 1) * 840 + 1 * 3 + 1] == 0xFFFFFF);  // inside the lit block (apple pixel 1,0)
    CHECK(px[(10 * 3) * 840 + 100 * 3] == 0x000000);       // far away: black
    hires::Page other;
    other.clear(0);
    w.setPage(other);
    pump(250);
    px = grab(w.hwnd(), cw, ch);
    CHECK(px.size() == static_cast<std::size_t>(840 * 576));
    if (px.size() == static_cast<std::size_t>(840 * 576)) CHECK(px[1 * 840 + 1] == 0x000000);  // the old block is gone
}

TEST(gamewindow_accepts_pages_from_another_thread) {
    GameWindow w(GameWindowOptions{2, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    std::atomic<bool> done{false};
    std::thread feeder([&] {
        for (int i = 0; i < 300; ++i) {
            if (i % 2 == 0) {
                w.setPage(whiteBlock());
            } else {
                hires::Page p;
                p.clear(0);
                w.setPage(p);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        w.setPage(whiteBlock());  // the last page wins
        done = true;
    });
    while (!done) pump(10);
    feeder.join();
    pump(250);
    int cw = 0, ch = 0;
    const auto px = grab(w.hwnd(), cw, ch);
    CHECK(px.size() == static_cast<std::size_t>(560 * 384));
    if (px.size() == static_cast<std::size_t>(560 * 384)) CHECK(px[(0 * 2 + 1) * 560 + 1 * 2 + 1] == 0xFFFFFF);
}

// ---- TASK-034-0/1: the picture through Direct3D 11, and where it sits ----
#include <cstdlib>

#include "../src/app/display_layout.h"
#include "../src/video/colour.h"

namespace {

// A page with every byte different in a repeatable way: all colours, palette bits and neighbouring pixels occur.
hires::Page busyPage() {
    hires::Page p;
    std::uint32_t s = 12345;
    for (std::size_t i = 0; i < hires::kPageBytes; ++i) {
        s = s * 1664525u + 1013904223u;
        p.set(i, static_cast<std::uint8_t>(s >> 24));
    }
    return p;
}

// What the window must show: the picture scaled by a whole number (nearest neighbour), centred, black around it.
std::vector<unsigned> expected(const hires::Page& page, int cw, int ch) {
    std::array<std::uint32_t, 280 * 192> px{};
    video::renderArgb(page, px);
    std::vector<unsigned> out(static_cast<std::size_t>(cw) * static_cast<std::size_t>(ch), 0u);
    const app::Viewport vp = app::computeViewport(cw, ch);
    for (int y = 0; y < vp.h; ++y)
        for (int x = 0; x < vp.w; ++x) out[static_cast<std::size_t>(vp.y + y) * static_cast<std::size_t>(cw) + static_cast<std::size_t>(vp.x + x)] = px[static_cast<std::size_t>(y / vp.scale) * 280 + static_cast<std::size_t>(x / vp.scale)] & 0xFFFFFF;
    return out;
}

std::size_t differences(const std::vector<std::uint32_t>& got, const std::vector<unsigned>& want) {
    if (got.size() != want.size()) return got.size() + want.size() + 1;
    std::size_t bad = 0;
    for (std::size_t i = 0; i < got.size(); ++i) bad += (got[i] & 0xFFFFFF) != want[i];
    return bad;
}

}  // namespace

TEST(display_layout_whole_number_scale_centred_bars_on_every_aspect_ratio) {
    struct Case { int cw, ch, scale, x, y; const char* name; };
    const Case cases[] = {
        {840, 576, 3, 0, 0, "exact 3x"},
        {1920, 1080, 5, 260, 60, "16:9 1080p"},
        {2560, 1080, 5, 580, 60, "21:9 2560x1080"},
        {3440, 1440, 7, 740, 48, "21:9 3440x1440"},
        {5120, 1440, 7, 1580, 48, "32:9"},
        {3840, 2160, 11, 380, 24, "16:9 4K"},
        {1920, 1200, 6, 120, 24, "16:10"},
        {1024, 768, 3, 92, 96, "4:3 1024x768"},
        {1280, 960, 4, 80, 96, "4:3 1280x960"},
        {1280, 720, 3, 220, 72, "16:9 720p"},
        {280, 192, 1, 0, 0, "exactly 1x"},
    };
    for (const Case& c : cases) {
        const app::Viewport v = app::computeViewport(c.cw, c.ch);
        const bool ok = v.scale == c.scale && v.w == 280 * c.scale && v.h == 192 * c.scale && v.x == c.x && v.y == c.y;
        if (!ok) std::printf("    %s: scale %d at %d,%d size %dx%d\n", c.name, v.scale, v.x, v.y, v.w, v.h);
        CHECK(ok);
        CHECK(v.x >= 0 && v.y >= 0 && v.x + v.w <= c.cw && v.y + v.h <= c.ch);                  // inside the screen
        CHECK(std::abs(v.x - (c.cw - v.x - v.w)) <= 1 && std::abs(v.y - (c.ch - v.y - v.h)) <= 1);   // centred
    }
    // smaller than 1x: shrunk by the largest fraction that fits, still 280:192, never larger than the area
    for (const auto& s : {std::pair<int, int>{200, 100}, std::pair<int, int>{100, 300}, std::pair<int, int>{279, 191}, std::pair<int, int>{50, 50}}) {
        const app::Viewport v = app::computeViewport(s.first, s.second);
        CHECK(v.scale == 0 && !v.empty() && v.w <= s.first && v.h <= s.second);
        CHECK(std::abs(static_cast<double>(v.w) / v.h - 280.0 / 192.0) < 0.05);
    }
    CHECK(app::computeViewport(0, 100).empty() && app::computeViewport(100, -5).empty());
}

TEST(direct3d_picture_equals_the_cpu_scaled_picture_byte_for_byte) {
    GameWindow w(GameWindowOptions{3, false});
    CHECK(w.ok());
    if (!w.ok()) return;
    CHECK_MSG(w.usesDirect3d(), "Direct3D 11 (hardware or WARP) could not start on this machine");
    if (!w.usesDirect3d()) return;
    const hires::Page page = busyPage();
    w.setPage(page);
    std::vector<std::uint32_t> got;
    int cw = 0, ch = 0;
    CHECK(w.captureFrame(got, cw, ch));
    CHECK(cw == 840 && ch == 576);
    std::size_t bad = differences(got, expected(page, cw, ch));
    std::printf("    3x window 840 x 576: %zu of %zu pixels differ\n", bad, got.size());
    CHECK(bad == 0);
    // a client area of another shape: bars on every side
    const SIZE sizes[] = {{1000, 700}, {1920, 1080}, {2560, 1080}, {1024, 768}, {1280, 720}};
    for (const SIZE& s : sizes) {
        RECT r{0, 0, s.cx, s.cy};
        AdjustWindowRect(&r, static_cast<DWORD>(GetWindowLongW(w.hwnd(), GWL_STYLE)), FALSE);
        SetWindowPos(w.hwnd(), nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        pump(30);
        CHECK(w.captureFrame(got, cw, ch));
        CHECK(cw == s.cx && ch == s.cy);
        bad = differences(got, expected(page, cw, ch));
        std::printf("    client %ld x %ld: scale %d, %zu pixels differ\n", s.cx, s.cy, app::computeViewport(cw, ch).scale, bad);
        CHECK(bad == 0);
    }
}

TEST(direct3d_full_screen_toggle_keeps_the_picture_and_the_game_untouched) {
    GameWindow w(GameWindowOptions{3, false});
    CHECK(w.ok());
    if (!w.ok() || !w.usesDirect3d()) return;
    const hires::Page page = busyPage();
    w.setPage(page);
    for (int round = 0; round < 3; ++round) {
        CHECK(w.handleHotkey(VK_RETURN, false, true));   // Alt+Enter: full screen
        pump(30);
        std::vector<std::uint32_t> got;
        int cw = 0, ch = 0;
        CHECK(w.captureFrame(got, cw, ch));
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(MonitorFromWindow(w.hwnd(), MONITOR_DEFAULTTONEAREST), &mi);
        CHECK(cw == mi.rcMonitor.right - mi.rcMonitor.left && ch == mi.rcMonitor.bottom - mi.rcMonitor.top);
        const std::size_t bad = differences(got, expected(page, cw, ch));
        std::printf("    round %d full screen %d x %d: %zu pixels differ\n", round, cw, ch, bad);
        CHECK(bad == 0);
        CHECK(w.handleHotkey(VK_RETURN, false, true));   // and back
        pump(30);
        CHECK(w.captureFrame(got, cw, ch));
        CHECK(cw == 840 && ch == 576 && differences(got, expected(page, cw, ch)) == 0);
    }
}

TEST(gdi_fallback_still_draws_when_direct3d_is_switched_off) {
    _putenv_s("PACMAN_GDI", "1");
    {
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (w.ok()) {
            CHECK(!w.usesDirect3d());
            w.setPage(whiteBlock());
            pump(250);
            int cw = 0, ch = 0;
            const auto px = grab(w.hwnd(), cw, ch);
            CHECK(px.size() == static_cast<std::size_t>(840 * 576));
            if (px.size() == static_cast<std::size_t>(840 * 576)) CHECK(px[(0 * 3 + 1) * 840 + 1 * 3 + 1] == 0xFFFFFF && px[(10 * 3) * 840 + 100 * 3] == 0x000000);
        }
    }
    _putenv_s("PACMAN_GDI", "");
}
