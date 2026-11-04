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
    SendMessageW(w.hwnd(), WM_CHAR, 0x1B, 0);  // Esc is the pause menu key now (TASK-034-3): it is not an Apple II key any more
    SendMessageW(w.hwnd(), WM_CHAR, ' ', 0);   // $A0
    SendMessageW(w.hwnd(), WM_CHAR, '1', 0);   // $B1
    SendMessageW(w.hwnd(), WM_CHAR, 0x0D, 0);  // Return = $8D
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_LEFT, 0);   // $88
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_RIGHT, 0);  // $95
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_UP, 0);     // $C1
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_DOWN, 0);   // $DA
    SendMessageW(w.hwnd(), WM_CHAR, 0xE9, 0);         // not 7-bit: ignored
    const unsigned expect[] = {0xC1, 0xDA, 0x93, 0xA0, 0xB1, 0x8D, 0x88, 0x95, 0xC1, 0xDA};
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

// A VISIBLE window must stay visible, cover its monitor, and show the picture in full screen and back (the hidden-window test above cannot see this: it found no bug where users saw
// a black screen with a white block, 2026-11-04).
TEST(visible_window_full_screen_stays_visible_and_shows_the_picture) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    const hires::Page page = busyPage();
    w.setPage(page);
    pump(100);
    CHECK(IsWindowVisible(w.hwnd()) != FALSE);
    w.handleHotkey(VK_F11, false, false);
    pump(200);
    CHECK(w.fullscreen());
    CHECK_MSG(IsWindowVisible(w.hwnd()) != FALSE, "the window is hidden after going full screen");
    CHECK((GetWindowLongW(w.hwnd(), GWL_STYLE) & WS_VISIBLE) != 0);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(w.hwnd(), MONITOR_DEFAULTTONEAREST), &mi);
    RECT r{};
    GetWindowRect(w.hwnd(), &r);
    CHECK(r.left == mi.rcMonitor.left && r.top == mi.rcMonitor.top && r.right == mi.rcMonitor.right && r.bottom == mi.rcMonitor.bottom);
    if (w.usesDirect3d()) {
        std::vector<std::uint32_t> got;
        int cw = 0, ch = 0;
        CHECK(w.captureFrame(got, cw, ch));
        CHECK(cw == mi.rcMonitor.right - mi.rcMonitor.left && ch == mi.rcMonitor.bottom - mi.rcMonitor.top);
        CHECK(differences(got, expected(page, cw, ch)) == 0);
    }
    w.handleHotkey(VK_F11, false, false);
    pump(200);
    CHECK(!w.fullscreen() && IsWindowVisible(w.hwnd()) != FALSE);
    CHECK(clientSize(w.hwnd()).cx == 840 && clientSize(w.hwnd()).cy == 576);
}

// ---- TASK-034-1: full screen must not touch the game or its clock ----
#include "../src/core/cycleclock.h"
#include "../src/core/gamedriver.h"
#include "flow_common.h"

TEST(full_screen_switching_while_the_game_runs_changes_nothing_in_the_game) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    constexpr int kTicks = 150000;
    struct Result { std::vector<std::uint8_t> page; unsigned score = 0, level = 0; int toggles = 0; };
    const auto run = [&](bool toggle) {
        Result res;
        core::GameDriver drv;
        CHECK(drv.init(pack) && drv.newGame());
        GameWindow w(GameWindowOptions{3, false});
        CHECK(w.ok());
        std::atomic<bool> done{false};
        std::thread worker([&] {   // the game thread, as in the program: ticks, and the newest page goes to the window
            for (int t = 0; t < kTicks; ++t) {
                drv.tick();
                if (t % 3 == 0) w.setPage(drv.page());
                if (t % 1000 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            done = true;
        });
        while (!done) {
            if (toggle) {
                w.setFullscreen(!w.fullscreen());
                ++res.toggles;
            }
            pump(2);
        }
        worker.join();
        if (w.fullscreen()) w.setFullscreen(false);
        for (std::size_t i = 0; i < hires::kPageBytes; ++i) res.page.push_back(drv.page().byte(i));
        res.score = drv.core().state().zp(0x52);
        res.level = drv.core().state().level();
        return res;
    };
    const Result plain = run(false);
    const Result toggled = run(true);
    std::printf("    %d ticks; %d full screen switches while it ran; page equal %d, score byte %u / %u\n", kTicks, toggled.toggles, plain.page == toggled.page, plain.score, toggled.score);
    CHECK(toggled.toggles >= 3);
    CHECK(plain.page == toggled.page);
    CHECK(plain.score == toggled.score && plain.level == toggled.level);
}

TEST(full_screen_switching_does_not_change_the_cycles_the_clock_hands_out) {
    GameWindow w(GameWindowOptions{3, false});
    CHECK(w.ok());
    core::CycleClock clock;
    const std::int64_t t0 = core::nowNanoseconds();
    clock.start(t0);
    std::atomic<bool> done{false};
    std::int64_t handed = 0, endNs = 0;
    std::thread worker([&] {   // the game thread's pacing loop
        while (core::nowNanoseconds() - t0 < 800'000'000) {
            handed += clock.cyclesDue(core::nowNanoseconds());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        endNs = core::nowNanoseconds();
        handed += clock.cyclesDue(endNs);
        done = true;
    });
    int toggles = 0;
    while (!done) {
        w.setFullscreen(!w.fullscreen());
        ++toggles;
        pump(20);
    }
    worker.join();
    const std::int64_t exact = clock.cyclesIn(endNs - t0);
    std::printf("    %d switches in 0.8 s; cycles handed out %lld, exact for the elapsed time %lld, skipped %lld\n", toggles, static_cast<long long>(handed), static_cast<long long>(exact), static_cast<long long>(clock.skipped()));
    CHECK(toggles >= 10);
    CHECK(handed == exact);        // not one cycle lost or gained
    CHECK(clock.skipped() == 0);   // the pacing never fell behind
}

TEST(full_screen_holding_the_key_down_switches_once_and_a_display_change_refits_the_window) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    const LPARAM first = 1, repeat = (1 << 30) | 1;
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_F11, first);
    CHECK(w.fullscreen());
    for (int i = 0; i < 5; ++i) SendMessageW(w.hwnd(), WM_KEYDOWN, VK_F11, repeat);   // auto-repeat of the held key
    CHECK(w.fullscreen());
    // the window is moved/resized by something else (a display change): it covers its monitor again
    SetWindowPos(w.hwnd(), nullptr, 10, 10, 400, 300, SWP_NOZORDER | SWP_NOACTIVATE);
    SendMessageW(w.hwnd(), WM_DISPLAYCHANGE, 32, 0);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(w.hwnd(), MONITOR_DEFAULTTONEAREST), &mi);
    RECT r{};
    GetWindowRect(w.hwnd(), &r);
    CHECK(r.left == mi.rcMonitor.left && r.top == mi.rcMonitor.top && r.right == mi.rcMonitor.right && r.bottom == mi.rcMonitor.bottom);
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_F11, first);
    CHECK(!w.fullscreen());
}

TEST(background_fill_reaches_the_bars_and_not_the_picture) {
    GameWindow w(GameWindowOptions{3, false});
    CHECK(w.ok());
    if (!w.ok() || !w.usesDirect3d()) return;
    CHECK(w.background() == app::Background::Black);
    CHECK(app::backgroundFill(app::Background::Black).r == 0.0f);
    const hires::Page page = busyPage();
    w.setPage(page);
    RECT r{0, 0, 1000, 700};
    AdjustWindowRect(&r, static_cast<DWORD>(GetWindowLongW(w.hwnd(), GWL_STYLE)), FALSE);
    SetWindowPos(w.hwnd(), nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    pump(30);
    w.setBackground(app::Background::Black);
    std::vector<std::uint32_t> got;
    int cw = 0, ch = 0;
    CHECK(w.captureFrame(got, cw, ch));
    const app::Viewport vp = app::computeViewport(cw, ch);
    std::size_t barsNotBlack = 0, bars = 0;
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x) {
            const bool inside = x >= vp.x && x < vp.x + vp.w && y >= vp.y && y < vp.y + vp.h;
            if (!inside) {
                ++bars;
                barsNotBlack += (got[static_cast<std::size_t>(y) * static_cast<std::size_t>(cw) + static_cast<std::size_t>(x)] & 0xFFFFFF) != 0;
            }
        }
    std::printf("    %zu bar pixels, %zu not black\n", bars, barsNotBlack);
    CHECK(bars > 0 && barsNotBlack == 0);
    CHECK(differences(got, expected(page, cw, ch)) == 0);
}

// ---- TASK-034-2: windowed mode: free resizing, the picture centred at a whole-number scale, optional removal of the title bar and frame ----
namespace {

// Gives the window this client area (the outer size follows the style and the DPI of the window).
void setClient(HWND h, int cw, int ch) {
    const DWORD style = static_cast<DWORD>(GetWindowLongW(h, GWL_STYLE));
    RECT r{0, 0, cw, ch};
    if ((style & WS_CAPTION) == WS_CAPTION) AdjustWindowRectExForDpi(&r, style, FALSE, 0, GetDpiForWindow(h));
    SetWindowPos(h, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

POINT clientOrigin(HWND h) {
    POINT p{0, 0};
    ClientToScreen(h, &p);
    return p;
}

// The part of the frame that is not black outside the viewport (must be nothing) and inside it (must be most of it).
void checkShape(const std::vector<std::uint32_t>& got, int cw, int ch, const app::Viewport& vp, std::size_t& outsideLit, std::size_t& insideLit, std::size_t& inside) {
    outsideLit = insideLit = inside = 0;
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x) {
            const bool in = x >= vp.x && x < vp.x + vp.w && y >= vp.y && y < vp.y + vp.h;
            const bool lit = (got[static_cast<std::size_t>(y) * static_cast<std::size_t>(cw) + static_cast<std::size_t>(x)] & 0xFFFFFF) != 0;
            if (in) {
                ++inside;
                insideLit += lit;
            } else {
                outsideLit += lit;
            }
        }
}

}  // namespace

TEST(windowed_free_resizing_keeps_the_picture_shape_centred_with_bars) {
    GameWindow w(GameWindowOptions{3, false});
    CHECK(w.ok());
    if (!w.ok() || !w.usesDirect3d()) return;
    const hires::Page page = busyPage();
    w.setPage(page);
    // any size: whole-number scale where it fits (the picture is compared pixel for pixel), a fractional fit below 1x (shape and bars are checked)
    const SIZE sizes[] = {{840, 576}, {777, 555}, {1000, 700}, {1500, 400}, {400, 900}, {2000, 1200}, {281, 193}, {200, 140}, {150, 100}};
    for (const SIZE& s : sizes) {
        setClient(w.hwnd(), s.cx, s.cy);
        pump(20);
        std::vector<std::uint32_t> got;
        int cw = 0, ch = 0;
        CHECK(w.captureFrame(got, cw, ch));
        CHECK(cw == s.cx && ch == s.cy);
        const app::Viewport vp = app::computeViewport(cw, ch);
        if (vp.scale >= 1) {
            const std::size_t bad = differences(got, expected(page, cw, ch));
            std::printf("    client %ld x %ld: scale %d at %d,%d, %zu pixels differ\n", s.cx, s.cy, vp.scale, vp.x, vp.y, bad);
            CHECK(bad == 0);
        } else {
            std::size_t outside = 0, lit = 0, inside = 0;
            checkShape(got, cw, ch, vp, outside, lit, inside);
            std::printf("    client %ld x %ld: fractional fit %d x %d at %d,%d, %zu lit outside, %zu of %zu lit inside\n", s.cx, s.cy, vp.w, vp.h, vp.x, vp.y, outside, lit, inside);
            CHECK(outside == 0 && lit > inside / 4);   // the test page is about half lit; nothing outside the viewport
            CHECK(std::abs(static_cast<double>(vp.w) / vp.h - 280.0 / 192.0) < 0.02);
        }
    }
    // the smallest the window can be is a half-size picture; a smaller request is raised to it
    setClient(w.hwnd(), 10, 10);
    const SIZE tiny = clientSize(w.hwnd());
    CHECK(tiny.cx >= 140 && tiny.cy >= 96);
}

TEST(windowed_leaving_full_screen_gives_back_the_window_as_it_was) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    setClient(w.hwnd(), 1000, 700);
    SetWindowPos(w.hwnd(), nullptr, 120, 90, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    pump(20);
    const POINT o0 = clientOrigin(w.hwnd());
    const SIZE s0 = clientSize(w.hwnd());
    w.setFullscreen(true);
    pump(30);
    w.setFullscreen(false);
    pump(30);
    CHECK(clientSize(w.hwnd()).cx == s0.cx && clientSize(w.hwnd()).cy == s0.cy);   // 1000 x 700, not the preset size
    CHECK(clientOrigin(w.hwnd()).x == o0.x && clientOrigin(w.hwnd()).y == o0.y);
    CHECK(IsWindowVisible(w.hwnd()) != FALSE);
    // Ctrl+Plus / Ctrl+Minus step from the scale the picture has now (1000 x 700 is scale 3)
    CHECK(w.handleHotkey(VK_ADD, true, false));
    CHECK(clientSize(w.hwnd()).cx == 1120 && clientSize(w.hwnd()).cy == 768);
    setClient(w.hwnd(), 1000, 700);
    CHECK(w.handleHotkey(VK_SUBTRACT, true, false));
    CHECK(clientSize(w.hwnd()).cx == 560 && clientSize(w.hwnd()).cy == 384);
}

TEST(borderless_windowed_mode_removes_title_bar_and_frame_and_keeps_resizing) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    const hires::Page page = busyPage();
    w.setPage(page);
    setClient(w.hwnd(), 1000, 700);
    pump(30);
    CHECK(w.borders());
    CHECK((GetWindowLongW(w.hwnd(), GWL_STYLE) & WS_CAPTION) == WS_CAPTION);
    const POINT o0 = clientOrigin(w.hwnd());
    const SIZE s0 = clientSize(w.hwnd());
    for (int round = 0; round < 6; ++round) {   // off and on again: the picture area never moves or changes size
        CHECK(w.handleHotkey(VK_F8, false, false));
        pump(20);
        CHECK(!w.borders());
        const LONG style = GetWindowLongW(w.hwnd(), GWL_STYLE);
        CHECK((style & WS_CAPTION) != WS_CAPTION && (style & WS_THICKFRAME) != 0 && (style & WS_VISIBLE) != 0);   // no title bar; still resizable; still shown
        CHECK(IsWindowVisible(w.hwnd()) != FALSE);
        CHECK(clientSize(w.hwnd()).cx == s0.cx && clientSize(w.hwnd()).cy == s0.cy);
        CHECK(clientOrigin(w.hwnd()).x == o0.x && clientOrigin(w.hwnd()).y == o0.y);
        RECT wr{};
        GetWindowRect(w.hwnd(), &wr);
        CHECK(wr.left == o0.x && wr.top == o0.y && wr.right - wr.left == s0.cx && wr.bottom - wr.top == s0.cy);   // the whole window is the picture area
        if (w.usesDirect3d()) {
            std::vector<std::uint32_t> got;
            int cw = 0, ch = 0;
            CHECK(w.captureFrame(got, cw, ch));
            CHECK(cw == s0.cx && ch == s0.cy && differences(got, expected(page, cw, ch)) == 0);
        }
        CHECK(w.handleHotkey(VK_F8, false, false));
        pump(20);
        CHECK(w.borders() && (GetWindowLongW(w.hwnd(), GWL_STYLE) & WS_CAPTION) == WS_CAPTION);
        CHECK(clientSize(w.hwnd()).cx == s0.cx && clientSize(w.hwnd()).cy == s0.cy);
        CHECK(clientOrigin(w.hwnd()).x == o0.x && clientOrigin(w.hwnd()).y == o0.y);
    }
    // without the frame: resize edges and the dragging area are found by the window itself
    w.setBorders(false);
    pump(20);
    RECT wr{};
    GetWindowRect(w.hwnd(), &wr);
    const auto hit = [&](int x, int y) { return static_cast<int>(SendMessageW(w.hwnd(), WM_NCHITTEST, 0, MAKELPARAM(static_cast<short>(x), static_cast<short>(y)))); };
    const int midX = (wr.left + wr.right) / 2, midY = (wr.top + wr.bottom) / 2;
    CHECK(hit(wr.left + 1, midY) == HTLEFT && hit(wr.right - 2, midY) == HTRIGHT);
    CHECK(hit(midX, wr.top + 1) == HTTOP && hit(midX, wr.bottom - 2) == HTBOTTOM);
    CHECK(hit(wr.left + 1, wr.top + 1) == HTTOPLEFT && hit(wr.right - 2, wr.top + 1) == HTTOPRIGHT);
    CHECK(hit(wr.left + 1, wr.bottom - 2) == HTBOTTOMLEFT && hit(wr.right - 2, wr.bottom - 2) == HTBOTTOMRIGHT);
    CHECK(hit(midX, midY) == HTCAPTION);   // dragging the picture moves the window
    // it can still be given any size (the same call the resize edges end in); the picture follows
    for (const SIZE& s : {SIZE{840, 576}, SIZE{1280, 720}, SIZE{700, 900}}) {
        setClient(w.hwnd(), s.cx, s.cy);
        pump(20);
        CHECK(clientSize(w.hwnd()).cx == s.cx && clientSize(w.hwnd()).cy == s.cy);
        if (w.usesDirect3d()) {
            std::vector<std::uint32_t> got;
            int cw = 0, ch = 0;
            CHECK(w.captureFrame(got, cw, ch));
            CHECK(cw == s.cx && ch == s.cy && differences(got, expected(page, cw, ch)) == 0);
        }
    }
    // full screen with the frame removed: back from it the window is still bare and the picture area is the same
    w.setFullscreen(true);
    pump(30);
    CHECK(w.setScale(2));   // asked for while in full screen: applies on the way back
    w.setBorders(true);   // changed while in full screen: applies on the way back
    w.setFullscreen(false);
    pump(30);
    CHECK(w.borders() && (GetWindowLongW(w.hwnd(), GWL_STYLE) & WS_CAPTION) == WS_CAPTION && IsWindowVisible(w.hwnd()) != FALSE);
    CHECK(clientSize(w.hwnd()).cx == 560 && clientSize(w.hwnd()).cy == 384);   // the scale asked for meanwhile
}

