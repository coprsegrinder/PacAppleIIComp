#include "gamepad.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
extern "C" {
#include <hidsdi.h>
}
#include <hidpi.h>

namespace pacman::app {

namespace {

constexpr int kStickThreshold = 40;   // of 127: where the left stick counts as pushed (a little under a third)

void setDirections(PadState& s, bool dUp, bool dDown, bool dLeft, bool dRight, int sx, int sy) {
    s.dpadUp = dUp;
    s.dpadDown = dDown;
    s.dpadLeft = dLeft;
    s.dpadRight = dRight;
    s.stickX = sx;
    s.stickY = sy;
    s.up = dUp || sy <= -kStickThreshold;
    s.down = dDown || sy >= kStickThreshold;
    s.left = dLeft || sx <= -kStickThreshold;
    s.right = dRight || sx >= kStickThreshold;
}

// the hat switch of the Sony pads: 0 = up, then clockwise in steps of 45 degrees; 8 (and more) = released
void hatToDirections(int hat, bool& up, bool& down, bool& left, bool& right) {
    up = hat == 7 || hat == 0 || hat == 1;
    right = hat == 1 || hat == 2 || hat == 3;
    down = hat == 3 || hat == 4 || hat == 5;
    left = hat == 5 || hat == 6 || hat == 7;
}

// the byte layout shared by the DualShock 4 and the DualSense: buttons0 = hat (low nibble) + square, cross, circle, triangle (bits 4..7); buttons1 = L1, R1, L2, R2, share/create, options, L3, R3
void fromSonyButtons(PadState& s, std::uint8_t lx, std::uint8_t ly, std::uint8_t b0, std::uint8_t b1) {
    bool u, d, l, r;
    hatToDirections(b0 & 0x0F, u, d, l, r);
    setDirections(s, u, d, l, r, static_cast<int>(lx) - 128, static_cast<int>(ly) - 128);
    s.confirm = (b0 & 0x20) != 0;   // cross
    s.back = (b0 & 0x40) != 0;      // circle
    s.menu = (b1 & 0x20) != 0;      // options
    s.anyInput = (b0 & 0xF0) != 0 || (b0 & 0x0F) < 8 || (b1 & 0xFF) != 0 || s.up || s.down || s.left || s.right;
}

}  // namespace

std::optional<std::uint8_t> padDirection(const PadState& s) noexcept {
    if (s.dpadUp || s.dpadDown || s.dpadLeft || s.dpadRight) {
        if (s.dpadLeft && !s.dpadRight) return static_cast<std::uint8_t>(3);
        if (s.dpadRight && !s.dpadLeft) return static_cast<std::uint8_t>(2);
        if (s.dpadUp && !s.dpadDown) return static_cast<std::uint8_t>(1);
        if (s.dpadDown && !s.dpadUp) return static_cast<std::uint8_t>(0);
        return std::nullopt;
    }
    const int ax = std::abs(s.stickX), ay = std::abs(s.stickY);
    if (ax < kStickThreshold && ay < kStickThreshold) return std::nullopt;
    if (ax >= ay) return static_cast<std::uint8_t>(s.stickX < 0 ? 3 : 2);
    return static_cast<std::uint8_t>(s.stickY < 0 ? 1 : 0);
}

bool parseDualShock4(const std::uint8_t* d, std::size_t n, PadState& out) noexcept {
    if (!d || n < 1) return false;
    std::size_t base;
    if (d[0] == 0x01) base = 1;         // USB, and the short report of Bluetooth
    else if (d[0] == 0x11) base = 3;    // Bluetooth, the full report (two bytes of header after the id)
    else return false;
    if (n < base + 6) return false;
    out = PadState{};
    out.kind = PadKind::DualShock4;
    fromSonyButtons(out, d[base], d[base + 1], d[base + 4], d[base + 5]);
    return true;
}

bool parseDualSense(const std::uint8_t* d, std::size_t n, bool bluetooth, PadState& out) noexcept {
    if (!d || n < 1) return false;
    std::size_t base, b0;
    if (d[0] == 0x01 && !bluetooth) { base = 1; b0 = 8; }      // USB: x y rx ry l2 r2 counter, then the buttons
    else if (d[0] == 0x01) { base = 1; b0 = 5; }               // Bluetooth, the short report: x y rx ry, then the buttons
    else if (d[0] == 0x31) { base = 2; b0 = 9; }               // Bluetooth, the full report (one byte of header after the id)
    else return false;
    if (n < b0 + 2) return false;
    out = PadState{};
    out.kind = PadKind::DualSense;
    fromSonyButtons(out, d[base], d[base + 1], d[b0], d[b0 + 1]);
    return true;
}

bool parseXInput(std::uint16_t buttons, std::int16_t lx, std::int16_t ly, PadState& out) noexcept {
    out = PadState{};
    out.kind = PadKind::XInput;
    const int sx = std::clamp(static_cast<int>(lx) / 256, -127, 127);
    const int sy = std::clamp(-static_cast<int>(ly) / 256, -127, 127);   // XInput: up is positive; here down is positive
    setDirections(out, (buttons & 0x0001) != 0, (buttons & 0x0002) != 0, (buttons & 0x0004) != 0, (buttons & 0x0008) != 0, sx, sy);
    out.confirm = (buttons & 0x1000) != 0;   // A
    out.back = (buttons & 0x2000) != 0;      // B
    out.menu = (buttons & 0x0010) != 0;      // Start
    out.anyInput = (buttons & 0xF3FF) != 0 || out.up || out.down || out.left || out.right;
    return true;
}

// ------------------------------------------------------------------------------------------------ the part that talks to Windows

struct GamepadHub::Impl {
    struct XInputState {   // XINPUT_STATE
        DWORD packet;
        WORD buttons;
        BYTE lt, rt;
        SHORT lx, ly, rx, ry;
    };
    using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XInputState*);

