// TASK-034-0 (modernization): the game picture through Direct3D 11 (part of Windows, nothing to download), replacing the GDI drawing of the game window. It shows exactly the same
// picture: the 280 x 192 ARGB image as a texture, nearest-neighbour, inside the viewport given by display_layout.h, black everywhere else. No colour conversion (UNORM, not sRGB).
// The swap chain is a flip-model chain; Alt+Enter is NOT handled by DXGI (the window handles it itself). Later sections build on this: scaling, filters, HDR, VSync, frame pacing.
// Window thread only.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "background.h"
#include "display_layout.h"

namespace pacman::app {

class D3dRenderer {
public:
    D3dRenderer();
    ~D3dRenderer();
    D3dRenderer(const D3dRenderer&) = delete;
    D3dRenderer& operator=(const D3dRenderer&) = delete;

    bool init(HWND hwnd);                                            // false: no usable Direct3D 11 device (hardware, then the WARP software rasteriser, are tried)
    void resize(int clientW, int clientH);                           // the back buffer follows the client area
    void setBackground(Background b);                                // what fills the bars
    bool draw(const std::uint32_t* argb280x192, const Viewport& vp); // clears to the background, draws the picture into vp; nothing is shown until present()
    bool present();                                                  // sync interval 0 for now (VSync is its own section)
    bool readBack(std::vector<std::uint32_t>& argb, int& w, int& h); // the back buffer after draw() (tests)
    // Exclusive full screen (the "Fullscreen" display mode): the swap chain takes over the monitor at the desktop's own resolution and refresh rate. Returns false (and stays windowed)
    // if Windows or the display refuses; the caller then falls back to the borderless window. Leaving always works.
    bool setExclusive(bool on);
    const std::string& failure() const noexcept;                     // why the last setExclusive(true) was refused (stage and HRESULT), for the log and the tests
    bool exclusive() const;                                          // really in exclusive full screen now (false after Alt+Tab away, until it is entered again)
    bool software() const noexcept;                                  // true when the WARP software device is in use
    int width() const noexcept;
    int height() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

}  // namespace pacman::app