// The game thread must not notice any of it: window size changes and frame removal while 150,000 game ticks run leave the page, score and level exactly as without.
TEST(resizing_and_removing_the_frame_while_the_game_runs_changes_nothing_in_the_game) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    constexpr int kTicks = 150000;
    struct Result { std::vector<std::uint8_t> page; unsigned score = 0, level = 0; int changes = 0; };
    const auto run = [&](bool disturb) {
        Result res;
        core::GameDriver drv;
        CHECK(drv.init(pack) && drv.newGame());
        GameWindow w(GameWindowOptions{3, false});
        CHECK(w.ok());
        std::atomic<bool> done{false};
        std::thread worker([&] {
            for (int t = 0; t < kTicks; ++t) {
                drv.tick();
                if (t % 3 == 0) w.setPage(drv.page());
                if (t % 1000 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            done = true;
        });
        const SIZE sizes[] = {{840, 576}, {1000, 700}, {1280, 720}, {500, 900}, {2000, 800}, {300, 200}};
        while (!done) {
            if (disturb) {
                setClient(w.hwnd(), sizes[res.changes % 6].cx, sizes[res.changes % 6].cy);
                if (res.changes % 4 == 3) w.setBorders(!w.borders());
                ++res.changes;
            }
            pump(2);
        }
        worker.join();
        for (std::size_t i = 0; i < hires::kPageBytes; ++i) res.page.push_back(drv.page().byte(i));
        res.score = drv.core().state().zp(0x52);
        res.level = drv.core().state().level();
        return res;
    };
    const Result plain = run(false);
    const Result disturbed = run(true);
    std::printf("    %d ticks; %d size/frame changes while it ran; page equal %d, score byte %u / %u\n", kTicks, disturbed.changes, plain.page == disturbed.page, plain.score, disturbed.score);
    CHECK(disturbed.changes >= 6);
    CHECK(plain.page == disturbed.page && plain.score == disturbed.score && plain.level == disturbed.level);
}

TEST(resizing_does_not_change_the_cycles_the_clock_hands_out) {
    GameWindow w(GameWindowOptions{3, false});
    CHECK(w.ok());
    core::CycleClock clock;
    const std::int64_t t0 = core::nowNanoseconds();
    clock.start(t0);
    std::atomic<bool> done{false};
    std::int64_t handed = 0, endNs = 0;
    std::thread worker([&] {
        while (core::nowNanoseconds() - t0 < 800'000'000) {
            handed += clock.cyclesDue(core::nowNanoseconds());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        endNs = core::nowNanoseconds();
        handed += clock.cyclesDue(endNs);
        done = true;
    });
    int changes = 0;
    while (!done) {
        setClient(w.hwnd(), 400 + (changes * 97) % 1400, 300 + (changes * 61) % 700);
        if (changes % 5 == 4) w.setBorders(!w.borders());
        ++changes;
        pump(15);
    }
    worker.join();
    const std::int64_t exact = clock.cyclesIn(endNs - t0);
    std::printf("    %d size/frame changes in 0.8 s; cycles handed out %lld, exact %lld, skipped %lld\n", changes, static_cast<long long>(handed), static_cast<long long>(exact), static_cast<long long>(clock.skipped()));
    CHECK(changes >= 10);
    CHECK(handed == exact && clock.skipped() == 0);
}

// ---- TASK-034-3: the pause menu ----
#include "../src/app/audio.h"
#include "../src/app/pausegate.h"
#include "../src/app/pausemenu.h"
#include "../src/core/textscreen.h"

namespace {

using app::PauseMenu;

const char* charRomPath() { return "Apple II roms/Apple II Character ROM - 341-0036.bin"; }

bool loadFont(core::CharacterRom& rom) {
    if (rom.load(charRomPath())) return true;
    std::printf("    (character ROM not found at %s)\n", charRomPath());
    return false;
}

// The centre of an item of the menu in picture coordinates.
// The centre of an entry of a page of the menu, in picture coordinates (the page is reached by keys).
POINT itemPointOn(PauseMenu::Page page, int index) {
    PauseMenu m;
    m.show();
    if (page == PauseMenu::Page::Graphics) {
        m.key(PauseMenu::Key::Down);
        m.key(PauseMenu::Key::Select);
    } else if (page == PauseMenu::Page::Confirm) {
        m.key(PauseMenu::Key::Down);
        m.key(PauseMenu::Key::Down);
        m.key(PauseMenu::Key::Select);
    }
    const PauseMenu::Point c = m.itemCenter(index);
    return POINT{c.x, c.y};
}
POINT mainItem(int i) { return itemPointOn(PauseMenu::Page::Main, i); }
POINT graphicsItem(int i) { return itemPointOn(PauseMenu::Page::Graphics, i); }
POINT confirmItem(int i) { return itemPointOn(PauseMenu::Page::Confirm, i); }

// A client position (for a window message) of a picture pixel at the window's present scale.
LPARAM clientPos(HWND h, int px, int py) {
    RECT rc{};
    GetClientRect(h, &rc);
    const app::Viewport vp = app::computeViewport(static_cast<int>(rc.right), static_cast<int>(rc.bottom));
    return MAKELPARAM(static_cast<short>(vp.x + px * vp.w / 280), static_cast<short>(vp.y + py * vp.h / 192));
}

}  // namespace

TEST(pause_menu_keyboard_navigation_pages_and_actions) {
    PauseMenu m;
    CHECK(!m.open() && m.key(PauseMenu::Key::Down) == PauseMenu::Action::None);   // closed: keys do nothing
    m.show();
    CHECK(m.open() && m.page() == PauseMenu::Page::Main && m.selected() == 0 && m.itemCount() == 3);
    // the three entries in the specified order, selection wraps round
    m.key(PauseMenu::Key::Down);
    CHECK(m.selected() == 1);
    m.key(PauseMenu::Key::Down);
    CHECK(m.selected() == 2);
    m.key(PauseMenu::Key::Down);
    CHECK(m.selected() == 0);
    m.key(PauseMenu::Key::Up);
    CHECK(m.selected() == 2);
    // Resume closes it
    m.show();
    CHECK(m.key(PauseMenu::Key::Select) == PauseMenu::Action::Resume && !m.open());
    // Esc on the main page resumes
    m.show();
    CHECK(m.key(PauseMenu::Key::Back) == PauseMenu::Action::Resume && !m.open());
    // Graphics Settings: its page, the toggle, Back and Esc return to the main page on the entry that led there
    m.show();
    m.key(PauseMenu::Key::Down);
    CHECK(m.key(PauseMenu::Key::Select) == PauseMenu::Action::None && m.page() == PauseMenu::Page::Graphics && m.selected() == 0 && m.itemCount() == 5);
    CHECK(m.key(PauseMenu::Key::Select) == PauseMenu::Action::NextDisplayMode && m.page() == PauseMenu::Page::Graphics);   // Display Mode: Enter = next
    CHECK(m.key(PauseMenu::Key::Right) == PauseMenu::Action::NextDisplayMode && m.key(PauseMenu::Key::Left) == PauseMenu::Action::PreviousDisplayMode);
    m.key(PauseMenu::Key::Down);
    CHECK(m.selected() == 1 && m.key(PauseMenu::Key::Select) == PauseMenu::Action::NextResolution && m.key(PauseMenu::Key::Right) == PauseMenu::Action::NextResolution && m.key(PauseMenu::Key::Left) == PauseMenu::Action::PreviousResolution);   // Resolution
    m.key(PauseMenu::Key::Down);
    CHECK(m.selected() == 2 && m.key(PauseMenu::Key::Select) == PauseMenu::Action::ToggleBorders && m.key(PauseMenu::Key::Left) == PauseMenu::Action::ToggleBorders);   // Window Borders
    m.key(PauseMenu::Key::Down);
    CHECK(m.selected() == 3 && m.key(PauseMenu::Key::Select) == PauseMenu::Action::NextBackground && m.key(PauseMenu::Key::Right) == PauseMenu::Action::NextBackground && m.key(PauseMenu::Key::Left) == PauseMenu::Action::PreviousBackground);   // Background
    m.key(PauseMenu::Key::Down);
    CHECK(m.selected() == 4 && m.key(PauseMenu::Key::Left) == PauseMenu::Action::None);   // Back: left/right do nothing
    m.key(PauseMenu::Key::Select);   // Back
    CHECK(m.page() == PauseMenu::Page::Main && m.selected() == 1);
    m.key(PauseMenu::Key::Select);
    CHECK(m.page() == PauseMenu::Page::Graphics);
    CHECK(m.key(PauseMenu::Key::Back) == PauseMenu::Action::None && m.page() == PauseMenu::Page::Main && m.selected() == 1 && m.open());
    // Quit Game asks first; Cancel is selected, so Enter twice does not quit; Esc cancels; Confirm quits
    m.key(PauseMenu::Key::Down);
    CHECK(m.selected() == 2);
    CHECK(m.key(PauseMenu::Key::Select) == PauseMenu::Action::None && m.page() == PauseMenu::Page::Confirm && m.selected() == 1);
    CHECK(m.key(PauseMenu::Key::Select) == PauseMenu::Action::None && m.page() == PauseMenu::Page::Main && m.selected() == 2);   // Cancel: back to the menu
    m.key(PauseMenu::Key::Select);
    CHECK(m.key(PauseMenu::Key::Back) == PauseMenu::Action::None && m.page() == PauseMenu::Page::Main && m.selected() == 2);   // Esc = cancel
    m.key(PauseMenu::Key::Select);
    m.key(PauseMenu::Key::Up);
    CHECK(m.selected() == 0);
    CHECK(m.key(PauseMenu::Key::Select) == PauseMenu::Action::Quit);
}

TEST(pause_menu_mouse_selects_on_hover_activates_on_click_and_ignores_the_rest) {
    PauseMenu m;
    m.show();
    for (int i = 0; i < 3; ++i) {
        const POINT p = mainItem(i);
        CHECK(m.itemAt(p.x, p.y) == i);
        m.pointerMove(p.x, p.y);
        CHECK(m.selected() == i);
    }
    CHECK(m.itemAt(5, 5) == -1 && m.itemAt(140, 40) == -1 && m.itemAt(140, 150) == -1 && m.itemAt(-1, -1) == -1 && m.itemAt(2, 76) == -1);
    m.pointerMove(5, 5);
    CHECK(m.selected() == 2);   // moving over nothing keeps the selection
    CHECK(m.pointerClick(5, 5) == PauseMenu::Action::None && m.page() == PauseMenu::Page::Main);   // a click outside the entries does nothing
    // click Graphics Settings, then the border entry, then Back
    CHECK(m.pointerClick(mainItem(1).x, mainItem(1).y) == PauseMenu::Action::None && m.page() == PauseMenu::Page::Graphics);
    CHECK(m.pointerClick(graphicsItem(0).x, graphicsItem(0).y) == PauseMenu::Action::NextDisplayMode);
    CHECK(m.pointerClick(graphicsItem(1).x, graphicsItem(1).y) == PauseMenu::Action::NextResolution);
    CHECK(m.pointerClick(graphicsItem(2).x, graphicsItem(2).y) == PauseMenu::Action::ToggleBorders);
    CHECK(m.pointerClick(graphicsItem(3).x, graphicsItem(3).y) == PauseMenu::Action::NextBackground);
    CHECK(m.pointerClick(graphicsItem(4).x, graphicsItem(4).y) == PauseMenu::Action::None && m.page() == PauseMenu::Page::Main);
    // Quit Game -> Cancel by mouse, Quit Game -> Confirm by mouse
    CHECK(m.pointerClick(mainItem(2).x, mainItem(2).y) == PauseMenu::Action::None && m.page() == PauseMenu::Page::Confirm);
    CHECK(m.pointerClick(confirmItem(1).x, confirmItem(1).y) == PauseMenu::Action::None && m.page() == PauseMenu::Page::Main);
    m.pointerClick(mainItem(2).x, mainItem(2).y);
    CHECK(m.pointerClick(confirmItem(0).x, confirmItem(0).y) == PauseMenu::Action::Quit);
    // Resume by mouse
    m.show();
    CHECK(m.pointerClick(mainItem(0).x, mainItem(0).y) == PauseMenu::Action::Resume && !m.open());
}

TEST(pause_menu_draws_inside_its_panel_with_the_apple_ii_font_and_marks_the_selection) {
    core::CharacterRom rom;
    if (!loadFont(rom)) return;
    PauseMenu m;
    std::vector<std::uint8_t> closed;
    PauseMenu::View view;
    m.render(rom, view, closed);
    CHECK(closed.size() == 280u * 192u);
    std::size_t lit = 0;
    for (auto v : closed) lit += v;
    CHECK(lit == 0);   // closed: nothing
    m.show();
    std::vector<std::uint8_t> a, b;
    m.render(rom, view, a);
    m.key(PauseMenu::Key::Down);
    m.render(rom, view, b);
    std::size_t outside = 0, inside = 0, differ = 0;
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 280; ++x) {
            const bool in = x >= PauseMenu::kPanelX && x < PauseMenu::kPanelX + PauseMenu::kPanelW && y >= PauseMenu::kPanelY && y < PauseMenu::kPanelY + PauseMenu::kPanelH;
            const std::uint8_t v = a[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)];
            if (!in) outside += v;
            else inside += v;
            differ += v != b[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)];
        }
    std::printf("    menu: %zu lit pixels inside the panel, %zu outside, %zu change with the selection\n", inside, outside, differ);
    CHECK(outside == 0 && inside > 600);
    CHECK(differ > 200);   // moving the selection redraws two highlighted bands
    // the graphics page shows the border state
    PauseMenu g;
    g.show();
    g.key(PauseMenu::Key::Down);
    g.key(PauseMenu::Key::Select);
    std::vector<std::uint8_t> on, off;
    PauseMenu::View viewOn, viewOff, viewMode;
    viewOff.borders = false;
    viewMode.displayMode = app::DisplayMode::Windowed;
    std::vector<std::uint8_t> mode;
    g.render(rom, viewOn, on);
    g.render(rom, viewOff, off);
    g.render(rom, viewMode, mode);
    CHECK(on != off);       // the border state is shown
    CHECK(on != mode);      // the display mode is shown
}