    struct Source {
        PadKind kind = PadKind::None;
        PadState state;
        std::uint64_t lastActiveMs = 0;
        bool present = false;
    };
    struct HidDevice {
        std::wstring path;
        HANDLE h = INVALID_HANDLE_VALUE;
        PadKind kind = PadKind::None;
        bool bluetooth = false;
        std::vector<std::uint8_t> buf;
        OVERLAPPED ov{};
        bool pending = false;
        Source src;
    };

    mutable std::mutex m;
    std::thread th;
    std::atomic<bool> running{false};
    std::atomic<std::uint32_t> changes{0};
    Source xin[4];
    std::vector<HidDevice*> hid;
    bool injected = false;
    Source injectedSource;
    XInputGetStateFn xGet = nullptr;
    HMODULE xlib = nullptr;

    static std::uint64_t nowMs() { return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }

    void loadXInput() {
        for (const wchar_t* name : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
            xlib = LoadLibraryW(name);
            if (xlib) break;
        }
        if (xlib) xGet = reinterpret_cast<XInputGetStateFn>(GetProcAddress(xlib, "XInputGetState"));
    }

    void pollXInput(bool scanEmpty) {
        if (!xGet) return;
        for (DWORD i = 0; i < 4; ++i) {
            if (!xin[i].present && !scanEmpty) continue;   // (an empty slot is slow to ask: only once in a while)
            XInputState st{};
            const DWORD r = xGet(i, &st);
            std::lock_guard<std::mutex> lock(m);
            if (r != ERROR_SUCCESS) {
                if (xin[i].present) {
                    xin[i] = Source{};
                    ++changes;
                }
                continue;
            }
            if (!xin[i].present) ++changes;
            xin[i].present = true;
            xin[i].kind = PadKind::XInput;
            parseXInput(st.buttons, st.lx, st.ly, xin[i].state);
            if (xin[i].state.anyInput) xin[i].lastActiveMs = nowMs();
        }
    }

    static bool sonyKind(USHORT vid, USHORT pid, PadKind& kind) {
        if (vid != 0x054C) return false;
        if (pid == 0x05C4 || pid == 0x09CC || pid == 0x0BA0) { kind = PadKind::DualShock4; return true; }   // DualShock 4 (two revisions) and its USB adapter
        if (pid == 0x0CE6 || pid == 0x0DF2) { kind = PadKind::DualSense; return true; }                      // DualSense and DualSense Edge
        return false;
    }

