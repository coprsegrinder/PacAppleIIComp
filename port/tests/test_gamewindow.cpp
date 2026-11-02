// Tests of the game window (src/app/gamewindow): size and scale, full screen, hotkeys, key codes, page copy and repaint, a second thread feeding pages.
// They open real (mostly hidden) windows on the test machine's desktop and drive them with scripted messages; nothing is read from the keyboard.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
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