TEST(pause_gate_holds_the_game_and_no_time_is_made_up_afterwards) {
    core::CycleClock clock;
    app::PauseGate gate;
    int paused = 0, resumed = 0;
    gate.onPause = [&] { ++paused; };
    gate.onResume = [&] { ++resumed; };
    std::int64_t now = 1'000'000'000, ran = 0;
    clock.start(now);
    const auto runFor = [&](std::int64_t ms, bool menuOpen) {
        for (std::int64_t t = 0; t < ms; ++t) {
            now += 1'000'000;
            if (gate.update(menuOpen, now, clock)) continue;   // the game loop's "continue"
            ran += clock.cyclesDue(now);
        }
    };
    runFor(100, false);
    const std::int64_t before = ran;
    runFor(5000, true);   // five seconds in the menu
    CHECK(ran == before && gate.held() && paused == 1 && resumed == 0);   // not one cycle earned
    runFor(100, false);
    CHECK(!gate.held() && resumed == 1);
    // 200 ms of running time in total; the five seconds are not made up (no fast-forward after resume)
    const std::int64_t exact = clock.cyclesIn(200'000'000);
    std::printf("    cycles run %lld, exact for 200 ms %lld, skipped %lld\n", static_cast<long long>(ran), static_cast<long long>(exact), static_cast<long long>(clock.skipped()));
    CHECK(ran >= exact - 2000 && ran <= exact);
    CHECK(clock.skipped() == 0);
    runFor(10, false);
    CHECK(paused == 1 && resumed == 1);   // the hooks fire once per change
}

TEST(pause_menu_in_the_window_esc_opens_it_the_game_never_sees_the_keys_and_it_closes_again) {
    core::CharacterRom rom;
    const bool haveFont = loadFont(rom);
    GameWindow w(GameWindowOptions{3, false});
    CHECK(w.ok());
    if (!w.ok()) return;
    if (haveFont) w.setMenuFont(rom);
    const hires::Page page = busyPage();
    w.setPage(page);
    CHECK(!w.menuOpen());
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    SendMessageW(w.hwnd(), WM_CHAR, 0x1B, 0);   // the character message that goes with it
    CHECK(w.menuOpen());
    CHECK(!w.popKey().has_value());   // Esc is not an Apple II key any more
    // while open, the game gets nothing, whatever is pressed
    for (unsigned vk : {unsigned(VK_UP), unsigned(VK_DOWN), unsigned(VK_DOWN), unsigned(VK_UP), unsigned(VK_LEFT), unsigned(VK_RIGHT), unsigned('A'), unsigned('Z')}) SendMessageW(w.hwnd(), WM_KEYDOWN, vk, 1);
    SendMessageW(w.hwnd(), WM_CHAR, 'a', 0);
    SendMessageW(w.hwnd(), WM_CHAR, 0x0D, 0);
    CHECK(!w.popKey().has_value());
    // the picture while open: the game at a quarter of its brightness outside the panel, the panel black with white text
    if (w.usesDirect3d()) {
        std::vector<std::uint32_t> got;
        int cw = 0, ch = 0;
        CHECK(w.captureFrame(got, cw, ch));
        std::array<std::uint32_t, 280 * 192> game{};
        video::renderArgb(page, game);
        std::size_t dimmedOk = 0, dimmedTotal = 0, panelWhite = 0;
        for (int y = 0; y < 192; ++y)
            for (int x = 0; x < 280; ++x) {
                const bool in = x >= PauseMenu::kPanelX && x < PauseMenu::kPanelX + PauseMenu::kPanelW && y >= PauseMenu::kPanelY && y < PauseMenu::kPanelY + PauseMenu::kPanelH;
                const std::uint32_t v = got[static_cast<std::size_t>(y * 3 + 1) * static_cast<std::size_t>(cw) + static_cast<std::size_t>(x * 3 + 1)] & 0xFFFFFF;
                if (in) {
                    panelWhite += v == 0xFFFFFF;
                } else {
                    ++dimmedTotal;
                    dimmedOk += v == (((game[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] & 0xFCFCFC) >> 2) & 0xFFFFFF);
                }
            }
        std::printf("    open: %zu of %zu pixels outside the panel are the game at 1/4 brightness, %zu white pixels in the panel\n", dimmedOk, dimmedTotal, panelWhite);
        CHECK(dimmedOk == dimmedTotal);
        if (haveFont) CHECK(panelWhite > 600);
    }
    // Esc closes it again: the picture is exactly the game's, and keys reach the game again
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK(!w.menuOpen());
    if (w.usesDirect3d()) {
        std::vector<std::uint32_t> got;
        int cw = 0, ch = 0;
        CHECK(w.captureFrame(got, cw, ch));
        CHECK(differences(got, expected(page, cw, ch)) == 0);
    }
    SendMessageW(w.hwnd(), WM_CHAR, 'a', 0);
    const auto k = w.popKey();
    CHECK(k.has_value() && *k == 0xC1);
    // holding Esc down opens and closes it once, not repeatedly
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    for (int i = 0; i < 5; ++i) SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, (1 << 30) | 1);
    CHECK(w.menuOpen());
}

TEST(pause_menu_in_the_window_mouse_keyboard_graphics_page_and_quit_with_confirmation) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    const HWND h = w.hwnd();
    SendMessageW(h, WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK(w.menuOpen());
    // mouse: Graphics Settings, then the border entry toggles the window frame
    CHECK(w.borders());
    SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, clientPos(h, mainItem(1).x, mainItem(1).y));
    SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, clientPos(h, graphicsItem(2).x, graphicsItem(2).y));
    CHECK(!w.borders() && w.menuOpen());
    SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, clientPos(h, graphicsItem(2).x, graphicsItem(2).y));
    CHECK(w.borders());
    // the mouse works at another window size too (the picture position is mapped through the viewport)
    SetWindowPos(h, nullptr, 0, 0, 1100, 800, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    pump(20);
    SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, clientPos(h, graphicsItem(4).x, graphicsItem(4).y));   // Back
    SendMessageW(h, WM_MOUSEMOVE, 0, clientPos(h, mainItem(2).x, mainItem(2).y));
    SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, clientPos(h, mainItem(2).x, mainItem(2).y));    // Quit Game: asks first
    CHECK(w.ok() && w.menuOpen());
    SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, clientPos(h, confirmItem(1).x, confirmItem(1).y));    // Cancel
    CHECK(w.ok() && w.menuOpen());
    // keyboard, from a fresh menu: Down, Down, Enter (asks), Enter (Cancel), Up, Up, then Resume by Enter
    SendMessageW(h, WM_KEYDOWN, VK_ESCAPE, 1);   // back to the game
    CHECK(!w.menuOpen());
    SendMessageW(h, WM_KEYDOWN, VK_ESCAPE, 1);   // and open again: Resume is selected
    CHECK(w.menuOpen());
    SendMessageW(h, WM_KEYDOWN, VK_DOWN, 1);
    SendMessageW(h, WM_KEYDOWN, VK_DOWN, 1);
    CHECK(w.menuOpen());
    SendMessageW(h, WM_KEYDOWN, VK_RETURN, 1);
    SendMessageW(h, WM_KEYDOWN, VK_RETURN, 1);
    CHECK(w.menuOpen() && w.ok());
    SendMessageW(h, WM_KEYDOWN, VK_UP, 1);
    SendMessageW(h, WM_KEYDOWN, VK_UP, 1);
    SendMessageW(h, WM_KEYDOWN, VK_RETURN, 1);   // Resume
    CHECK(!w.menuOpen());
    // Esc, Quit Game, Confirm: the window closes (the program then ends in the normal order)
    SendMessageW(h, WM_KEYDOWN, VK_ESCAPE, 1);
    SendMessageW(h, WM_KEYDOWN, VK_UP, 1);
    SendMessageW(h, WM_KEYDOWN, VK_RETURN, 1);   // Quit Game (wrapped round from Resume)
    SendMessageW(h, WM_KEYDOWN, VK_UP, 1);       // Confirm
    CHECK(w.ok());
    SendMessageW(h, WM_KEYDOWN, VK_RETURN, 1);
    pump(100);
    CHECK(!w.ok());   // WM_CLOSE was handled: the window is gone
}

TEST(pause_menu_borderless_window_lets_the_mouse_through_while_open_and_full_screen_hides_the_pointer) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    w.setBorders(false);
    pump(20);
    RECT wr{};
    GetWindowRect(w.hwnd(), &wr);
    const LPARAM mid = MAKELPARAM(static_cast<short>((wr.left + wr.right) / 2), static_cast<short>((wr.top + wr.bottom) / 2));
    CHECK(SendMessageW(w.hwnd(), WM_NCHITTEST, 0, mid) == HTCAPTION);   // the picture moves the window
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK(SendMessageW(w.hwnd(), WM_NCHITTEST, 0, mid) == HTCLIENT);    // the menu needs the clicks
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK(SendMessageW(w.hwnd(), WM_NCHITTEST, 0, mid) == HTCAPTION);
    // full screen: the pointer is hidden until the menu is open
    w.setFullscreen(true);
    pump(30);
    CHECK(SendMessageW(w.hwnd(), WM_SETCURSOR, reinterpret_cast<WPARAM>(w.hwnd()), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)) == TRUE);
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK(SendMessageW(w.hwnd(), WM_SETCURSOR, reinterpret_cast<WPARAM>(w.hwnd()), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)) != TRUE);
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    w.setFullscreen(false);
}

TEST(pause_audio_stands_still_and_continues_from_the_same_sample) {
    app::AudioOutput audio;
    if (!audio.open(44100)) {
        std::printf("    SKIPPED (no audio device)\n");
        return;
    }
    // one second of a tone, played: after half a second of play and a pause of one second, the sound still has about half a second left
    core::Pcm tone;
    tone.sampleRate = 44100;
    tone.samples.assign(44100, 0);   // silence is enough: the position is what is measured
    audio.play(tone);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    audio.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    CHECK(audio.busy());              // paused: still not finished although 1.5 s have passed
    audio.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK(audio.busy());              // about 200 ms are left
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    CHECK(!audio.busy());             // done: it finished about 1 s later than it would have without the pause
    audio.close();
}

TEST(pause_menu_only_while_a_game_is_running_esc_does_nothing_before_that) {
    GameWindowOptions o{3, false};
    o.menuAllowed = false;   // as the program starts: boot menus, attract loop, prompts
    GameWindow w(o);
    CHECK(w.ok());
    if (!w.ok()) return;
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    SendMessageW(w.hwnd(), WM_CHAR, 0x1B, 0);
    CHECK(!w.menuOpen());
    CHECK(!w.popKey().has_value());   // and the original Esc pause does not come back either
    SendMessageW(w.hwnd(), WM_CHAR, 'a', 0);   // other keys still reach the program (the boot menus need them)
    CHECK(w.popKey().has_value());
    for (unsigned vk : {unsigned(VK_UP), unsigned(VK_DOWN), unsigned(VK_RETURN)}) SendMessageW(w.hwnd(), WM_KEYDOWN, vk, 1);
    SendMessageW(w.hwnd(), WM_CHAR, 0x0D, 0);
    CHECK(w.popKey().has_value() && !w.menuOpen());   // Enter etc. are ordinary keys here
    w.setMenuAllowed(true);                           // a game starts
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK(w.menuOpen());
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK(!w.menuOpen());
    w.setMenuAllowed(false);                          // the game is over
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK(!w.menuOpen());
    w.setMenuAllowed(true);                           // a menu that is open when the game ends is closed
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK(w.menuOpen());
    w.setMenuAllowed(false);
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_DOWN, 1);
    CHECK(!w.menuOpen());
}

