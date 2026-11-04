#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "gamewindow.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cstring>

#include <windowsx.h>

#include "../video/colour.h"

namespace pacman::app {
namespace {

constexpr int kW = 280;
constexpr int kH = 192;
constexpr UINT_PTR kRefreshTimer = 1;
constexpr const wchar_t* kClass = L"PacmanApplePortGameWindow";
constexpr std::size_t kMaxQueuedKeys = 64;
constexpr int kMinClientW = kW / 2, kMinClientH = kH / 2;   // smallest picture area: half size

}  // namespace

GameWindow::GameWindow(const GameWindowOptions& options) {
    // True pixels on every monitor (no Windows bitmap scaling, which would blur the picture and misplace a full-screen window on a monitor with another scale setting)
    {
        using SetContext = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        const auto setContext = reinterpret_cast<SetContext>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext")));
        if (!(setContext && setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))) SetProcessDPIAware();
    }
    scale_ = std::clamp(options.scale, 1, 8);
    muted_ = options.muted;
    menuAllowed_ = options.menuAllowed;
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = &GameWindow::proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = kClass;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
    page_.clear(0);  // black until the emulation thread sets a page
    RECT r{0, 0, kW * scale_, kH * scale_};
    const DWORD style = WS_OVERLAPPEDWINDOW;   // resizable, with maximize
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
    if (!hwnd_) return scale_;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    return std::max(1, computeViewport(static_cast<int>(rc.right - rc.left), static_cast<int>(rc.bottom - rc.top)).scale);
}

LONG GameWindow::windowedStyle() const {
    // framed: the normal Windows window. bare: no title bar, no frame drawn (WM_NCCALCSIZE gives the whole window to the picture), but WS_THICKFRAME stays so that Windows still lets
    // the edges resize it and keeps the shadow; no maximize button (nothing to click it with)
    return static_cast<LONG>(borders_ ? WS_OVERLAPPEDWINDOW : (WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_SYSMENU));
}

RECT GameWindow::outerRect(int x, int y, int w, int h) const {
    RECT r{x, y, x + w, y + h};
    if (borders_) AdjustWindowRectExForDpi(&r, static_cast<DWORD>(windowedStyle()), FALSE, 0, dpi());
    return r;
}

LRESULT GameWindow::hitTest(LPARAM lp) {
    // borderless windowed mode only: the frame is not drawn, so the edges are found here; everywhere else the picture area moves the window when dragged
    const POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    RECT r{};
    GetWindowRect(hwnd_, &r);
    if (!IsZoomed(hwnd_)) {
        const int b = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi()) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi());
        const bool left = p.x < r.left + b, right = p.x >= r.right - b, top = p.y < r.top + b, bottom = p.y >= r.bottom - b;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
    }
    return HTCAPTION;
}

