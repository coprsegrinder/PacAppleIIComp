#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "pagewindow.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../video/colour.h"

namespace pacman::app {
namespace {

constexpr int kW = 280;
constexpr int kH = 192;
constexpr UINT_PTR kShotTimer = 1;
constexpr UINT_PTR kCloseTimer = 2;

struct State {
    std::array<std::uint32_t, kW * kH> pixels{};
    int scale = 3;
    std::wstring shot;
    int result = 0;
};

bool writeBmp(const std::wstring& path, int w, int h, const std::vector<std::uint32_t>& topDown) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) return false;
    const std::uint32_t imageBytes = static_cast<std::uint32_t>(w) * static_cast<std::uint32_t>(h) * 4u;
    BITMAPFILEHEADER fh{};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fh.bfSize = fh.bfOffBits + imageBytes;
    BITMAPINFOHEADER ih{};
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = -h;  // top-down
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = imageBytes;
    const bool ok = fwrite(&fh, sizeof fh, 1, f) == 1 && fwrite(&ih, sizeof ih, 1, f) == 1 &&
                    fwrite(topDown.data(), 4, topDown.size(), f) == topDown.size();
    fclose(f);
    return ok;
}

// Captures only this window's client area through PrintWindow (never the desktop).
bool captureClient(HWND hwnd, const std::wstring& path) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return false;
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
    bool ok = false;
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(mem, bmp);
        ok = PrintWindow(hwnd, mem, PW_CLIENTONLY | PW_RENDERFULLCONTENT) != FALSE;
        SelectObject(mem, old);
        if (ok) {
            const auto* p = static_cast<const std::uint32_t*>(bits);
            std::vector<std::uint32_t> copy(p, p + static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
            for (auto& v : copy) v |= 0xFF000000u;
            ok = writeBmp(path, w, h, copy);
        }
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(hwnd, wdc);
    return ok;
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (st) {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            BITMAPINFO bi{};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = kW;
            bi.bmiHeader.biHeight = -kH;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            SetStretchBltMode(dc, COLORONCOLOR);  // no smoothing: exact pixel replication at integer scales
            StretchDIBits(dc, 0, 0, kW * st->scale, kH * st->scale, 0, 0, kW, kH, st->pixels.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
            HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            RECT right{kW * st->scale, 0, rc.right, rc.bottom};
            RECT bottom{0, kH * st->scale, rc.right, rc.bottom};
            FillRect(dc, &right, black);
            FillRect(dc, &bottom, black);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:
        if (wp == kShotTimer) {
            KillTimer(hwnd, kShotTimer);
            if (st && !captureClient(hwnd, st->shot)) st->result = 2;
            DestroyWindow(hwnd);
        } else if (wp == kCloseTimer) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

}  // namespace

int showPage(const hires::Page& page, const WindowOptions& options) {
    SetProcessDPIAware();  // real pixels, so an integer scale stays an integer scale
    State st;
    st.scale = options.scale < 1 ? 1 : (options.scale > 8 ? 8 : options.scale);
    st.shot = options.screenshotPath;
    video::renderArgb(page, st.pixels);

    const HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"PacmanApplePortPageWindow";
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;

    RECT r{0, 0, kW * st.scale, kH * st.scale};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;  // fixed size: the scale is chosen at start
    AdjustWindowRect(&r, style, FALSE);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Pac-Man (Apple II port) - page view", style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left,
                              r.bottom - r.top, nullptr, nullptr, inst, &st);
    if (!hwnd) return 1;
    ShowWindow(hwnd, options.screenshotPath.empty() ? SW_SHOW : SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);
    if (!options.screenshotPath.empty()) SetTimer(hwnd, kShotTimer, 300, nullptr);
    else if (options.seconds > 0) SetTimer(hwnd, kCloseTimer, static_cast<UINT>(options.seconds) * 1000u, nullptr);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return st.result;
}

}  // namespace pacman::app
