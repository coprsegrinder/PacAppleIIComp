#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "gamewindow.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

#include "../video/colour.h"

namespace pacman::app {
namespace {

constexpr int kW = 280;
constexpr int kH = 192;
constexpr UINT_PTR kRefreshTimer = 1;
constexpr const wchar_t* kClass = L"PacmanApplePortGameWindow";
constexpr std::size_t kMaxQueuedKeys = 64;

}  // namespace

GameWindow::GameWindow(const GameWindowOptions& options) {
    // True pixels on every monitor (no Windows bitmap scaling, which would blur the picture and misplace a full-screen window on a monitor with another scale setting)
    {
        using SetContext = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        const auto setContext = reinterpret_cast<SetContext>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext")));
        if (!(setContext && setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))) SetProcessDPIAware();
    }
    scale_ = std::clamp(options.scale, 1, 8);
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = &GameWindow::proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = kClass;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
    page_.clear(0);  // black until the emulation thread sets a page
    RECT r{0, 0, kW * scale_, kH * scale_};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, GetDpiForSystem());
    hwnd_ = CreateWindowW(kClass, L"Pac-Man (Apple II port)", style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr,
                          inst, this);
    if (!hwnd_) return;
    HDC dc = GetDC(hwnd_);
    const int hz = GetDeviceCaps(dc, VREFRESH);
    ReleaseDC(hwnd_, dc);
    refreshHz_ = hz >= 24 && hz <= 500 ? static_cast<unsigned>(hz) : 60u;
    // WM_TIMER is coarse (about 15.6 ms); good enough for the first version, a better refresh source can replace it later
    SetTimer(hwnd_, kRefreshTimer, std::max(1u, 1000u / refreshHz_), nullptr);
    char* gdiOnly = nullptr;
    _dupenv_s(&gdiOnly, nullptr, "PACMAN_GDI");
    const bool useGdi = gdiOnly != nullptr && gdiOnly[0] != 0 && gdiOnly[0] != '0';   // PACMAN_GDI=1 forces the GDI fallback
    free(gdiOnly);
    if (!useGdi) {   // Direct3D 11 draws the picture; GDI stays as the fallback
        auto renderer = std::make_unique<D3dRenderer>();
        if (renderer->init(hwnd_)) d3d_ = std::move(renderer);
    }
    if (options.visible) {
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
    }
}

GameWindow::~GameWindow() {
    if (hwnd_) DestroyWindow(hwnd_);
}

void GameWindow::setPage(const hires::Page& page) {
    std::lock_guard<std::mutex> lock(mutex_);
    page_ = page;
    useImage_ = false;
    ++pageVersion_;
}

void GameWindow::setImage(std::span<const std::uint32_t> argb) {
    if (argb.size() < static_cast<std::size_t>(kW) * kH) return;
    std::lock_guard<std::mutex> lock(mutex_);
    image_.assign(argb.begin(), argb.begin() + static_cast<std::ptrdiff_t>(kW) * kH);
    useImage_ = true;
    ++pageVersion_;
}

std::optional<std::uint8_t> GameWindow::popKey() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (keys_.empty()) return std::nullopt;
    const std::uint8_t k = keys_.front();
    keys_.pop_front();
    return k;
}

void GameWindow::pushKey(std::uint8_t code) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (keys_.size() < kMaxQueuedKeys) keys_.push_back(code);
}

int GameWindow::effectiveScale() const noexcept {
    if (!fullscreen_ || !hwnd_) return scale_;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    return std::max(1, std::min(static_cast<int>(rc.right - rc.left) / kW, static_cast<int>(rc.bottom - rc.top) / kH));
}

UINT GameWindow::dpi() const {
    const UINT d = hwnd_ ? GetDpiForWindow(hwnd_) : 0;
    return d != 0 ? d : 96;
}