// ---- TASK-034-4: display mode (Fullscreen / Borderless Fullscreen / Windowed) and the settings that keep it ----
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

using app::DisplayMode;
using app::Settings;
using app::SettingsStore;

std::string fileText(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream o;
    o << f.rdbuf();
    return o.str();
}

RECT monitorRectOf(HWND h) {
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
    return mi.rcMonitor;
}

std::filesystem::path scratchDir(const char* name) {
    const std::filesystem::path d = std::filesystem::temp_directory_path() / (std::string("pacman_display_test_") + name);
    std::filesystem::remove_all(d);
    std::filesystem::create_directories(d);
    return d;
}

}  // namespace

TEST(display_mode_windowed_and_borderless_fullscreen_switch_back_and_forth_exactly) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    CHECK(w.displayMode() == DisplayMode::Windowed && !w.fullscreen());
    const hires::Page page = busyPage();
    w.setPage(page);
    setClient(w.hwnd(), 1000, 700);
    SetWindowPos(w.hwnd(), nullptr, 100, 80, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    pump(30);
    const POINT o0 = clientOrigin(w.hwnd());
    for (int round = 0; round < 4; ++round) {
        // Borderless Fullscreen: the desktop's own resolution, the picture centred at the largest whole scale
        w.setDisplayMode(DisplayMode::Borderless);
        pump(60);
        CHECK(w.displayMode() == DisplayMode::Borderless && w.fullscreen());
        const RECT mon = monitorRectOf(w.hwnd());
        RECT wr{};
        GetWindowRect(w.hwnd(), &wr);
        CHECK(wr.left == mon.left && wr.top == mon.top && wr.right == mon.right && wr.bottom == mon.bottom);
        CHECK(clientSize(w.hwnd()).cx == mon.right - mon.left && clientSize(w.hwnd()).cy == mon.bottom - mon.top);   // native resolution, not one of a list
        CHECK(IsWindowVisible(w.hwnd()) != FALSE);
        if (w.usesDirect3d()) {
            std::vector<std::uint32_t> got;
            int cw = 0, ch = 0;
            CHECK(w.captureFrame(got, cw, ch));
            CHECK(cw == mon.right - mon.left && ch == mon.bottom - mon.top && differences(got, expected(page, cw, ch)) == 0);
        }
        // and back: exactly the window the user had (same picture area, same place), the same frame, still shown
        w.setDisplayMode(DisplayMode::Windowed);
        pump(60);
        CHECK(w.displayMode() == DisplayMode::Windowed && !w.fullscreen() && IsWindowVisible(w.hwnd()) != FALSE);
        CHECK(clientSize(w.hwnd()).cx == 1000 && clientSize(w.hwnd()).cy == 700 && clientOrigin(w.hwnd()).x == o0.x && clientOrigin(w.hwnd()).y == o0.y);
        CHECK((GetWindowLongW(w.hwnd(), GWL_STYLE) & WS_CAPTION) == WS_CAPTION);
        if (w.usesDirect3d()) {
            std::vector<std::uint32_t> got;
            int cw = 0, ch = 0;
            CHECK(w.captureFrame(got, cw, ch));
            CHECK(cw == 1000 && ch == 700 && differences(got, expected(page, cw, ch)) == 0);
        }
    }
    // the same mode twice does nothing harmful
    w.setDisplayMode(DisplayMode::Windowed);
    w.setDisplayMode(DisplayMode::Windowed);
    CHECK(clientSize(w.hwnd()).cx == 1000 && clientSize(w.hwnd()).cy == 700);
    w.setDisplayMode(DisplayMode::Borderless);
    w.setDisplayMode(DisplayMode::Borderless);
    pump(60);
    CHECK(w.fullscreen());
    w.setDisplayMode(DisplayMode::Windowed);
    pump(60);
    CHECK(clientSize(w.hwnd()).cx == 1000 && clientSize(w.hwnd()).cy == 700);
}

TEST(display_mode_f11_and_alt_enter_go_between_the_window_and_borderless_fullscreen) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    CHECK(w.handleHotkey(VK_F11, false, false));
    pump(60);
    CHECK(w.displayMode() == DisplayMode::Borderless && w.fullscreen());
    CHECK(w.handleHotkey(VK_RETURN, false, true));
    pump(60);
    CHECK(w.displayMode() == DisplayMode::Windowed && !w.fullscreen());
}

TEST(display_mode_is_changed_from_the_pause_menu_with_keys_and_mouse_and_the_menu_stays) {
    core::CharacterRom rom;
    const bool haveFont = loadFont(rom);
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    if (haveFont) w.setMenuFont(rom);
    const HWND h = w.hwnd();
    SendMessageW(h, WM_KEYDOWN, VK_ESCAPE, 1);
    SendMessageW(h, WM_KEYDOWN, VK_DOWN, 1);
    SendMessageW(h, WM_KEYDOWN, VK_RETURN, 1);   // Graphics Settings; "Display Mode" is the first entry
    CHECK(w.menuOpen() && w.displayMode() == DisplayMode::Windowed);
    SendMessageW(h, WM_KEYDOWN, VK_LEFT, 1);     // the other mode: Windowed -> Borderless Fullscreen
    pump(60);
    CHECK(w.displayMode() == DisplayMode::Borderless && w.fullscreen() && w.menuOpen());
    // the menu is drawn on the full screen picture too
    if (w.usesDirect3d() && haveFont) {
        std::vector<std::uint32_t> got;
        int cw = 0, ch = 0;
        CHECK(w.captureFrame(got, cw, ch));
        const app::Viewport vp = app::computeViewport(cw, ch);
        std::size_t white = 0;
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x) white += (got[static_cast<std::size_t>(y) * static_cast<std::size_t>(cw) + static_cast<std::size_t>(x)] & 0xFFFFFF) == 0xFFFFFF && x >= vp.x && x < vp.x + vp.w && y >= vp.y && y < vp.y + vp.h;
        CHECK(white > 600u * static_cast<std::size_t>(vp.scale) * static_cast<std::size_t>(vp.scale) / 2);
    }
    SendMessageW(h, WM_KEYDOWN, VK_RIGHT, 1);    // Borderless -> Windowed
    pump(60);
    CHECK(w.displayMode() == DisplayMode::Windowed && !w.fullscreen());
    SendMessageW(h, WM_KEYDOWN, VK_RETURN, 1);   // Enter: Windowed -> Borderless Fullscreen
    pump(60);
    CHECK(w.displayMode() == DisplayMode::Borderless && w.fullscreen() && w.menuOpen());
    // the mouse: click the Display Mode entry until it is the window again
    const POINT entry = graphicsItem(0);
    for (int i = 0; i < 2 && w.displayMode() != DisplayMode::Windowed; ++i) {
        SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, clientPos(h, entry.x, entry.y));
        pump(300);
    }
    CHECK(w.displayMode() == DisplayMode::Windowed && !w.fullscreen() && w.menuOpen());
    SendMessageW(h, WM_KEYDOWN, VK_ESCAPE, 1);
    SendMessageW(h, WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK(!w.menuOpen());
}

