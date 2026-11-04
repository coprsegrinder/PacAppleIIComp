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

## Section 1 - Fullscreen (done 2026-11-04, TASK-034-1)
What the spec asks and where it stands:
| Requirement | Status |
|---|---|
| Alt+Enter toggles fullscreen / windowed | V (F11 does the same; both unchanged keys; holding the key now switches ONCE: auto-repeat is ignored, test) |
| Keep the picture's ratio (280:192 by the user's decision, not 4:3), never stretch, 16:9 / 16:10 / 21:9 / ultrawide | V: `computeViewport` + byte-exact Direct3D picture at 2560x1440 full screen; layout test covers 4:3, 16:9, 16:10, 21:9 (2 sizes), 32:9, exact fits |
| Unused space filled with the selected background | V: `background.h` (`Background`, `backgroundFill`) is asked by both renderers; only Black exists until Section 7; test: every bar pixel is the fill and the picture is untouched |
| Centred | V (layout test: margins differ by at most 1 pixel) |
| Switching does not interrupt or reset the game | V: test runs 150,000 game ticks on a worker thread while the main thread switches fullscreen 150 times: final page, score and level are identical to the run without switching |
| Switching does not affect timing or speed | V: while fullscreen is switched 27 times in 0.8 s the cycle clock hands out exactly the cycles the elapsed time earned (831,598 = 831,598), nothing skipped |
Also done: per-monitor DPI awareness (true pixels on every monitor; `WM_DPICHANGED` keeps the window's picture size), `WM_DISPLAYCHANGE` refits a fullscreen window to its monitor (test), fullscreen goes to the monitor the window is on, the window's own style keeps `WS_VISIBLE` (earlier fix). Suite 158/0; real game checked in full screen with the window-only screenshot script.
Limits / not done: the mouse cursor stays visible in full screen (it will be needed for the pause menu in Section 3); there is no exclusive-mode fullscreen (borderless fullscreen only, which is what the Section 4 "Borderless Fullscreen" will use; the separate "Fullscreen" option of Section 4 needs a decision, see below); window position is not remembered between runs (Section 4, settings file).

## Decision 4 (2026-11-04, for Section 4): Fullscreen vs Borderless Fullscreen
User: "Go with your recommendation." So: **Fullscreen = true exclusive fullscreen** that switches the display to the chosen Resolution (Section 5), with a safe fallback when the mode is unsupported; **Borderless Fullscreen = the current borderless window at the desktop's native resolution, and the default**; the settings page notes that Borderless gives the sharpest picture because exclusive mode's scaling is done by the monitor/GPU. Reasons: on Windows 10/11 a flip-model borderless window has the same latency as exclusive, alt-tab and overlays are better, and VRR/HDR work from borderless; exclusive only adds a real resolution change, a forced refresh rate and old-hardware compatibility.