    void closeDevice(HidDevice* d) {
        if (d->h != INVALID_HANDLE_VALUE) {
            if (d->pending) {
                CancelIoEx(d->h, &d->ov);
                DWORD n = 0;
                GetOverlappedResult(d->h, &d->ov, &n, TRUE);
            }
            CloseHandle(d->h);
        }
        if (d->ov.hEvent) CloseHandle(d->ov.hEvent);
        delete d;
    }

    void scanHid() {
        GUID guid;
        HidD_GetHidGuid(&guid);
        HDEVINFO info = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (info == INVALID_HANDLE_VALUE) return;
        std::vector<std::wstring> seen;
        for (DWORD i = 0;; ++i) {
            SP_DEVICE_INTERFACE_DATA ifd{};
            ifd.cbSize = sizeof(ifd);
            if (!SetupDiEnumDeviceInterfaces(info, nullptr, &guid, i, &ifd)) break;
            DWORD need = 0;
            SetupDiGetDeviceInterfaceDetailW(info, &ifd, nullptr, 0, &need, nullptr);
            if (need == 0) continue;
            std::vector<std::uint8_t> raw(need);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(raw.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (!SetupDiGetDeviceInterfaceDetailW(info, &ifd, detail, need, nullptr, nullptr)) continue;
            const std::wstring path = detail->DevicePath;
            seen.push_back(path);
            bool known = false;
            {
                std::lock_guard<std::mutex> lock(m);
                for (const HidDevice* d : hid) known = known || d->path == path;
            }
            if (known) continue;
            HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
            if (h == INVALID_HANDLE_VALUE) continue;
            HIDD_ATTRIBUTES attr{};
            attr.Size = sizeof(attr);
            PadKind kind = PadKind::None;
            if (!HidD_GetAttributes(h, &attr) || !sonyKind(attr.VendorID, attr.ProductID, kind)) {
                CloseHandle(h);
                continue;
            }
            PHIDP_PREPARSED_DATA pp = nullptr;
            HIDP_CAPS caps{};
            bool ok = false;
            if (HidD_GetPreparsedData(h, &pp)) {
                ok = HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS;
                HidD_FreePreparsedData(pp);
            }
            if (!ok || caps.UsagePage != 0x01 || caps.Usage != 0x05 || caps.InputReportByteLength < 10) {   // the game pad interface only (not the motion sensor or the touch pad)
                CloseHandle(h);
                continue;
            }
            auto* d = new HidDevice();
            d->path = path;
            d->h = h;
            d->kind = kind;
            d->bluetooth = kind == PadKind::DualSense && caps.InputReportByteLength != 64;
            d->buf.assign(caps.InputReportByteLength, 0);
            d->ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            d->src.kind = kind;
            d->src.present = true;
            std::lock_guard<std::mutex> lock(m);
            hid.push_back(d);
            ++changes;
        }
        SetupDiDestroyDeviceInfoList(info);
        // devices no longer listed are gone
        std::vector<HidDevice*> gone;
        {
            std::lock_guard<std::mutex> lock(m);
            for (auto it = hid.begin(); it != hid.end();) {
                if (std::find(seen.begin(), seen.end(), (*it)->path) == seen.end()) {
                    gone.push_back(*it);
                    it = hid.erase(it);
                    ++changes;
                } else {
                    ++it;
                }
            }
        }
        for (HidDevice* d : gone) closeDevice(d);
    }

    void pollHid() {
        std::vector<HidDevice*> failed;
        std::lock_guard<std::mutex> lock(m);
        for (HidDevice* d : hid) {
            for (int round = 0; round < 8; ++round) {   // (all the reports that have arrived, the newest counts)
                if (!d->pending) {
                    ResetEvent(d->ov.hEvent);
                    DWORD got = 0;
                    if (ReadFile(d->h, d->buf.data(), static_cast<DWORD>(d->buf.size()), &got, &d->ov)) {
                        handleReport(*d, got);
                        continue;
                    }
                    if (GetLastError() != ERROR_IO_PENDING) {
                        failed.push_back(d);
                        break;
                    }
                    d->pending = true;
                }
                if (WaitForSingleObject(d->ov.hEvent, 0) != WAIT_OBJECT_0) break;
                DWORD got = 0;
                d->pending = false;
                if (!GetOverlappedResult(d->h, &d->ov, &got, FALSE)) {
                    failed.push_back(d);
                    break;
                }
                handleReport(*d, got);
            }
        }
        for (HidDevice* d : failed) {   // pulled out: closed here, the next scan finds it again when it returns
            auto it = std::find(hid.begin(), hid.end(), d);
            if (it == hid.end()) continue;
            hid.erase(it);
            ++changes;
            d->pending = false;
            closeDevice(d);
        }
    }

    void handleReport(HidDevice& d, DWORD n) {
        PadState st;
        const bool ok = d.kind == PadKind::DualShock4 ? parseDualShock4(d.buf.data(), n, st) : parseDualSense(d.buf.data(), n, d.bluetooth, st);
        if (!ok) return;
        d.src.state = st;
        if (st.anyInput) d.src.lastActiveMs = nowMs();
    }

    void loop() {
        loadXInput();
        std::uint64_t lastScan = 0, lastXScan = 0;
        while (running.load()) {
            const std::uint64_t now = nowMs();
            if (now - lastScan >= 1000 || lastScan == 0) {
                lastScan = now;
                scanHid();
            }
            const bool scanEmpty = now - lastXScan >= 1000 || lastXScan == 0;
            if (scanEmpty) lastXScan = now;
            pollXInput(scanEmpty);
            pollHid();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        std::vector<HidDevice*> all;
        {
            std::lock_guard<std::mutex> lock(m);
            all.swap(hid);
        }
        for (HidDevice* d : all) closeDevice(d);
        if (xlib) FreeLibrary(xlib);
    }

    // the controller in use: the one with the newest input; before any input, the first that is plugged in
    const Source* inUse() const {
        const Source* best = nullptr;
        const auto consider = [&](const Source& s) {
            if (!s.present) return;
            if (!best || s.lastActiveMs > best->lastActiveMs) best = &s;
        };
        if (injected) return injectedSource.present ? &injectedSource : nullptr;
        for (const Source& s : xin) consider(s);
        for (const HidDevice* d : hid) consider(d->src);
        return best;
    }
};

GamepadHub::GamepadHub() : d_(std::make_unique<Impl>()) {}
GamepadHub::~GamepadHub() { stop(); }

void GamepadHub::start() {
    if (d_->running.exchange(true)) return;
    d_->th = std::thread([this] { d_->loop(); });
}

void GamepadHub::stop() {
    if (!d_->running.exchange(false)) return;
    if (d_->th.joinable()) d_->th.join();
}

PadState GamepadHub::state() const {
    std::lock_guard<std::mutex> lock(d_->m);
    const Impl::Source* s = d_->inUse();
    return s ? s->state : PadState{};
}

PadKind GamepadHub::connected() const {
    std::lock_guard<std::mutex> lock(d_->m);
    const Impl::Source* s = d_->inUse();
    return s ? s->kind : PadKind::None;
}

bool GamepadHub::has(PadKind k) const {
    std::lock_guard<std::mutex> lock(d_->m);
    if (d_->injected) return d_->injectedSource.present && d_->injectedSource.kind == k;
    for (const auto& s : d_->xin)
        if (s.present && s.kind == k) return true;
    for (const auto* d : d_->hid)
        if (d->src.present && d->src.kind == k) return true;
    return false;
}

std::uint32_t GamepadHub::changeCount() const { return d_->changes.load(); }

void GamepadHub::injectForTests(const PadState& s, bool present) {
    std::lock_guard<std::mutex> lock(d_->m);
    d_->injected = true;
    d_->injectedSource.state = s;
    d_->injectedSource.kind = s.kind;
    d_->injectedSource.present = present;
    ++d_->changes;
}

}  // namespace pacman::app