TEST(display_settings_are_saved_when_they_change_and_restored_at_the_next_start) {
    const auto dir = scratchDir("restore");
    const std::filesystem::path file = dir / "settings.ini";
    SettingsStore store(file.wstring());
    Settings start;
    start.displayMode = DisplayMode::Windowed;   // the tests must not start in full screen
    POINT origin{0, 0};
    {
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        w.attachSettings(&store, start);
        setClient(w.hwnd(), 1000, 700);
        SetWindowPos(w.hwnd(), nullptr, 150, 120, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        pump(40);
        w.setBorders(false);
        pump(40);
        CHECK(fileText(file).find("window_borders=0") != std::string::npos);   // written at once, not only on exit
        setClient(w.hwnd(), 900, 600);
        pump(40);
        origin = clientOrigin(w.hwnd());
        w.setDisplayMode(DisplayMode::Borderless);
        pump(100);
        CHECK(fileText(file).find("display_mode=borderless") != std::string::npos);
        // the window is closed while in full screen: the window as the user had it is what is kept
    }
    const Settings kept = store.load();
    std::printf("    kept: mode %s, borders %d, window %d,%d %dx%d\n", app::displayModeKey(kept.displayMode), kept.windowBorders, kept.windowX, kept.windowY, kept.windowW, kept.windowH);
    CHECK(kept.displayMode == DisplayMode::Borderless && !kept.windowBorders && kept.hasWindowRect);
    CHECK(kept.windowW == 900 && kept.windowH == 600 && kept.windowX == origin.x && kept.windowY == origin.y);
    {   // the next start: the same mode, frame, size and place
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        Settings s = kept;
        s.displayMode = DisplayMode::Windowed;
        w.attachSettings(&store, s);
        pump(60);
        CHECK(w.displayMode() == DisplayMode::Windowed && !w.borders());
        CHECK(clientSize(w.hwnd()).cx == 900 && clientSize(w.hwnd()).cy == 600 && clientOrigin(w.hwnd()).x == origin.x && clientOrigin(w.hwnd()).y == origin.y);
        w.setDisplayMode(DisplayMode::Borderless);
        pump(60);
        w.setDisplayMode(DisplayMode::Windowed);
        pump(100);
        CHECK(clientSize(w.hwnd()).cx == 900 && clientSize(w.hwnd()).cy == 600 && clientOrigin(w.hwnd()).x == origin.x && clientOrigin(w.hwnd()).y == origin.y);   // and back from full screen to the same place
    }
    {   // the saved display mode itself is what the program starts in
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        w.attachSettings(&store, store.load());
        pump(100);
        CHECK(w.displayMode() == DisplayMode::Windowed);   // the second window above ended in Windowed
        w.setDisplayMode(DisplayMode::Borderless);
    }
    {
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        w.attachSettings(&store, store.load());
        pump(100);
        CHECK(w.displayMode() == DisplayMode::Borderless && w.fullscreen());
    }
    std::filesystem::remove_all(dir);
}

TEST(display_settings_with_a_window_place_that_no_longer_exists_are_made_reachable) {
    const auto dir = scratchDir("offscreen");
    SettingsStore store((dir / "settings.ini").wstring());
    Settings s;
    s.displayMode = DisplayMode::Windowed;
    s.hasWindowRect = true;
    s.windowX = 40000;   // a monitor that is not there any more
    s.windowY = -9000;
    s.windowW = 600;
    s.windowH = 400;
    {
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        w.attachSettings(&store, s);
        pump(60);
        RECT wr{};
        GetWindowRect(w.hwnd(), &wr);
        CHECK(MonitorFromRect(&wr, MONITOR_DEFAULTTONULL) != nullptr);   // on a monitor
        CHECK(clientSize(w.hwnd()).cx == 600 && clientSize(w.hwnd()).cy == 400);
    }
    s.windowX = 100;
    s.windowY = 100;
    s.windowW = 9000;   // larger than any screen
    s.windowH = 7000;
    {
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        w.attachSettings(&store, s);
        pump(60);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(MonitorFromWindow(w.hwnd(), MONITOR_DEFAULTTONEAREST), &mi);
        RECT wr{};
        GetWindowRect(w.hwnd(), &wr);
        CHECK(wr.right - wr.left <= mi.rcWork.right - mi.rcWork.left && wr.bottom - wr.top <= mi.rcWork.bottom - mi.rcWork.top);   // cut to the work area
        CHECK(clientSize(w.hwnd()).cx >= 140 && clientSize(w.hwnd()).cy >= 96);
    }
    s.windowW = 3;   // smaller than a window may be
    s.windowH = 2;
    {
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        w.attachSettings(&store, s);
        pump(60);
        CHECK(clientSize(w.hwnd()).cx >= 140 && clientSize(w.hwnd()).cy >= 96);
    }
    std::filesystem::remove_all(dir);
}

TEST(display_mode_changes_while_the_game_runs_leave_the_game_and_its_clock_untouched) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    constexpr int kTicks = 150000;
    struct Result { std::vector<std::uint8_t> page; unsigned score = 0, level = 0; int changes = 0; };
    const auto run = [&](bool disturb) {
        Result res;
        core::GameDriver drv;
        CHECK(drv.init(pack) && drv.newGame());
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        std::atomic<bool> done{false};
        std::thread worker([&] {
            for (int t = 0; t < kTicks; ++t) {
                drv.tick();
                if (t % 3 == 0) w.setPage(drv.page());
                if (t % 1000 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            done = true;
        });
        const DisplayMode order[2] = {DisplayMode::Borderless, DisplayMode::Windowed};
        while (!done) {
            if (disturb) {
                w.setDisplayMode(order[res.changes % 2]);
                ++res.changes;
            }
            pump(20);
        }
        worker.join();
        if (w.displayMode() != DisplayMode::Windowed) w.setDisplayMode(DisplayMode::Windowed);
        for (std::size_t i = 0; i < hires::kPageBytes; ++i) res.page.push_back(drv.page().byte(i));
        res.score = drv.core().state().zp(0x52);
        res.level = drv.core().state().level();
        return res;
    };
    const Result plain = run(false);
    const Result disturbed = run(true);
    std::printf("    %d ticks; %d display mode changes while it ran; page equal %d\n", kTicks, disturbed.changes, plain.page == disturbed.page);
    CHECK(disturbed.changes >= 3);
    CHECK(plain.page == disturbed.page && plain.score == disturbed.score && plain.level == disturbed.level);
    // and the clock
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    core::CycleClock clock;
    const std::int64_t t0 = core::nowNanoseconds();
    clock.start(t0);
    std::atomic<bool> done{false};
    std::int64_t handed = 0, endNs = 0;
    std::thread worker([&] {
        while (core::nowNanoseconds() - t0 < 1'000'000'000) {
            handed += clock.cyclesDue(core::nowNanoseconds());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        endNs = core::nowNanoseconds();
        handed += clock.cyclesDue(endNs);
        done = true;
    });
    const DisplayMode order[2] = {DisplayMode::Borderless, DisplayMode::Windowed};
    int changes = 0;
    while (!done) {
        w.setDisplayMode(order[changes++ % 2]);
        pump(120);
    }
    worker.join();
    if (w.displayMode() != DisplayMode::Windowed) w.setDisplayMode(DisplayMode::Windowed);
    const std::int64_t exact = clock.cyclesIn(endNs - t0);
    std::printf("    %d display mode changes in 1 s; cycles handed out %lld, exact %lld, skipped %lld\n", changes, static_cast<long long>(handed), static_cast<long long>(exact), static_cast<long long>(clock.skipped()));
    CHECK(changes >= 4);
    CHECK(handed == exact && clock.skipped() == 0);
}

// ---- TASK-034-5: Resolution ----
#include "../src/app/resolutions.h"

TEST(resolution_list_every_size_keeps_the_280_by_192_picture_unstretched_and_centred) {
    struct Want { int w, h, scale; };
    // the whole-number scale that fits, from the specification's list (21:9, 16:9, 16:10, 4:3)
    const Want want[] = {{2560, 1080, 5}, {3440, 1440, 7}, {1920, 1080, 5}, {2560, 1440, 7}, {3840, 2160, 11}, {1280, 720, 3}, {1600, 900, 4}, {1366, 768, 4},
                         {1680, 1050, 5}, {1920, 1200, 6}, {1024, 768, 3}, {1280, 960, 4}};
    CHECK(app::kResolutionCount == 12);
    for (int i = 0; i < app::kResolutionCount; ++i) {
        const app::Resolution& r = app::kResolutions[i];
        CHECK(r.w == want[i].w && r.h == want[i].h);
        const app::Viewport v = app::computeViewport(r.w, r.h);
        const bool ok = v.scale == want[i].scale && v.w == 280 * v.scale && v.h == 192 * v.scale;   // every Apple pixel an exact block: not stretched
        if (!ok) std::printf("    %dx%d: scale %d (wanted %d)\n", r.w, r.h, v.scale, want[i].scale);
        CHECK(ok);
        CHECK(v.x >= 0 && v.y >= 0 && v.x + v.w <= r.w && v.y + v.h <= r.h);
        CHECK(std::abs(v.x - (r.w - v.x - v.w)) <= 1 && std::abs(v.y - (r.h - v.y - v.h)) <= 1);   // centred, bars on both sides
        CHECK(app::resolutionIndex(r.w, r.h) == i);
    }
    CHECK(app::resolutionIndex(1920, 1081) == -1 && app::resolutionIndex(0, 0) == -1);
    CHECK(app::resolutionLabel(1920, 1080) == "1920X1080 (16:9)" && app::resolutionLabel(3440, 1440) == "3440X1440 (21:9)" && app::resolutionLabel(1680, 1050) == "1680X1050 (16:10)" && app::resolutionLabel(1024, 768) == "1024X768 (4:3)");
    CHECK(app::resolutionLabel(777, 555) == "CUSTOM 777X555");
    // the cycle: only sizes the display can show, round the list in both directions, from a custom size the first / last that fits
    const auto all = [](int) { return true; };
    CHECK(app::nextResolution(0, 1, all) == 1 && app::nextResolution(11, 1, all) == 0 && app::nextResolution(0, -1, all) == 11 && app::nextResolution(5, -1, all) == 4);
    CHECK(app::nextResolution(-1, 1, all) == 0 && app::nextResolution(-1, -1, all) == 11);
    const auto narrow = [](int i) { return app::kResolutions[i].w <= 1366; };   // a display that is 1366 wide: 1280x720, 1366x768, 1024x768, 1280x960
    CHECK(app::nextResolution(-1, 1, narrow) == 5 && app::nextResolution(5, 1, narrow) == 7 && app::nextResolution(7, 1, narrow) == 10 && app::nextResolution(11, 1, narrow) == 5);
    CHECK(app::nextResolution(5, -1, narrow) == 11 && app::nextResolution(-1, -1, narrow) == 11);
    CHECK(app::nextResolution(3, 1, [](int) { return false; }) == -1 && app::nextResolution(-1, 1, [](int) { return false; }) == -1);   // nothing fits: nothing chosen
}

TEST(resolution_in_the_window_each_size_that_fits_is_exact_and_one_that_does_not_is_refused) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    const hires::Page page = busyPage();
    w.setPage(page);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(w.hwnd(), MONITOR_DEFAULTTONEAREST), &mi);
    int fitted = 0, refused = 0;
    for (int i = 0; i < app::kResolutionCount; ++i) {
        const app::Resolution& r = app::kResolutions[i];
        const SIZE before = clientSize(w.hwnd());
        const POINT originBefore = clientOrigin(w.hwnd());
        const bool fits = w.resolutionFits(r.w, r.h);
        const bool done = w.setResolution(r.w, r.h);
        pump(20);
        CHECK(done == fits);
        if (done) {
            ++fitted;
            CHECK(clientSize(w.hwnd()).cx == r.w && clientSize(w.hwnd()).cy == r.h);
            RECT wr{};
            GetWindowRect(w.hwnd(), &wr);
            CHECK(wr.left >= mi.rcWork.left && wr.top >= mi.rcWork.top && wr.right <= mi.rcWork.right && wr.bottom <= mi.rcWork.bottom);   // all of the window on the monitor
            const GameWindow::RenderInfo ri = w.renderInfo();
            CHECK(ri.displayW == r.w && ri.displayH == r.h && ri.gameW == 280 && ri.gameH == 192);   // display resolution and game resolution are different things
            CHECK(ri.viewport.w == 280 * ri.viewport.scale && ri.viewport.h == 192 * ri.viewport.scale);
            if (w.usesDirect3d()) {
                std::vector<std::uint32_t> got;
                int cw = 0, ch = 0;
                CHECK(w.captureFrame(got, cw, ch));
                CHECK(cw == r.w && ch == r.h && differences(got, expected(page, cw, ch)) == 0);
            }
            std::printf("    %s: picture area %d x %d, scale %d\n", app::resolutionLabel(r.w, r.h).c_str(), r.w, r.h, w.renderInfo().viewport.scale);
        } else {
            ++refused;   // the display cannot show it: nothing changed
            CHECK(clientSize(w.hwnd()).cx == before.cx && clientSize(w.hwnd()).cy == before.cy && clientOrigin(w.hwnd()).x == originBefore.x && clientOrigin(w.hwnd()).y == originBefore.y);
            std::printf("    %s: does not fit this monitor (work area %ld x %ld): refused, window unchanged\n", app::resolutionLabel(r.w, r.h).c_str(), mi.rcWork.right - mi.rcWork.left, mi.rcWork.bottom - mi.rcWork.top);
        }
    }
    CHECK(fitted + refused == app::kResolutionCount && fitted >= 2);
    CHECK(!w.setResolution(10, 10) && !w.setResolution(0, 0) && !w.setResolution(-5, 900));   // below a half-size picture / nonsense
    // Borderless Fullscreen: the desktop's own resolution; a listed one is not forced on it
    w.setDisplayMode(DisplayMode::Borderless);
    pump(60);
    const GameWindow::RenderInfo nat = w.renderInfo();
    CHECK(nat.displayW == mi.rcMonitor.right - mi.rcMonitor.left && nat.displayH == mi.rcMonitor.bottom - mi.rcMonitor.top);
    CHECK(!w.setResolution(1280, 720));
    w.cycleResolution(1);
    CHECK(w.renderInfo().displayW == nat.displayW && w.renderInfo().displayH == nat.displayH && w.fullscreen());
    w.setDisplayMode(DisplayMode::Windowed);
}

TEST(resolution_cycling_goes_round_the_sizes_that_fit_and_a_custom_size_is_labelled) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    std::vector<int> fits;
    for (int i = 0; i < app::kResolutionCount; ++i)
        if (w.resolutionFits(app::kResolutions[i].w, app::kResolutions[i].h)) fits.push_back(i);
    CHECK(fits.size() >= 2);
    if (fits.size() < 2) return;
    setClient(w.hwnd(), 777, 555);   // a size dragged by hand
    pump(20);
    const auto current = [&] { return app::resolutionIndex(clientSize(w.hwnd()).cx, clientSize(w.hwnd()).cy); };
    CHECK(current() == -1);
    w.cycleResolution(1);
    pump(20);
    CHECK(current() == fits.front());   // from a custom size: the first that fits
    for (std::size_t k = 1; k < fits.size(); ++k) {
        w.cycleResolution(1);
        pump(20);
        CHECK(current() == fits[k]);
    }
    w.cycleResolution(1);
    pump(20);
    CHECK(current() == fits.front());   // round
    w.cycleResolution(-1);
    pump(20);
    CHECK(current() == fits.back());    // and backwards
    setClient(w.hwnd(), 777, 555);
    pump(20);
    w.cycleResolution(-1);
    pump(20);
    CHECK(current() == fits.back());    // from a custom size backwards: the last that fits
}

TEST(resolution_is_changed_from_the_pause_menu_and_kept_in_the_settings) {
    core::CharacterRom rom;
    const bool haveFont = loadFont(rom);
    const auto dir = scratchDir("resolution");
    SettingsStore store((dir / "settings.ini").wstring());
    Settings start;
    start.displayMode = DisplayMode::Windowed;
    int chosenW = 0, chosenH = 0;
    {
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        if (haveFont) w.setMenuFont(rom);
        w.attachSettings(&store, start);
        const HWND h = w.hwnd();
        SendMessageW(h, WM_KEYDOWN, VK_ESCAPE, 1);
        SendMessageW(h, WM_KEYDOWN, VK_DOWN, 1);
        SendMessageW(h, WM_KEYDOWN, VK_RETURN, 1);   // Graphics Settings
        SendMessageW(h, WM_KEYDOWN, VK_DOWN, 1);     // Resolution
        SendMessageW(h, WM_KEYDOWN, VK_RIGHT, 1);
        pump(40);
        CHECK(w.menuOpen());
        const SIZE s1 = clientSize(h);
        CHECK(app::resolutionIndex(s1.cx, s1.cy) >= 0);
        SendMessageW(h, WM_KEYDOWN, VK_RETURN, 1);   // Enter = next, too
        pump(40);
        const SIZE s2 = clientSize(h);
        CHECK(app::resolutionIndex(s2.cx, s2.cy) >= 0 && (s2.cx != s1.cx || s2.cy != s1.cy));
        SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, clientPos(h, graphicsItem(1).x, graphicsItem(1).y));   // a click: next
        pump(40);
        const SIZE s3 = clientSize(h);
        CHECK(app::resolutionIndex(s3.cx, s3.cy) >= 0 && (s3.cx != s2.cx || s3.cy != s2.cy));
        SendMessageW(h, WM_KEYDOWN, VK_LEFT, 1);     // previous: back to the second
        pump(40);
        CHECK(clientSize(h).cx == s2.cx && clientSize(h).cy == s2.cy);
        chosenW = s2.cx;
        chosenH = s2.cy;
        CHECK(w.menuOpen());   // the game stays paused behind the menu the whole time
        // the picture of the menu at the new size is the picture at that size (the menu is drawn into the game picture)
        if (w.usesDirect3d()) {
            std::vector<std::uint32_t> got;
            int cw = 0, ch = 0;
            CHECK(w.captureFrame(got, cw, ch));
            CHECK(cw == chosenW && ch == chosenH);
        }
        const Settings s = store.load();
        CHECK(s.hasWindowRect && s.windowW == chosenW && s.windowH == chosenH);   // written at once
    }
    {   // the next start comes up at that resolution
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        w.attachSettings(&store, store.load());
        pump(40);
        CHECK(w.displayMode() == DisplayMode::Windowed && clientSize(w.hwnd()).cx == chosenW && clientSize(w.hwnd()).cy == chosenH);
    }
    std::filesystem::remove_all(dir);
}

TEST(resolution_changes_while_the_game_runs_leave_the_game_and_its_clock_untouched) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    constexpr int kTicks = 150000;
    struct Result { std::vector<std::uint8_t> page; unsigned score = 0, level = 0; int changes = 0; };
    const auto run = [&](bool disturb) {
        Result res;
        core::GameDriver drv;
        CHECK(drv.init(pack) && drv.newGame());
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        std::atomic<bool> done{false};
        std::thread worker([&] {
            for (int t = 0; t < kTicks; ++t) {
                drv.tick();
                if (t % 3 == 0) w.setPage(drv.page());
                if (t % 1000 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            done = true;
        });
        while (!done) {
            if (disturb) {
                w.cycleResolution(1);
                ++res.changes;
            }
            pump(5);
        }
        worker.join();
        for (std::size_t i = 0; i < hires::kPageBytes; ++i) res.page.push_back(drv.page().byte(i));
        res.score = drv.core().state().zp(0x52);
        res.level = drv.core().state().level();
        return res;
    };
    const Result plain = run(false);
    const Result disturbed = run(true);
    std::printf("    %d ticks; %d resolution changes while it ran; page equal %d\n", kTicks, disturbed.changes, plain.page == disturbed.page);
    CHECK(disturbed.changes >= 6);
    CHECK(plain.page == disturbed.page && plain.score == disturbed.score && plain.level == disturbed.level);
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    core::CycleClock clock;
    const std::int64_t t0 = core::nowNanoseconds();
    clock.start(t0);
    std::atomic<bool> done{false};
    std::int64_t handed = 0, endNs = 0;
    std::thread worker([&] {
        while (core::nowNanoseconds() - t0 < 800'000'000) {
            handed += clock.cyclesDue(core::nowNanoseconds());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        endNs = core::nowNanoseconds();
        handed += clock.cyclesDue(endNs);
        done = true;
    });
    int changes = 0;
    while (!done) {
        w.cycleResolution(1);
        ++changes;
        pump(15);
    }
    worker.join();
    const std::int64_t exact = clock.cyclesIn(endNs - t0);
    std::printf("    %d resolution changes in 0.8 s; cycles handed out %lld, exact %lld, skipped %lld\n", changes, static_cast<long long>(handed), static_cast<long long>(exact), static_cast<long long>(clock.skipped()));
    CHECK(changes >= 10);
    CHECK(handed == exact && clock.skipped() == 0);
}

TEST(resolution_entry_in_borderless_fullscreen_shows_the_desktop_size_without_a_native_label) {
    core::CharacterRom rom;
    if (!loadFont(rom)) return;
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    w.setMenuFont(rom);
    w.setDisplayMode(DisplayMode::Borderless);
    pump(60);
    const GameWindow::RenderInfo ri = w.renderInfo();
    PauseMenu::View view;
    view.displayMode = DisplayMode::Borderless;
    view.resolution = std::to_string(ri.displayW) + "X" + std::to_string(ri.displayH);   // what the window gives the menu in this mode
    PauseMenu m;
    m.show();
    m.key(PauseMenu::Key::Down);
    m.key(PauseMenu::Key::Select);   // the graphics page
    std::vector<std::uint8_t> shown, withLabel;
    m.render(rom, view, shown);
    view.resolution = "NATIVE " + view.resolution;
    m.render(rom, view, withLabel);
    CHECK(shown != withLabel);   // the label is a visible difference: the window's text is the plain one (checked below through the real code path)
    // the real code path: Esc, Down, Enter, then the frame must equal the menu rendered with the plain text
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_DOWN, 1);
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_RETURN, 1);
    CHECK(w.menuOpen());
    if (w.usesDirect3d()) {
        std::vector<std::uint32_t> got;
        int cw = 0, ch = 0;
        CHECK(w.captureFrame(got, cw, ch));
        const app::Viewport vp = app::computeViewport(cw, ch);
        view.resolution = std::to_string(ri.displayW) + "X" + std::to_string(ri.displayH);
        view.borders = w.borders();
        std::vector<std::uint8_t> want;
        m.render(rom, view, want);
        std::size_t bad = 0;
        for (int y = PauseMenu::kPanelY; y < PauseMenu::kPanelY + PauseMenu::kPanelH; ++y)
            for (int x = PauseMenu::kPanelX; x < PauseMenu::kPanelX + PauseMenu::kPanelW; ++x) {
                const std::uint32_t px = got[static_cast<std::size_t>(vp.y + y * vp.scale + vp.scale / 2) * static_cast<std::size_t>(cw) + static_cast<std::size_t>(vp.x + x * vp.scale + vp.scale / 2)] & 0xFFFFFF;
                bad += (px == 0xFFFFFF) != (want[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] != 0);
            }
        std::printf("    graphics page in Borderless Fullscreen, panel pixels that differ from the plain-text menu: %zu\n", bad);
        CHECK(bad == 0);
    }
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    w.setDisplayMode(DisplayMode::Windowed);
}

