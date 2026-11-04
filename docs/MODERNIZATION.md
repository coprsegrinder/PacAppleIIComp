# Modernization phase (started 2026-11-04)

Spec given by the user (12 sections, in this order, each implemented, tested, documented and approved before the next): 1 Fullscreen, 2 Windowed mode improvements, 3 Pause menu, 4 Display mode, 5 Resolution, 6 Window borders, 7 Background (Black / Ambient Glow / CRT Bezel / Apple II Desktop), 8 Colour mode (Original / Native B&W), 9 CRT filter, 10 HDR, 11 FPS, 12 VSync.
Highest priority: the original gameplay (logic, timing, collisions, input, animations) must not change. External assets, shaders and filters: research and present them (source, licence, redistribution), and ask before downloading or integrating anything. Do not change the original simulation without explaining why and getting approval.

## Decisions by the user (2026-11-04)
1. **Viewport:** keeps the picture's own ratio, 280:192 = 1.458:1 (not 4:3). It is never stretched; black bars take the rest, horizontally and vertically, for 4:3, 16:9, 16:10, 21:9 and 32:9. Scaling is nearest neighbour. (My reading, to be corrected if wrong: the viewport is the largest WHOLE-NUMBER multiple of 280x192 that fits, so every Apple pixel is an exact block; below 1x it is shrunk by the largest fraction that fits.)
2. **Esc:** the original Esc behaviour (pause until any key) is replaced by the pause menu (Section 3).
3. **Renderer:** Direct3D 11 first ("Section 0"), because CRT filters, HDR, VSync and frame pacing need it.

## Audit of the existing program (before Section 0)
* Rendering: GDI `StretchDIBits` at a whole-number scale, repaint from a coarse window timer. Window: fixed client size 280x192 x scale; F11/Alt+Enter borderless full screen; Ctrl+Plus/Minus scale; F9 mute.
* Timing: the worker thread runs the game from the virtual 1,022,727 Hz cycle clock; the window thread only shows the newest page under a lock. So window changes cannot change game speed. One game tick is about 2,927 cycles = about 349 ticks per second, much faster than any display refresh (matters for Section 11).
* Input: window thread turns key events into Apple II key codes in a queue. Settings: none are saved yet. Menus: only the game's own text screens. Audio: WASAPI mixer. Build: build.bat (cl) + CMakeLists.txt, no third-party libraries.

## Section 0 - Direct3D 11 renderer (done 2026-11-04, TASK-034-0)
* `port/src/app/d3drenderer.{h,cpp}`: device (hardware, WARP software device as the second try), flip-model swap chain (DXGI Alt+Enter handling switched off, the window's own hotkey stays), the 280x192 picture as a dynamic BGRA texture, one fullscreen triangle drawn into the viewport with a point sampler, UNORM (no colour conversion), black clear. Shaders are compiled at start-up with the system `d3dcompiler`. No download, no new dependency (system libs d3d11, dxgi, d3dcompiler).
* `port/src/app/display_layout.h`: `computeViewport(clientW, clientH)`: pure function, whole-number scale, centred, fractional fit below 1x. Used by Direct3D and testable alone.
* `GameWindow`: uses Direct3D when it starts, otherwise (or with environment variable `PACMAN_GDI=1`) the old GDI drawing, which stays as the fallback. `WM_SIZE` resizes the swap chain. `captureFrame()` (tests) reads the back buffer.
* Present uses sync interval 0 for now (no waiting for the monitor); VSync is Section 12. Repaint is still driven by the window timer; frame pacing is Section 11.
* **Tests (V, tests/test_gamewindow.cpp):** layout for 11 screen shapes incl. 21:9, 32:9, 4:3, 16:10, exact fit, sub-1x; the Direct3D picture of a page with every byte different equals the CPU-scaled picture **byte for byte** (0 of 483,840 pixels differ at 3x; 0 differ at five other client shapes with bars on every side; 0 differ in full screen at 2560x1440, three round trips of Alt+Enter); the GDI fallback still draws. Suite 153/0. Live check: the real game's boot menu is drawn through Direct3D.
* Limits: the layout is whole-number scale only (a "fill" mode is not offered); an HDR or wide-colour swap chain is Section 10.

## Fix 2026-11-04: full screen showed a black screen with a white block (user report; broken since TASK-028k)
* **Cause (V, reproduced on a visible window with the window-only screenshot script, and by a failing test):** `GameWindow::setFullscreen` replaced the window style with `WS_POPUP` and so dropped `WS_VISIBLE`; Windows then treated the window as hidden and stopped drawing it. The old fullscreen test used hidden windows (`visible = false`), where the flag is irrelevant, so it never saw this.
* **Fix:** the popup style keeps the saved window's `WS_VISIBLE`. New test `visible_window_full_screen_stays_visible_and_shows_the_picture` (a visible window: stays visible, covers the monitor, Direct3D picture byte-equal, F11 back restores the 840x576 client). Suite 154/0. Real game checked: full screen fills the monitor with the picture centred at the largest whole-number scale, F11 returns to the window.
* Lesson recorded: window behaviour must also be tested on VISIBLE windows.