void GameWindow::setBorders(bool on) {
    if (!hwnd_ || on == borders_) return;
    if (fullscreen_) {   // applies when leaving full screen
        borders_ = on;
        saveSettings();
        return;
    }
    if (IsZoomed(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
    RECT cr{};
    GetClientRect(hwnd_, &cr);
    POINT origin{0, 0};
    ClientToScreen(hwnd_, &origin);
    const bool visible = (GetWindowLongW(hwnd_, GWL_STYLE) & WS_VISIBLE) != 0;
    borders_ = on;
    SetWindowLongW(hwnd_, GWL_STYLE, windowedStyle() | (visible ? static_cast<LONG>(WS_VISIBLE) : 0));
    const RECT r = outerRect(origin.x, origin.y, static_cast<int>(cr.right), static_cast<int>(cr.bottom));   // the picture stays exactly where it is
    SetWindowPos(hwnd_, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    InvalidateRect(hwnd_, nullptr, FALSE);
    saveSettings();
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
    if (IsZoomed(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
    const RECT r = outerRect(0, 0, kW * scale_, kH * scale_);
    SetWindowPos(hwnd_, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    scalePending_ = false;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool GameWindow::setScale(int scale) {
    if (!hwnd_ || scale < 1 || scale > 8) return false;
    scale_ = scale;
    if (!fullscreen_) applyWindowedSize();
    else scalePending_ = true;   // the picture is as large as the screen allows in full screen; the window gets this size when it comes back
    return true;
}

void GameWindow::setFullscreen(bool on) {
    setDisplayMode(on ? DisplayMode::Borderless : DisplayMode::Windowed);
}

void GameWindow::rememberWindowRect() {
    if (!hwnd_ || fullscreen_ || IsIconic(hwnd_) || IsZoomed(hwnd_)) return;
    RECT cr{};
    GetClientRect(hwnd_, &cr);
    POINT o{0, 0};
    ClientToScreen(hwnd_, &o);
    if (cr.right < 1 || cr.bottom < 1) return;
    settings_.hasWindowRect = true;
    settings_.windowX = o.x;
    settings_.windowY = o.y;
    settings_.windowW = cr.right;
    settings_.windowH = cr.bottom;
}

const Settings& GameWindow::settings() {
    settings_.displayMode = wantedMode_;
    settings_.windowBorders = borders_;
    rememberWindowRect();
    return settings_;
}

void GameWindow::saveSettings() {
    if (store_) store_->save(settings());
}

void GameWindow::logMode(const char* what) {
    if (logPath_.empty()) return;
    std::ofstream f{std::filesystem::path(logPath_), std::ios::app};
    RECT cr{};
    if (hwnd_) GetClientRect(hwnd_, &cr);
    f << what << ": mode=" << displayModeKey(mode_) << " client=" << cr.right << "x" << cr.bottom << " renderer=" << (d3d_ ? (d3d_->software() ? "warp" : "hardware") : "gdi") << "\n";
}

GameWindow::RenderInfo GameWindow::renderInfo() const {
    RenderInfo r;
    if (!hwnd_) return r;
    RECT cr{};
    GetClientRect(hwnd_, &cr);
    r.displayW = cr.right;
    r.displayH = cr.bottom;
    r.viewport = computeViewport(r.displayW, r.displayH);
    return r;
}

bool GameWindow::resolutionFits(int w, int h) const {
    if (!hwnd_ || w < kMinClientW || h < kMinClientH) return false;
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi)) return false;
    const RECT outer = outerRect(0, 0, w, h);
    return outer.right - outer.left <= mi.rcWork.right - mi.rcWork.left && outer.bottom - outer.top <= mi.rcWork.bottom - mi.rcWork.top;
}

bool GameWindow::setResolution(int w, int h) {
    if (!hwnd_ || fullscreen_ || !resolutionFits(w, h)) return false;
    if (IsZoomed(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
    RECT cr{};
    GetClientRect(hwnd_, &cr);
    POINT o{0, 0};
    ClientToScreen(hwnd_, &o);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
    // the new picture area is centred on the old one, and the whole window is kept on the monitor
    RECT r = outerRect(o.x + cr.right / 2 - w / 2, o.y + cr.bottom / 2 - h / 2, w, h);
    int dx = 0, dy = 0;
    if (r.right > mi.rcWork.right) dx = mi.rcWork.right - r.right;
    if (r.bottom > mi.rcWork.bottom) dy = mi.rcWork.bottom - r.bottom;
    if (r.left + dx < mi.rcWork.left) dx = mi.rcWork.left - r.left;
    if (r.top + dy < mi.rcWork.top) dy = mi.rcWork.top - r.top;
    SetWindowPos(hwnd_, nullptr, r.left + dx, r.top + dy, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(hwnd_, nullptr, FALSE);
    rememberWindowRect();
    saveSettings();
    logMode("resolution");
    return true;
}

void GameWindow::cycleResolution(int dir) {
    if (!hwnd_ || fullscreen_) return;
    RECT cr{};
    GetClientRect(hwnd_, &cr);
    const int next = nextResolution(resolutionIndex(cr.right, cr.bottom), dir, [&](int i) { return resolutionFits(kResolutions[i].w, kResolutions[i].h); });
    if (next >= 0) setResolution(kResolutions[next].w, kResolutions[next].h);
}

void GameWindow::setDisplayMode(DisplayMode m) {
    if (!hwnd_) return;
    if (m == DisplayMode::Windowed) {
        if (fullscreen_) applyFullscreenWindow(false);
        mode_ = DisplayMode::Windowed;
    } else {
        if (!fullscreen_) applyFullscreenWindow(true);
        mode_ = DisplayMode::Borderless;
    }
    wantedMode_ = mode_;
    saveSettings();
    logMode("set");
}

void GameWindow::attachSettings(SettingsStore* store, const Settings& initial) {
    store_ = store;
    settings_ = initial;
    wantedMode_ = initial.displayMode;
    if (!hwnd_) return;
    // the frame setting and the window as the user left it
    if (!initial.windowBorders) {
        borders_ = false;
        SetWindowLongW(hwnd_, GWL_STYLE, windowedStyle() | (GetWindowLongW(hwnd_, GWL_STYLE) & WS_VISIBLE));
    }
    if (initial.hasWindowRect) {
        // never put the window where it cannot be reached: it must overlap a monitor, and it is cut to the monitor's work area
        int cw = std::max(initial.windowW, kMinClientW), ch = std::max(initial.windowH, kMinClientH);
        RECT want{initial.windowX, initial.windowY, initial.windowX + cw, initial.windowY + ch};
        HMONITOR mon = MonitorFromRect(&want, MONITOR_DEFAULTTONULL);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (!mon) mon = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
        GetMonitorInfoW(mon, &mi);
        const int maxW = mi.rcWork.right - mi.rcWork.left, maxH = mi.rcWork.bottom - mi.rcWork.top;
        const RECT frame = outerRect(0, 0, cw, ch);
        cw = std::min(cw, std::max(kMinClientW, cw - std::max(0, static_cast<int>(frame.right - frame.left) - maxW)));
        ch = std::min(ch, std::max(kMinClientH, ch - std::max(0, static_cast<int>(frame.bottom - frame.top) - maxH)));
        int x = initial.windowX, y = initial.windowY;
        if (!MonitorFromRect(&want, MONITOR_DEFAULTTONULL)) {   // on no monitor any more: centred on the primary one
            x = mi.rcWork.left + (maxW - cw) / 2;
            y = mi.rcWork.top + (maxH - ch) / 2;
        }
        const RECT r = outerRect(x, y, cw, ch);
        SetWindowPos(hwnd_, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    } else {
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    rememberWindowRect();
    // the display mode
    setDisplayMode(initial.displayMode);
}

void GameWindow::applyFullscreenWindow(bool on) {
    if (!hwnd_ || on == fullscreen_) return;
    if (on) {
        rememberWindowRect();
        fullscreen_ = true;   // from here on the window's size is the monitor's, not the user's
        saved_.length = sizeof(saved_);
        GetWindowPlacement(hwnd_, &saved_);
        savedStyle_ = GetWindowLongW(hwnd_, GWL_STYLE);
        savedZoomed_ = IsZoomed(hwnd_) != FALSE;
        RECT cr{};
        GetClientRect(hwnd_, &cr);
        savedClientOrigin_ = POINT{0, 0};
        ClientToScreen(hwnd_, &savedClientOrigin_);
        savedClientSize_ = SIZE{cr.right, cr.bottom};
        SetWindowLongW(hwnd_, GWL_STYLE, static_cast<LONG>(WS_POPUP) | (savedStyle_ & WS_VISIBLE));   // WS_VISIBLE must be kept: without it the window disappears (black screen)
        fitToMonitor();
        fullscreen_ = true;
    } else {
        fullscreen_ = false;
        SetWindowLongW(hwnd_, GWL_STYLE, windowedStyle() | (savedStyle_ & WS_VISIBLE));   // the border setting may have been changed meanwhile
        if (savedZoomed_ && borders_) {   // it was maximised: back to maximised
            SetWindowPlacement(hwnd_, &saved_);
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        } else {   // the window as it was: the same picture area at the same place (or the scale asked for in full screen)
            const int cw = scalePending_ ? kW * scale_ : static_cast<int>(savedClientSize_.cx), ch = scalePending_ ? kH * scale_ : static_cast<int>(savedClientSize_.cy);
            const RECT r = outerRect(savedClientOrigin_.x, savedClientOrigin_.y, cw, ch);
            SetWindowPos(hwnd_, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
        scalePending_ = false;
    }
    InvalidateRect(hwnd_, nullptr, TRUE);
}

bool GameWindow::handleHotkey(unsigned vk, bool ctrl, bool alt) {
    if (vk == VK_F11 || (alt && vk == VK_RETURN)) {
        setDisplayMode(mode_ == DisplayMode::Windowed ? DisplayMode::Borderless : DisplayMode::Windowed);
        return true;
    }
    if (vk == VK_F8) {   // title bar and frame on/off (windowed mode)
        setBorders(!borders_);
        saveSettings();
        return true;
    }
    if (ctrl && (vk == VK_OEM_PLUS || vk == VK_ADD || vk == VK_OEM_MINUS || vk == VK_SUBTRACT)) {
        // one step up or down from the picture's present whole-number scale (a window resized freely is stepped from where it is)
        int base = scale_;
        if (hwnd_ && !fullscreen_) {
            RECT cr{};
            GetClientRect(hwnd_, &cr);
            const bool preset = cr.right == kW * scale_ && cr.bottom == kH * scale_;
            if (!preset) base = std::max(1, computeViewport(static_cast<int>(cr.right), static_cast<int>(cr.bottom)).scale);
        }
        setScale(base + ((vk == VK_OEM_PLUS || vk == VK_ADD) ? 1 : -1));
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
    if (menu_.open()) {   // the game dimmed to a quarter, the menu panel in black with white text
        std::vector<std::uint8_t> lit;
        PauseMenu::View view;
        view.displayMode = mode_;
        view.borders = borders_;
        {
            const RenderInfo ri = renderInfo();
            view.resolution = fullscreen_ ? std::to_string(ri.displayW) + "X" + std::to_string(ri.displayH) : resolutionLabel(ri.displayW, ri.displayH);
        }
        menu_.render(font_, view, lit);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x) {
                std::uint32_t& p = pixels[static_cast<std::size_t>(y) * kW + static_cast<std::size_t>(x)];
                const bool inPanel = x >= PauseMenu::kPanelX && x < PauseMenu::kPanelX + PauseMenu::kPanelW && y >= PauseMenu::kPanelY && y < PauseMenu::kPanelY + PauseMenu::kPanelH;
                if (inPanel) p = lit[static_cast<std::size_t>(y) * kW + static_cast<std::size_t>(x)] ? 0xFFFFFFFFu : 0xFF000000u;
                else p = ((p & 0x00FCFCFCu) >> 2) | 0xFF000000u;
            }
    }
    std::copy(pixels.begin(), pixels.end(), out);
}

void GameWindow::menuChanged() {
    menuOpen_ = menu_.open();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++pageVersion_;   // the refresh timer repaints
    }
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void GameWindow::menuAction(PauseMenu::Action a) {
    switch (a) {
    case PauseMenu::Action::Resume: break;   // the menu hid itself
    case PauseMenu::Action::ToggleBorders: setBorders(!borders_); break;
    case PauseMenu::Action::NextDisplayMode:
    case PauseMenu::Action::PreviousDisplayMode:   // two modes: next and previous are the same
        setDisplayMode(mode_ == DisplayMode::Windowed ? DisplayMode::Borderless : DisplayMode::Windowed);
        break;
    case PauseMenu::Action::NextResolution: cycleResolution(1); break;
    case PauseMenu::Action::PreviousResolution: cycleResolution(-1); break;
    case PauseMenu::Action::Quit: close(); break;   // the normal way out: the window closes, the game threads end, everything is released in order
    case PauseMenu::Action::None: break;
    }
    menuChanged();
}

bool GameWindow::menuKey(unsigned vk) {
    if (!menuAllowed_.load()) {   // no game is running: no menu, and Esc is not an Apple II key any more either
        if (menu_.open()) {
            menu_.hide();
            menuChanged();
        }
        return vk == VK_ESCAPE;
    }
    if (vk == VK_ESCAPE) {
        if (!menu_.open()) menu_.show();
        else menuAction(menu_.key(PauseMenu::Key::Back));
        menuChanged();
        return true;
    }
    if (!menu_.open()) return false;
    switch (vk) {
    case VK_UP: menuAction(menu_.key(PauseMenu::Key::Up)); break;
    case VK_DOWN: menuAction(menu_.key(PauseMenu::Key::Down)); break;
    case VK_LEFT: menuAction(menu_.key(PauseMenu::Key::Left)); break;
    case VK_RIGHT: menuAction(menu_.key(PauseMenu::Key::Right)); break;
    case VK_RETURN:
    case VK_SPACE: menuAction(menu_.key(PauseMenu::Key::Select)); break;
    default: break;   // every other key is for nobody while the menu is open: the game must not see it
    }
    return true;
}

bool GameWindow::pictureCoords(LPARAM lp, int& px, int& py) const {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const Viewport vp = computeViewport(static_cast<int>(rc.right), static_cast<int>(rc.bottom));
    const int x = GET_X_LPARAM(lp) - vp.x, y = GET_Y_LPARAM(lp) - vp.y;
    if (vp.empty() || x < 0 || y < 0 || x >= vp.w || y >= vp.h) return false;
    px = x * kW / vp.w;
    py = y * kH / vp.h;
    return true;
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
    case WM_GETMINMAXINFO: {   // free resizing, but never smaller than a half-size picture
        auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
        const RECT r = outerRect(0, 0, kMinClientW, kMinClientH);
        mm->ptMinTrackSize.x = r.right - r.left;
        mm->ptMinTrackSize.y = r.bottom - r.top;
        return 0;
    }
    case WM_NCCALCSIZE:       // borderless windowed mode: the whole window is the picture area (a maximised window is cut to the work area, not to the overhanging frame)
        if (!borders_ && !fullscreen_ && wp) {
            if (IsZoomed(hwnd_)) {
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
                reinterpret_cast<NCCALCSIZE_PARAMS*>(lp)->rgrc[0] = mi.rcWork;
            }
            return 0;
        }
        break;
    case WM_NCHITTEST:
        if (!borders_ && !fullscreen_) {
            const LRESULT h = hitTest(lp);
            return (h == HTCAPTION && menuOpen_.load()) ? HTCLIENT : h;   // the picture moves the window, except while the menu needs the mouse
        }
        break;
    case WM_SETCURSOR:        // full screen without a menu: no mouse pointer over the picture
        if (LOWORD(lp) == HTCLIENT && fullscreen_ && !menuOpen_.load()) {
            SetCursor(nullptr);
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_MOUSEWHEEL:
        if (menu_.open()) {
            int px = -1, py = -1;
            if (msg == WM_MOUSEWHEEL) {
                menuAction(menu_.key(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? PauseMenu::Key::Up : PauseMenu::Key::Down));
            } else if (pictureCoords(lp, px, py)) {
                if (msg == WM_MOUSEMOVE) {
                    const int before = menu_.selected();
                    menu_.pointerMove(px, py);
                    if (menu_.selected() != before) menuChanged();
                } else {
                    menuAction(menu_.pointerClick(px, py));
                }
            }
            return 0;
        }
        break;
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
        rememberWindowRect();
        return 0;
    case WM_MOVE:
        rememberWindowRect();
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
        const bool alt = msg == WM_SYSKEYDOWN || (lp & (1 << 29)) != 0;   // from the message itself (bit 29: Alt was down when the key was pressed), not from the keyboard now
        const bool repeat = (lp & (1 << 30)) != 0;   // key held down: the full screen switch must not flicker back and forth
        if (repeat && (wp == VK_F11 || (alt && wp == VK_RETURN))) return 0;
        if (handleHotkey(static_cast<unsigned>(wp), ctrl, alt)) return 0;
        if (!alt && !(repeat && wp == VK_ESCAPE) && menuKey(static_cast<unsigned>(wp))) return 0;   // a held Ctrl does not matter for the menu keys (Alt+Esc belongs to Windows)   // Esc and the menu keys never reach the game
        if (repeat && wp == VK_ESCAPE) return 0;
        if (menu_.open()) return 0;   // nothing else reaches the game while the menu is open
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
        if (c >= 0x80 || c == 0x1B || menu_.open()) return 0;   // Esc is the menu key now (the original pause is replaced); nothing reaches the game while the menu is open
        if (c >= 'a' && c <= 'z') c -= 0x20;  // the Apple II keyboard sends capitals
        pushKey(static_cast<std::uint8_t>(0x80 | c));
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd_, kRefreshTimer);
        saveSettings();   // the window's last place and size, the modes: written once more on the way out
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