// ---- TASK-034-6: Window Borders (title bar and frame of the windowed mode) ----
namespace {

RECT workAreaOf(HWND h) {
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
    return mi.rcWork;
}

bool hasCaption(HWND h) { return (GetWindowLongW(h, GWL_STYLE) & WS_CAPTION) == WS_CAPTION; }

}  // namespace

TEST(window_borders_both_states_are_resizable_and_the_switch_needs_no_restart) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    const HWND h = w.hwnd();
    const hires::Page page = busyPage();
    w.setPage(page);
    setClient(h, 1000, 700);
    pump(30);
    const auto hit = [&](int x, int y) { return static_cast<int>(SendMessageW(h, WM_NCHITTEST, 0, MAKELPARAM(static_cast<short>(x), static_cast<short>(y)))); };
    for (int round = 0; round < 3; ++round) {
        // frame on: the Windows frame, title bar, maximise button; Windows finds the resize edges itself
        CHECK(w.borders() && hasCaption(h));
        CHECK((GetWindowLongW(h, GWL_STYLE) & (WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU)) == (WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU));
        RECT wr{};
        GetWindowRect(h, &wr);
        CHECK(hit(wr.left + 1, (wr.top + wr.bottom) / 2) == HTLEFT && hit(wr.right - 2, wr.bottom - 2) == HTBOTTOMRIGHT && hit(wr.left + 100, wr.top + 10) == HTCAPTION);
        w.setBorders(false);   // no restart: the very same window
        pump(30);
        CHECK(!w.borders() && !hasCaption(h) && (GetWindowLongW(h, GWL_STYLE) & WS_THICKFRAME) != 0 && IsWindowVisible(h) != FALSE);
        GetWindowRect(h, &wr);
        CHECK(hit(wr.left + 1, (wr.top + wr.bottom) / 2) == HTLEFT && hit(wr.right - 2, wr.bottom - 2) == HTBOTTOMRIGHT && hit(wr.right - 2, wr.top + 1) == HTTOPRIGHT);   // the edges still resize it
        CHECK(clientSize(h).cx == 1000 && clientSize(h).cy == 700);
        if (w.usesDirect3d()) {
            std::vector<std::uint32_t> got;
            int cw = 0, ch = 0;
            CHECK(w.captureFrame(got, cw, ch));
            CHECK(cw == 1000 && ch == 700 && differences(got, expected(page, cw, ch)) == 0);
        }
        w.setBorders(true);
        pump(30);
    }
}

TEST(window_borders_a_maximised_window_stays_maximised_when_the_frame_is_switched) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    const HWND h = w.hwnd();
    const hires::Page page = busyPage();
    w.setPage(page);
    setClient(h, 900, 600);
    pump(30);
    ShowWindow(h, SW_MAXIMIZE);
    pump(60);
    CHECK(IsZoomed(h) != FALSE);
    const RECT work = workAreaOf(h);
    w.setBorders(false);
    pump(60);
    CHECK(IsZoomed(h) != FALSE && !hasCaption(h));
    // a maximised window without frame is exactly the work area (no overhanging frame)
    CHECK(clientSize(h).cx == work.right - work.left && clientSize(h).cy == work.bottom - work.top);
    if (w.usesDirect3d()) {
        std::vector<std::uint32_t> got;
        int cw = 0, ch = 0;
        CHECK(w.captureFrame(got, cw, ch));
        CHECK(cw == work.right - work.left && ch == work.bottom - work.top && differences(got, expected(page, cw, ch)) == 0);
    }
    w.setBorders(true);
    pump(60);
    CHECK(IsZoomed(h) != FALSE && hasCaption(h) && clientSize(h).cx <= work.right - work.left && clientSize(h).cy < work.bottom - work.top);
    ShowWindow(h, SW_RESTORE);   // restoring gives the normal window back (not a window the size of the screen)
    pump(60);
    CHECK(!IsZoomed(h) && clientSize(h).cx == 900 && clientSize(h).cy == 600);
}

TEST(window_borders_switching_the_frame_on_never_leaves_the_window_larger_than_the_screen) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    const HWND h = w.hwnd();
    const hires::Page page = busyPage();
    w.setPage(page);
    w.setBorders(false);
    pump(30);
    const RECT work = workAreaOf(h);
    SetWindowPos(h, nullptr, work.left, work.top, work.right - work.left, work.bottom - work.top, SWP_NOZORDER | SWP_NOACTIVATE);   // a bare window as big as the work area
    pump(40);
    CHECK(clientSize(h).cx == work.right - work.left && clientSize(h).cy == work.bottom - work.top);
    w.setBorders(true);
    pump(40);
    RECT wr{};
    GetWindowRect(h, &wr);
    CHECK(hasCaption(h));
    CHECK(wr.left >= work.left && wr.top >= work.top && wr.right <= work.right && wr.bottom <= work.bottom);   // the picture area gave way, the window is all on the monitor
    CHECK(clientSize(h).cx >= 140 && clientSize(h).cy >= 96 && clientSize(h).cx < work.right - work.left);
    if (w.usesDirect3d()) {
        std::vector<std::uint32_t> got;
        int cw = 0, ch = 0;
        CHECK(w.captureFrame(got, cw, ch));
        CHECK(cw == clientSize(h).cx && ch == clientSize(h).cy && differences(got, expected(page, cw, ch)) == 0);
    }
    // a window pushed against the right/bottom edge: the frame does not push it off the screen
    w.setBorders(false);
    pump(30);
    setClient(h, 800, 500);
    SetWindowPos(h, nullptr, work.right - 800, work.bottom - 500, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    pump(30);
    w.setBorders(true);
    pump(30);
    GetWindowRect(h, &wr);
    CHECK(wr.right <= work.right && wr.bottom <= work.bottom && wr.left >= work.left && wr.top >= work.top);
}

TEST(window_borders_apply_to_windowed_mode_only_and_the_menu_says_so) {
    core::CharacterRom rom;
    const bool haveFont = loadFont(rom);
    // the menu's note: only on the Window Borders entry, only while Borderless Fullscreen is active
    if (haveFont) {
        PauseMenu m;
        m.show();
        m.key(PauseMenu::Key::Down);
        m.key(PauseMenu::Key::Select);
        m.key(PauseMenu::Key::Down);
        m.key(PauseMenu::Key::Down);   // Window Borders
        PauseMenu::View windowed, borderless;
        windowed.displayMode = DisplayMode::Windowed;
        borderless.displayMode = DisplayMode::Borderless;
        std::vector<std::uint8_t> a, b, c;
        const auto rowsThatDiffer = [](const std::vector<std::uint8_t>& p, const std::vector<std::uint8_t>& q) {
            std::vector<int> rows;
            for (int y = 0; y < 192; ++y) {
                bool differs = false;
                for (int x = 0; x < 280; ++x) differs |= p[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] != q[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)];
                if (differs) rows.push_back(y);
            }
            return rows;
        };
        m.render(rom, windowed, a);
        m.render(rom, borderless, b);
        // the two views differ in the Display Mode entry (text row 8) and, with Window Borders selected, in the note (text row 17); nowhere else
        int inEntry = 0, inNote = 0, elsewhere = 0;
        for (int y : rowsThatDiffer(a, b)) (y >= 8 * 8 && y < 9 * 8 ? inEntry : y >= 17 * 8 && y < 18 * 8 ? inNote : elsewhere)++;
        CHECK(inEntry > 0 && inNote > 0 && elsewhere == 0);
        m.key(PauseMenu::Key::Up);   // Resolution selected: no note any more
        m.render(rom, windowed, a);
        m.render(rom, borderless, c);
        inEntry = inNote = elsewhere = 0;
        for (int y : rowsThatDiffer(a, c)) (y >= 8 * 8 && y < 9 * 8 ? inEntry : y >= 17 * 8 && y < 18 * 8 ? inNote : elsewhere)++;
        CHECK(inEntry > 0 && inNote == 0 && elsewhere == 0);
    }
    // in Borderless Fullscreen the entry changes the setting, the window does not change, and the frame is there when the window comes back
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok()) return;
    setClient(w.hwnd(), 1000, 700);
    pump(30);
    w.setDisplayMode(DisplayMode::Borderless);
    pump(60);
    const SIZE full = clientSize(w.hwnd());
    w.setBorders(false);
    pump(30);
    CHECK(!w.borders() && w.fullscreen() && clientSize(w.hwnd()).cx == full.cx && clientSize(w.hwnd()).cy == full.cy);
    w.setDisplayMode(DisplayMode::Windowed);
    pump(60);
    CHECK(!hasCaption(w.hwnd()) && clientSize(w.hwnd()).cx == 1000 && clientSize(w.hwnd()).cy == 700);
    w.setBorders(true);
    pump(30);
    CHECK(hasCaption(w.hwnd()) && clientSize(w.hwnd()).cx == 1000 && clientSize(w.hwnd()).cy == 700);
}