void GameWindow::fitToMonitor() {
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
    SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void GameWindow::setBackground(Background b) {
    background_ = b;
    if (d3d_) d3d_->setBackground(b);
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void GameWindow::applyWindowedSize() {
    RECT r{0, 0, kW * scale_, kH * scale_};
    AdjustWindowRectExForDpi(&r, static_cast<DWORD>(GetWindowLongW(hwnd_, GWL_STYLE)), FALSE, 0, dpi());
    SetWindowPos(hwnd_, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool GameWindow::setScale(int scale) {
    if (!hwnd_ || scale < 1 || scale > 8) return false;
    scale_ = scale;
    if (!fullscreen_) applyWindowedSize();
    return true;
}

void GameWindow::setFullscreen(bool on) {
    if (!hwnd_ || on == fullscreen_) return;
    if (on) {
        saved_.length = sizeof(saved_);
        GetWindowPlacement(hwnd_, &saved_);
        savedStyle_ = GetWindowLongW(hwnd_, GWL_STYLE);
        SetWindowLongW(hwnd_, GWL_STYLE, static_cast<LONG>(WS_POPUP) | (savedStyle_ & WS_VISIBLE));   // WS_VISIBLE must be kept: without it the window disappears (black screen)
        fitToMonitor();
        fullscreen_ = true;
    } else {
        SetWindowLongW(hwnd_, GWL_STYLE, savedStyle_);
        fullscreen_ = false;
        SetWindowPlacement(hwnd_, &saved_);
        applyWindowedSize();
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    InvalidateRect(hwnd_, nullptr, TRUE);
}

bool GameWindow::handleHotkey(unsigned vk, bool ctrl, bool alt) {
    if (vk == VK_F11 || (alt && vk == VK_RETURN)) {
        setFullscreen(!fullscreen_);
        return true;
    }
    if (ctrl && (vk == VK_OEM_PLUS || vk == VK_ADD)) {
        setScale(scale_ + 1);
        return true;
    }
    if (ctrl && (vk == VK_OEM_MINUS || vk == VK_SUBTRACT)) {
        setScale(scale_ - 1);
        return true;
    }
    if (vk == VK_F9) {
        muted_ = !muted_;
        return true;
    }
    return false;
}

void GameWindow::currentPixels(std::uint32_t* out) {
    std::array<std::uint32_t, kW * kH> pixels{};
    hires::Page copy;
    bool image = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        image = useImage_ && image_.size() >= pixels.size();
        if (image) std::copy(image_.begin(), image_.begin() + static_cast<std::ptrdiff_t>(pixels.size()), pixels.begin());
        else copy = page_;
        paintedVersion_ = pageVersion_;
    }
    if (!image) video::renderArgb(copy, pixels);
    std::copy(pixels.begin(), pixels.end(), out);
}

bool GameWindow::captureFrame(std::vector<std::uint32_t>& argb, int& w, int& h) {
    if (!d3d_ || !hwnd_) return false;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    d3d_->resize(static_cast<int>(rc.right), static_cast<int>(rc.bottom));
    std::array<std::uint32_t, kW * kH> pixels{};
    currentPixels(pixels.data());
    if (!d3d_->draw(pixels.data(), computeViewport(d3d_->width(), d3d_->height()))) return false;
    return d3d_->readBack(argb, w, h);
}

void GameWindow::paint() {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd_, &ps);
    std::array<std::uint32_t, kW * kH> pixels{};
    currentPixels(pixels.data());
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    if (d3d_) {   // Direct3D 11: same picture, same whole-number scale, same bars
        d3d_->resize(static_cast<int>(rc.right), static_cast<int>(rc.bottom));
        if (d3d_->draw(pixels.data(), computeViewport(d3d_->width(), d3d_->height()))) d3d_->present();
        EndPaint(hwnd_, &ps);
        return;
    }
    const int s = effectiveScale();
    const int w = kW * s, h = kH * s;
    const int ox = std::max(0, (static_cast<int>(rc.right) - w) / 2), oy = std::max(0, (static_cast<int>(rc.bottom) - h) / 2);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = kW;
    bi.bmiHeader.biHeight = -kH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, ox, oy, w, h, 0, 0, kW, kH, pixels.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
    const BackgroundFill fill = backgroundFill(background_);
    HBRUSH black = CreateSolidBrush(RGB(static_cast<int>(fill.r * 255.0f + 0.5f), static_cast<int>(fill.g * 255.0f + 0.5f), static_cast<int>(fill.b * 255.0f + 0.5f)));
    const RECT bars[4] = {{0, 0, rc.right, oy}, {0, oy + h, rc.right, rc.bottom}, {0, oy, ox, oy + h}, {ox + w, oy, rc.right, oy + h}};
    for (const RECT& b : bars)
        if (b.right > b.left && b.bottom > b.top) FillRect(dc, &b, black);
    DeleteObject(black);
    EndPaint(hwnd_, &ps);
}

LRESULT GameWindow::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_DISPLAYCHANGE:   // resolution or monitor layout changed: a full-screen window must cover its monitor again
        if (fullscreen_) fitToMonitor();
        return 0;
    case WM_DPICHANGED:      // moved to a monitor with another scale setting: the window keeps its picture size in real pixels
        if (!fullscreen_) {
            const RECT* suggested = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            applyWindowedSize();
        }
        return 0;
    case WM_SIZE:
        if (d3d_ && wp != SIZE_MINIMIZED) d3d_->resize(static_cast<int>(LOWORD(lp)), static_cast<int>(HIWORD(lp)));
        return 0;
    case WM_TIMER:
        if (wp == kRefreshTimer) {
            bool changed;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                changed = pageVersion_ != paintedVersion_;
            }
            if (changed) InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool alt = msg == WM_SYSKEYDOWN || (GetKeyState(VK_MENU) & 0x8000) != 0;
        const bool repeat = (lp & (1 << 30)) != 0;   // key held down: the full screen switch must not flicker back and forth
        if (repeat && (wp == VK_F11 || (alt && wp == VK_RETURN))) return 0;
        if (handleHotkey(static_cast<unsigned>(wp), ctrl, alt)) return 0;
        switch (wp) {
        case VK_LEFT: pushKey(0x88); return 0;   // Ctrl-H
        case VK_RIGHT: pushKey(0x95); return 0;  // Ctrl-U
        case VK_UP: pushKey(0xC1); return 0;     // same as 'A' (up)
        case VK_DOWN: pushKey(0xDA); return 0;   // same as 'Z' (down)
        default: break;
        }
        if (msg == WM_SYSKEYDOWN) break;  // Alt+F4 etc.: let Windows act
        return 0;
    }
    case WM_CHAR: {
        unsigned c = static_cast<unsigned>(wp);
        if (c >= 0x80) return 0;
        if (c >= 'a' && c <= 'z') c -= 0x20;  // the Apple II keyboard sends capitals
        pushKey(static_cast<std::uint8_t>(0x80 | c));
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd_, kRefreshTimer);
        d3d_.reset();
        hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;
    default: break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

LRESULT CALLBACK GameWindow::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    GameWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = static_cast<GameWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<GameWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    return self->handle(msg, wp, lp);
}

int GameWindow::run() {
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}

void GameWindow::close() {
    if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

}  // namespace pacman::app