TEST(window_borders_switching_changes_nothing_in_the_game_or_its_clock) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    constexpr int kTicks = 150000;
    struct Result { std::vector<std::uint8_t> page; unsigned score = 0, level = 0; int changes = 0; };
    const auto run = [&](bool disturb) {
        Result res;
        core::GameDriver drv;
        CHECK(drv.init(pack) && drv.newGame());
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        std::atomic<bool> done{false};
        std::thread worker([&] {
            for (int t = 0; t < kTicks; ++t) {
                drv.tick();
                if (t % 3 == 0) w.setPage(drv.page());
                if (t % 1000 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            done = true;
        });
        while (!done) {
            if (disturb) {
                w.setBorders(!w.borders());
                ++res.changes;
            }
            pump(5);
        }
        worker.join();
        for (std::size_t i = 0; i < hires::kPageBytes; ++i) res.page.push_back(drv.page().byte(i));
        res.score = drv.core().state().zp(0x52);
        res.level = drv.core().state().level();
        return res;
    };
    const Result plain = run(false);
    const Result disturbed = run(true);
    std::printf("    %d ticks; %d frame switches while it ran; page equal %d\n", kTicks, disturbed.changes, plain.page == disturbed.page);
    CHECK(disturbed.changes >= 6);
    CHECK(plain.page == disturbed.page && plain.score == disturbed.score && plain.level == disturbed.level);
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    core::CycleClock clock;
    const std::int64_t t0 = core::nowNanoseconds();
    clock.start(t0);
    std::atomic<bool> done{false};
    std::int64_t handed = 0, endNs = 0;
    std::thread worker([&] {
        while (core::nowNanoseconds() - t0 < 800'000'000) {
            handed += clock.cyclesDue(core::nowNanoseconds());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        endNs = core::nowNanoseconds();
        handed += clock.cyclesDue(endNs);
        done = true;
    });
    int changes = 0;
    while (!done) {
        w.setBorders(!w.borders());
        ++changes;
        pump(15);
    }
    worker.join();
    const std::int64_t exact = clock.cyclesIn(endNs - t0);
    std::printf("    %d frame switches in 0.8 s; cycles handed out %lld, exact %lld, skipped %lld\n", changes, static_cast<long long>(handed), static_cast<long long>(exact), static_cast<long long>(clock.skipped()));
    CHECK(changes >= 10);
    CHECK(handed == exact && clock.skipped() == 0);
}

TEST(window_borders_setting_is_saved_at_once_and_restored_with_the_window) {
    const auto dir = scratchDir("borders");
    SettingsStore store((dir / "settings.ini").wstring());
    Settings start;
    start.displayMode = DisplayMode::Windowed;
    {
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        w.attachSettings(&store, start);
        setClient(w.hwnd(), 1000, 700);
        pump(30);
        w.setBorders(false);
        CHECK(!store.load().windowBorders);   // written at the change, not only at the exit
        w.setBorders(true);
        CHECK(store.load().windowBorders);
        w.setBorders(false);
    }
    const Settings kept = store.load();
    CHECK(!kept.windowBorders && kept.hasWindowRect && kept.windowW == 1000 && kept.windowH == 700);
    {
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        w.attachSettings(&store, kept);
        pump(40);
        CHECK(!w.borders() && !hasCaption(w.hwnd()) && IsWindowVisible(w.hwnd()) != FALSE);   // the frame is off from the first moment
        CHECK(clientSize(w.hwnd()).cx == 1000 && clientSize(w.hwnd()).cy == 700);
        w.handleHotkey(VK_F8, false, false);   // F8 still switches it
        pump(30);
        CHECK(w.borders() && hasCaption(w.hwnd()) && store.load().windowBorders);
    }
    std::filesystem::remove_all(dir);
}

// ---- TASK-034-7: Background (Black, Ambient Glow) ----
#include "../src/app/background.h"
#include "../src/app/glow.h"

namespace {

std::vector<std::uint32_t> solidPicture(std::uint32_t argb) {   // on the heap: 215 KB each
    return std::vector<std::uint32_t>(280 * 192, argb);
}

int peakOf(std::uint32_t argb) { return static_cast<int>(std::max({(argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF})); }
int redOf(std::uint32_t argb) { return static_cast<int>((argb >> 16) & 0xFF); }
int greenOf(std::uint32_t argb) { return static_cast<int>((argb >> 8) & 0xFF); }
int blueOf(std::uint32_t argb) { return static_cast<int>(argb & 0xFF); }

std::uint32_t at(const std::vector<std::uint32_t>& frame, int cw, int x, int y) { return frame[static_cast<std::size_t>(y) * static_cast<std::size_t>(cw) + static_cast<std::size_t>(x)] & 0xFFFFFF; }

// the picture area of a captured frame equals the expected picture (the background must never reach into it)
std::size_t insideDiffers(const std::vector<std::uint32_t>& got, const hires::Page& page, int cw, int ch) {
    const std::vector<unsigned> want = expected(page, cw, ch);
    const app::Viewport vp = app::computeViewport(cw, ch);
    std::size_t bad = 0;
    for (int y = vp.y; y < vp.y + vp.h; ++y)
        for (int x = vp.x; x < vp.x + vp.w; ++x) bad += at(got, cw, x, y) != want[static_cast<std::size_t>(y) * static_cast<std::size_t>(cw) + static_cast<std::size_t>(x)];
    return bad;
}

hires::Page allWhitePage() {
    hires::Page p;
    p.clear(0x7F);   // every pixel lit: a white picture
    return p;
}

}  // namespace

TEST(background_names_cycle_and_the_settings_file_keeps_the_choice) {
    CHECK(app::kBackgroundCount == 2);
    CHECK(std::string(app::backgroundKey(app::Background::Black)) == "black" && std::string(app::backgroundKey(app::Background::AmbientGlow)) == "glow");
    CHECK(std::string(app::backgroundLabel(app::Background::Black)) == "BLACK" && std::string(app::backgroundLabel(app::Background::AmbientGlow)) == "AMBIENT GLOW");
    CHECK(app::nextBackground(app::Background::Black, 1) == app::Background::AmbientGlow && app::nextBackground(app::Background::AmbientGlow, 1) == app::Background::Black);
    CHECK(app::nextBackground(app::Background::Black, -1) == app::Background::AmbientGlow && app::nextBackground(app::Background::AmbientGlow, -1) == app::Background::Black);
    app::Background b = app::Background::Black;
    CHECK(app::parseBackground("glow", b) && b == app::Background::AmbientGlow);
    CHECK(!app::parseBackground("rainbow", b) && b == app::Background::AmbientGlow);   // unchanged on a bad word
    Settings s;
    CHECK(s.background == app::Background::Black);   // the default
    s.background = app::Background::AmbientGlow;
    CHECK(SettingsStore::parse(SettingsStore::format(s)).background == app::Background::AmbientGlow);
    CHECK(SettingsStore::parse("background=GLOW\n").background == app::Background::AmbientGlow);
    CHECK(SettingsStore::parse("background=sideways\n").background == app::Background::Black);   // a bad word: the default
}

// The picture area of a captured frame equals this 280 x 192 image drawn at the frame's scale.
static std::size_t insideDiffersFromImage(const std::vector<std::uint32_t>& got, const std::vector<std::uint32_t>& image, int cw, int ch) {
    const app::Viewport vp = app::computeViewport(cw, ch);
    std::size_t bad = 0;
    for (int y = 0; y < vp.h; ++y)
        for (int x = 0; x < vp.w; ++x) bad += at(got, cw, vp.x + x, vp.y + y) != (image[static_cast<std::size_t>(y / vp.scale) * 280 + static_cast<std::size_t>(x / vp.scale)] & 0xFFFFFF);
    return bad;
}

TEST(glow_field_backdrop_is_the_blurred_picture_black_stays_black_hue_and_mass_are_kept) {
    app::GlowField g;
    float r, gr, b;
    const auto black = solidPicture(0xFF000000);
    g.update(black.data(), 1.0, 4);
    for (int y = 0; y < 192; y += 17)
        for (int x = 0; x < 280; x += 13) {
            g.backdrop(x, y, r, gr, b);
            CHECK(r == 0.0f && gr == 0.0f && b == 0.0f);   // nothing in the picture, nothing behind it
        }
    CHECK(g.backdropBytes()[3] == 255);
    // a solid colour stays that colour whatever the blur (the blur extends the picture's own edges)
    const auto green = solidPicture(0xFF00FF00);
    g.reset();
    g.update(green.data(), 0.0, 9);
    g.backdrop(0, 0, r, gr, b);
    CHECK(std::abs(r) < 1e-4f && std::abs(gr - 1.0f) < 1e-4f && std::abs(b) < 1e-4f);
    g.backdrop(140, 96, r, gr, b);
    CHECK(std::abs(r) < 1e-4f && std::abs(gr - 1.0f) < 1e-4f && std::abs(b) < 1e-4f);
    // a white patch: without blur it is exactly the patch; with blur it spreads, gets fainter, and keeps its total brightness
    auto patch = solidPicture(0xFF000000);
    for (int y = 90; y < 102; ++y)
        for (int x = 134; x < 146; ++x) patch[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] = 0xFFFFFFFF;
    g.reset();
    g.update(patch.data(), 0.0, 0);
    g.backdrop(140, 96, r, gr, b);
    CHECK(r == 1.0f);
    g.backdrop(140, 110, r, gr, b);
    CHECK(r == 0.0f);   // 8 pixels below the patch: nothing without blur
    double sharp = 0;
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 280; ++x) {
            g.backdrop(x, y, r, gr, b);
            sharp += r;
        }
    g.reset();
    g.update(patch.data(), 0.0, 6);
    double soft = 0;
    float centre = 0;
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 280; ++x) {
            g.backdrop(x, y, r, gr, b);
            soft += r;
            if (x == 140 && y == 96) centre = r;
        }
    g.backdrop(140, 110, r, gr, b);
    std::printf("    a 12 x 12 patch, blur radius 6: centre %.2f, 14 px below it %.3f, total brightness %.1f -> %.1f\n", centre, r, sharp, soft);
    CHECK(centre < 0.6f && centre > 0.05f && r > 0.001f);   // fainter, and spread beyond the patch
    CHECK(std::abs(soft - sharp) < 0.01 * sharp);           // nothing lost or gained
    // the first picture is taken as it is; reset starts again
    g.reset();
    g.update(green.data(), 0.0, 0);
    g.backdrop(5, 5, r, gr, b);
    CHECK(gr == 1.0f);
}

TEST(glow_field_follows_the_game_within_a_few_frames_so_movement_shows) {
    app::GlowField g;
    float r, gr, b;
    const auto white = solidPicture(0xFFFFFFFF), black = solidPicture(0xFF000000);
    g.update(black.data(), 0.0, 0);
    for (int frame = 0; frame < 3; ++frame) g.update(white.data(), 1.0 / 60.0, 0);   // three frames at 60 per second
    g.backdrop(100, 100, r, gr, b);
    std::printf("    black -> white: after 3 frames the backdrop is at %.2f\n", r);
    CHECK(r > 0.5f && r < 0.65f);
    for (int frame = 0; frame < 15; ++frame) g.update(white.data(), 1.0 / 60.0, 0);
    g.backdrop(100, 100, r, gr, b);
    CHECK(r > 0.97f);                                                            // a quarter of a second: all the way
    g.update(black.data(), 0.06, 0);                                             // one time constant back down: 37 %
    g.backdrop(100, 100, r, gr, b);
    CHECK(r > 0.34f && r < 0.40f);
    g.update(black.data(), 0.0, 0);                                              // no time, no change
    float r2;
    g.backdrop(100, 100, r2, gr, b);
    CHECK(r2 == r);
    // a block that moves from the left to the right: the backdrop follows it
    const auto blockAt = [](int x0) {
        auto pic = solidPicture(0xFF000000);
        for (int y = 82; y < 110; ++y)
            for (int x = x0; x < x0 + 28; ++x) pic[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] = 0xFFFFFFFF;
        return pic;
    };
    app::GlowField m;
    m.update(blockAt(30).data(), 0.0, 3);
    for (int frame = 0; frame < 6; ++frame) m.update(blockAt(220).data(), 1.0 / 60.0, 3);   // jumps to the right and stays: 6 frames
    float left, right;
    m.backdrop(44, 96, left, gr, b);
    m.backdrop(234, 96, right, gr, b);
    std::printf("    a block that jumped from the left to the right: after 6 frames left %.2f, right %.2f\n", left, right);
    CHECK(right > 0.7f && left < 0.3f);
}

TEST(glow_field_edge_colours_follow_the_picture_at_once_are_smoothed_along_the_edge_and_cover_all_four_sides) {
    app::GlowField g;
    const auto bgra = [&](int row, int i, int& r, int& gr, int& b) {
        const std::uint8_t* px = &g.edgeBytes()[(static_cast<std::size_t>(row) * app::kEdgeW + static_cast<std::size_t>(i)) * 4];
        b = px[0];
        gr = px[1];
        r = px[2];
    };
    int r, gr, b;
    // a solid colour: every edge texel is that colour (top, bottom: 280; left, right: 192)
    const auto red = solidPicture(0xFFFF0000);
    g.update(red.data(), 1.0, 3);
    g.updateEdges(red.data());
    for (int row = 0; row < 4; ++row)
        for (int i = 0; i < (row < 2 ? 280 : 192); i += 13) {
            bgra(row, i, r, gr, b);
            CHECK(r == 255 && gr == 0 && b == 0);
        }
    // the edges follow the picture immediately (the backdrop is slower): black frame after 1 ms
    const auto black = solidPicture(0xFF000000);
    g.update(black.data(), 0.001, 3);
    g.updateEdges(black.data());
    bgra(0, 100, r, gr, b);
    CHECK(r == 0 && gr == 0 && b == 0);
    // four different sides: top red, bottom green, left blue, right white (the outer three pixels)
    auto sides = solidPicture(0xFF000000);
    for (int d = 0; d < 3; ++d)
        for (int x = 0; x < 280; ++x) {
            sides[static_cast<std::size_t>(d) * 280 + static_cast<std::size_t>(x)] = 0xFFFF0000;
            sides[static_cast<std::size_t>(191 - d) * 280 + static_cast<std::size_t>(x)] = 0xFF00FF00;
        }
    for (int d = 0; d < 3; ++d)
        for (int y = 20; y < 172; ++y) {
            sides[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(d)] = 0xFF0000FF;
            sides[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(279 - d)] = 0xFFFFFFFF;
        }
    g.updateEdges(sides.data());
    bgra(0, 140, r, gr, b);
    CHECK(r == 255 && gr == 0 && b == 0);
    bgra(1, 140, r, gr, b);
    CHECK(r == 0 && gr == 255 && b == 0);
    bgra(2, 96, r, gr, b);
    CHECK(r == 0 && gr == 0 && b == 255);
    bgra(3, 96, r, gr, b);
    CHECK(r == 255 && gr == 255 && b == 255);
    // one bright pixel is spread over the 11 texels around it and weakened to a thirty-third; nothing beyond them
    auto speck = solidPicture(0xFF000000);
    speck[static_cast<std::size_t>(0) * 280 + 140] = 0xFFFFFFFF;   // top edge, x = 140
    g.updateEdges(speck.data());
    int lit = 0, peakValue = 0;
    for (int i = 0; i < 280; ++i) {
        bgra(0, i, r, gr, b);
        if (r) {
            ++lit;
            peakValue = std::max(peakValue, r);
            CHECK(i >= 135 && i <= 145);
        }
    }
    CHECK(lit == 11 && peakValue >= 6 && peakValue <= 9);   // 255 / 33 = 7.7
}

TEST(ambient_glow_in_the_window_is_seamless_at_the_edge_shows_shapes_and_movement_and_stays_faint) {
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok() || !w.usesDirect3d()) return;
    const hires::Page white = allWhitePage();
    w.setPage(white);
    setClient(w.hwnd(), 1920, 1080);   // a picture 1400 x 960 at (260, 60): bars on every side
    pump(40);
    std::vector<std::uint32_t> got;
    int cw = 0, ch = 0;
    // Black: the bars are exactly black
    w.setBackground(app::Background::Black);
    CHECK(w.captureFrame(got, cw, ch) && cw == 1920 && ch == 1080);
    const app::Viewport vp = app::computeViewport(cw, ch);
    std::size_t lit = 0;
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x)
            if (!(x >= vp.x && x < vp.x + vp.w && y >= vp.y && y < vp.y + vp.h)) lit += at(got, cw, x, y) != 0;
    CHECK(lit == 0 && insideDiffers(got, white, cw, ch) == 0);
    // Ambient Glow next to a picture that is white right to its edge: no step, then a faint backdrop that only ever fades
    w.setBackground(app::Background::AmbientGlow);
    CHECK(w.captureFrame(got, cw, ch));
    CHECK(insideDiffers(got, white, cw, ch) == 0);   // no bloom into the game graphics
    const int midY = vp.y + vp.h / 2;
    const int next = peakOf(at(got, cw, vp.x - 1, midY)), inside = peakOf(at(got, cw, vp.x, midY));
    const int d30 = peakOf(at(got, cw, vp.x - 30, midY)), d130 = peakOf(at(got, cw, vp.x - 130, midY)), screenEdge = peakOf(at(got, cw, 0, midY)), corner = peakOf(at(got, cw, 0, 0));
    std::printf("    white-edged picture: picture edge %d, bar beside it %d, 30 px out %d, 130 px out %d, screen edge %d, far corner %d (of 255)\n", inside, next, d30, d130, screenEdge, corner);
    CHECK(inside == 255 && next >= 245);   // seamless: the bar next to the picture is the picture's own colour
    CHECK(d30 < next && d130 < d30 && screenEdge <= d130);
    CHECK(d130 >= 18 && d130 <= 45);       // faint, far in the background (at most 15 % of a colour)
    CHECK(screenEdge >= 12 && screenEdge <= 38 && corner <= 38);
    int worstRise = 0;
    for (int x = vp.x - 1; x > 0; --x) worstRise = std::max(worstRise, peakOf(at(got, cw, x - 1, midY)) - peakOf(at(got, cw, x, midY)));
    CHECK(worstRise <= 1);   // it only ever fades outwards (a level of noise hides the steps)
    // all round the picture: no step on any side or corner (solid green picture, so the edge colour is green everywhere)
    std::vector<std::uint32_t> green(280 * 192, 0xFF00FF00u), black(280 * 192, 0xFF000000u);
    w.setImage(green);
    CHECK(w.captureFrame(got, cw, ch));
    int seam = 255;
    for (int x = vp.x - 1; x <= vp.x + vp.w; ++x) {
        seam = std::min(seam, greenOf(at(got, cw, x, vp.y - 1)));
        seam = std::min(seam, greenOf(at(got, cw, x, vp.y + vp.h)));
    }
    for (int y = vp.y - 1; y <= vp.y + vp.h; ++y) {
        seam = std::min(seam, greenOf(at(got, cw, vp.x - 1, y)));
        seam = std::min(seam, greenOf(at(got, cw, vp.x + vp.w, y)));
    }
    std::printf("    green picture: the weakest green of the ring of bar pixels touching the picture: %d (of 255)\n", seam);
    CHECK(seam >= 240);
    CHECK(redOf(at(got, cw, vp.x - 1, midY)) <= 4 && blueOf(at(got, cw, vp.x - 1, midY)) <= 4);   // and the hue is the picture's
    // a black-edged picture (the game's own look: a black border, the maze inside): the bars start black at its edge and rise into a soft halo that then fades
    std::vector<std::uint32_t> framed(280 * 192, 0xFF000000u);
    for (int y = 12; y < 180; ++y)
        for (int x = 12; x < 268; ++x) framed[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] = 0xFFFFFFFFu;
    w.setImage(framed);
    CHECK(w.captureFrame(got, cw, ch));
    CHECK(insideDiffersFromImage(got, framed, cw, ch) == 0);
    int values[240];
    int peak = 0, peakAt = 0;
    for (int d = 1; d <= 240; ++d) {
        values[d - 1] = peakOf(at(got, cw, vp.x - d, midY));
        if (values[d - 1] > peak) {
            peak = values[d - 1];
            peakAt = d;
        }
    }
    std::printf("    black-edged picture: bar beside it %d, halo peak %d at %d px out, 240 px out %d\n", values[0], peak, peakAt, values[239]);
    CHECK(values[0] <= 4);                                         // no step against the black border
    CHECK(peak >= values[0] + 8 && peakAt >= 15 && peakAt <= 150);   // a soft halo a little way out ...
    CHECK(values[239] < peak && peak <= 40);                       // ... that then fades, faint
    // a single bright pixel on the picture's edge does not streak outwards
    std::vector<std::uint32_t> speck(280 * 192, 0xFF000000u);
    speck[static_cast<std::size_t>(96) * 280] = 0xFFFFFFFFu;
    w.setImage(speck);
    CHECK(w.captureFrame(got, cw, ch));
    const int speckMid = (96 * vp.scale) + vp.scale / 2;
    int streak = 0;
    for (int d = 1; d <= 60; ++d) streak = std::max(streak, peakOf(at(got, cw, vp.x - d, vp.y + speckMid)));
    std::printf("    one white pixel on the picture's edge: the strongest bar pixel in line with it: %d (of 255)\n", streak);
    CHECK(streak <= 12);
    // a picture with different colours at its two sides: each side takes its own
    std::vector<std::uint32_t> halves(280 * 192, 0xFF0000FFu);
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 140; ++x) halves[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] = 0xFFFF0000u;
    w.setImage(halves);
    CHECK(w.captureFrame(got, cw, ch));
    const std::uint32_t left = at(got, cw, vp.x - 1, midY), right = at(got, cw, vp.x + vp.w, midY);
    CHECK(redOf(left) >= 240 && blueOf(left) <= 4 && blueOf(right) >= 240 && redOf(right) <= 4);
    // no content, no glow
    w.setImage(black);
    CHECK(w.captureFrame(got, cw, ch));
    int brightest = 0;
    for (int x = 0; x < vp.x; x += 7) brightest = std::max(brightest, peakOf(at(got, cw, x, midY)));
    CHECK(brightest <= 1);
    // SHAPES: two white bands (full width) with black between them: the bars show both bands as soft shapes with a dark gap between them
    std::vector<std::uint32_t> bands(280 * 192, 0xFF000000u);
    for (int y = 30; y < 60; ++y)
        for (int x = 0; x < 280; ++x) bands[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] = 0xFFFFFFFFu;
    for (int y = 130; y < 160; ++y)
        for (int x = 0; x < 280; ++x) bands[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] = 0xFFFFFFFFu;
    w.setImage(bands);
    CHECK(w.captureFrame(got, cw, ch));
    const double zoom = app::GlowField::kBackdropScale * vp.scale;   // the backdrop: the picture repeated outwards at 0.8 of its own scale
    const auto screenY = [&](int gameY) { return static_cast<int>(vp.y + vp.h / 2 + (gameY - 96) * zoom); };
    const int colX = vp.x - 120;
    const int upper = peakOf(at(got, cw, colX, screenY(45))), lower = peakOf(at(got, cw, colX, screenY(145))), gap = peakOf(at(got, cw, colX, screenY(96)));
    std::printf("    two white bands behind the picture: upper %d, lower %d, the gap between them %d (of 255)\n", upper, lower, gap);
    CHECK(upper >= 20 && lower >= 20 && gap * 3 <= std::min(upper, lower));   // both shapes show, the gap is dark
    // MOVEMENT: a block on the left of the picture, then the same block on the right: the left bar, then the right bar, lights up
    const auto blockPicture = [](int x0) {
        std::vector<std::uint32_t> pic(280 * 192, 0xFF000000u);
        for (int y = 82; y < 110; ++y)
            for (int x = x0; x < x0 + 28; ++x) pic[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] = 0xFFFFFFFFu;
        return pic;
    };
    const auto barLight = [&](bool leftSide) {
        long sum = 0;
        for (int y = midY - 100; y < midY + 100; y += 5)
            for (int x = leftSide ? 20 : vp.x + vp.w + 20; x < (leftSide ? vp.x - 20 : cw - 20); x += 5) sum += peakOf(at(got, cw, x, y));
        return sum;
    };
    w.setImage(blockPicture(30));
    CHECK(w.captureFrame(got, cw, ch));
    const long leftWhenLeft = barLight(true), rightWhenLeft = barLight(false);
    w.setImage(blockPicture(220));
    CHECK(w.captureFrame(got, cw, ch));
    const long leftWhenRight = barLight(true), rightWhenRight = barLight(false);
    std::printf("    a block on the left of the picture: left bar %ld, right bar %ld; on the right: left bar %ld, right bar %ld\n", leftWhenLeft, rightWhenLeft, leftWhenRight, rightWhenRight);
    CHECK(leftWhenLeft > 3 * rightWhenLeft && rightWhenRight > 3 * leftWhenRight && leftWhenLeft > 500 && rightWhenRight > 500);
    // the picture area is still the game's whatever the window shape
    w.setPage(white);
    for (const SIZE& s : {SIZE{1000, 700}, SIZE{1280, 720}, SIZE{2560 / 2, 1100}, SIZE{900, 900}}) {
        setClient(w.hwnd(), s.cx, s.cy);
        pump(20);
        CHECK(w.captureFrame(got, cw, ch) && cw == s.cx && ch == s.cy);
        CHECK(insideDiffers(got, white, cw, ch) == 0);
    }
}

TEST(ambient_glow_is_not_changed_by_the_pause_menu_and_follows_the_game_not_the_menu) {
    core::CharacterRom rom;
    const bool haveFont = loadFont(rom);
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    if (!w.ok() || !w.usesDirect3d()) return;
    if (haveFont) w.setMenuFont(rom);
    w.setPage(allWhitePage());
    setClient(w.hwnd(), 1920, 1080);
    pump(40);
    w.setBackground(app::Background::AmbientGlow);
    std::vector<std::uint32_t> closed, open;
    int cw = 0, ch = 0;
    CHECK(w.captureFrame(closed, cw, ch));
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK(w.menuOpen());
    CHECK(w.captureFrame(open, cw, ch));
    const app::Viewport vp = app::computeViewport(cw, ch);
    std::size_t differing = 0;
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x)
            if (!(x >= vp.x && x < vp.x + vp.w && y >= vp.y && y < vp.y + vp.h)) differing += at(closed, cw, x, y) != at(open, cw, x, y);
    CHECK(differing == 0);   // the dimmed picture and the menu do not change the bars
    SendMessageW(w.hwnd(), WM_KEYDOWN, VK_ESCAPE, 1);
}

TEST(background_is_changed_from_the_pause_menu_kept_in_the_settings_and_restored) {
    core::CharacterRom rom;
    const bool haveFont = loadFont(rom);
    const auto dir = scratchDir("background");
    SettingsStore store((dir / "settings.ini").wstring());
    Settings start;
    start.displayMode = DisplayMode::Windowed;
    {
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        if (haveFont) w.setMenuFont(rom);
        w.attachSettings(&store, start);
        CHECK(w.background() == app::Background::Black);
        const HWND h = w.hwnd();
        SendMessageW(h, WM_KEYDOWN, VK_ESCAPE, 1);
        SendMessageW(h, WM_KEYDOWN, VK_DOWN, 1);
        SendMessageW(h, WM_KEYDOWN, VK_RETURN, 1);   // Graphics Settings
        for (int i = 0; i < 3; ++i) SendMessageW(h, WM_KEYDOWN, VK_DOWN, 1);   // Background
        SendMessageW(h, WM_KEYDOWN, VK_RIGHT, 1);
        CHECK(w.background() == app::Background::AmbientGlow && w.menuOpen());
        CHECK(store.load().background == app::Background::AmbientGlow);   // written at once
        SendMessageW(h, WM_KEYDOWN, VK_LEFT, 1);
        CHECK(w.background() == app::Background::Black && store.load().background == app::Background::Black);
        SendMessageW(h, WM_KEYDOWN, VK_RETURN, 1);   // Enter = next
        CHECK(w.background() == app::Background::AmbientGlow);
        SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, clientPos(h, graphicsItem(3).x, graphicsItem(3).y));   // a click: next (round to black)
        CHECK(w.background() == app::Background::Black);
        SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, clientPos(h, graphicsItem(3).x, graphicsItem(3).y));
        CHECK(w.background() == app::Background::AmbientGlow);
    }
    CHECK(store.load().background == app::Background::AmbientGlow);
    {
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        if (!w.ok()) return;
        w.attachSettings(&store, store.load());
        CHECK(w.background() == app::Background::AmbientGlow);   // the next start
    }
    std::filesystem::remove_all(dir);
}

TEST(background_menu_entry_shows_the_choice) {
    core::CharacterRom rom;
    if (!loadFont(rom)) return;
    PauseMenu m;
    m.show();
    m.key(PauseMenu::Key::Down);
    m.key(PauseMenu::Key::Select);
    PauseMenu::View black, glow;
    glow.background = app::Background::AmbientGlow;
    std::vector<std::uint8_t> a, b;
    m.render(rom, black, a);
    m.render(rom, glow, b);
    int rowsDiffering = 0, elsewhere = 0;
    for (int y = 0; y < 192; ++y) {
        bool differs = false;
        for (int x = 0; x < 280; ++x) differs |= a[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] != b[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)];
        if (differs) {
            ++rowsDiffering;
            if (y < 11 * 8 || y >= 12 * 8) ++elsewhere;   // the Background entry is text row 11
        }
    }
    CHECK(rowsDiffering > 0 && elsewhere == 0);
}

TEST(background_changes_while_the_game_runs_leave_the_game_and_its_clock_untouched) {
    if (testfw::config().image.empty()) {
        std::printf("    SKIPPED (needs --image)\n");
        return;
    }
    datapack::Pack pack;
    CHECK(flowtest::loadPack(pack));
    constexpr int kTicks = 150000;
    struct Result { std::vector<std::uint8_t> page; unsigned score = 0, level = 0; int changes = 0; };
    const auto run = [&](bool disturb) {
        Result res;
        core::GameDriver drv;
        CHECK(drv.init(pack) && drv.newGame());
        GameWindow w(GameWindowOptions{3, true});
        CHECK(w.ok());
        w.setBackground(app::Background::AmbientGlow);
        std::atomic<bool> done{false};
        std::thread worker([&] {
            for (int t = 0; t < kTicks; ++t) {
                drv.tick();
                if (t % 3 == 0) w.setPage(drv.page());
                if (t % 1000 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            done = true;
        });
        while (!done) {
            if (disturb) {
                w.cycleBackground(1);
                ++res.changes;
            }
            pump(5);
        }
        worker.join();
        for (std::size_t i = 0; i < hires::kPageBytes; ++i) res.page.push_back(drv.page().byte(i));
        res.score = drv.core().state().zp(0x52);
        res.level = drv.core().state().level();
        return res;
    };
    const Result plain = run(false);
    const Result disturbed = run(true);
    std::printf("    %d ticks, glow drawn all the time; %d background changes while it ran; page equal %d\n", kTicks, disturbed.changes, plain.page == disturbed.page);
    CHECK(disturbed.changes >= 6);
    CHECK(plain.page == disturbed.page && plain.score == disturbed.score && plain.level == disturbed.level);
    GameWindow w(GameWindowOptions{3, true});
    CHECK(w.ok());
    w.setBackground(app::Background::AmbientGlow);
    core::CycleClock clock;
    const std::int64_t t0 = core::nowNanoseconds();
    clock.start(t0);
    std::atomic<bool> done{false};
    std::int64_t handed = 0, endNs = 0;
    std::thread worker([&] {
        while (core::nowNanoseconds() - t0 < 800'000'000) {
            handed += clock.cyclesDue(core::nowNanoseconds());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        endNs = core::nowNanoseconds();
        handed += clock.cyclesDue(endNs);
        done = true;
    });
    int changes = 0;
    while (!done) {
        w.cycleBackground(1);
        ++changes;
        pump(15);
    }
    worker.join();
    const std::int64_t exact = clock.cyclesIn(endNs - t0);
    std::printf("    %d background changes in 0.8 s; cycles handed out %lld, exact %lld, skipped %lld\n", changes, static_cast<long long>(handed), static_cast<long long>(exact), static_cast<long long>(clock.skipped()));
    CHECK(changes >= 10);
    CHECK(handed == exact && clock.skipped() == 0);
}
