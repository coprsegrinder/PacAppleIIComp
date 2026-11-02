# Window, pacing and input design (TASK-028j, decided by the user 2026-11-02)

Decisions (all four were offered with a recommendation; the user chose the recommended option each time):

| # | Question | Decision |
|---|---|---|
| W1 | Picture shape (faithful default) | **Square pixels, whole-number scale** (1-8, plus full screen at the largest whole number that fits). 280x192 is within about 9% of 4:3. A 4:3 stretch is a possible later option, off by default. |
| W2 | Pacing | **Virtual 1.0227 MHz cycle clock** drives the game logic (slowdowns when much is drawn are kept, see ARCHITECTURE.md); the window shows the **newest page at the monitor's refresh**, independent of the game clock, so 60 Hz and high-refresh screens both work. A "steady speed" mode may come later as a separate option. |
| W3 | Window extras | **Hotkeys only, no menu bar**: faithful mode shows only the original game's own screens. |
| W4 | Graphics API | **GDI until a modern option needs more**; Direct3D 11 later as its own step (smoothing, high-refresh pacing, HDR evaluation). This replaces the earlier default wording "Direct3D 11 first" in PORT_PLAN.md D3. |

## Design that follows

**Threads and clocks (planned, M3/M4):** one *emulation* thread advances the virtual cycle counter in step with `QueryPerformanceCounter` (target 1,022,727 cycles per second; it never runs faster than real time and, if the PC stalls, catches up by at most 100 ms and then drops the rest, so it cannot spiral). It owns the hi-res `Page`. The *window* thread copies the page (8 KB, under a short lock or a double buffer) when it paints and repaints at the monitor refresh. Sound (M7) will be generated from the cycle-stamped click list the emulation thread produces.

**Hotkeys** (must not collide with the game, which uses A, Z, the arrow keys, Ctrl-S, Esc, Space, digits and Return, see CONTROLS.md): F11 or Alt+Enter = full screen; Ctrl+Plus / Ctrl+Minus = scale up / down; F9 = mute the whole port (the game's own Ctrl-S sound switch is untouched); Alt+F4 = quit. **Esc is NOT quit** (the game uses it to pause). The development viewer `pacman_view` still closes on Esc; that is fine for a viewer only and must not be copied into the game window.

**Keys to the game:** the window thread turns Windows key events into the Apple II keyboard latch ($C000/$C010 behaviour: 7-bit ASCII with bit 7 set, "strobe" cleared on read), including Ctrl-H / Ctrl-U for the arrows (Left = $88, Right = $95), A = $C1, Z = $DA, Ctrl-S = $93, Esc = $9B. Gamepad later as a separate modern option.

**Window behaviour:** fixed client size = 280 x 192 times the scale; the title shows the port's name; the window may be minimised; losing focus does not pause (the original had no such concept) but the emulation clock must not accumulate time while the window is minimised/suspended (the catch-up cap above handles this).

**What is NOT decided:** exact hotkey letters beyond the above, window position memory, a settings file, whether to show a speed/FPS indicator, high-DPI behaviour on mixed-DPI monitors (the viewer declares itself system-DPI-aware).

## New ledger rows
* TASK-028k: window class for the game (not the viewer): hotkeys, full screen, scale change at run time, the page copy under a lock, repaint at refresh; tested with scripted messages. Depends on this design.
* TASK-028l: virtual cycle clock (pacing) as a tested C++ module: paced against a fake clock in unit tests (catch-up cap, no drift), then against the real clock.
* TASK-029 (M4) can start after these two or in parallel with 028l, since the game core only needs "advance by N cycles".

## TASK-028k: the game window (implemented 2026-11-02)
`port/src/app/gamewindow.{h,cpp}` (class `GameWindow`, GDI): fixed windowed size 280x192 times a whole-number scale (1-8); `setScale`; full screen (borderless on the window's monitor, the picture at the largest whole-number scale that fits, centred, black bars; leaving restores the window and the remembered scale); `setPage` (any thread, copies the 8 KB page under a mutex) and a refresh timer at the monitor's refresh rate that repaints only when the page changed (WM_TIMER is coarse, about 15.6 ms: good enough for now, a better source can replace it); `popKey` (any thread; bounded queue of 64) giving Apple II key codes from WM_CHAR (capital letters, `$80 | ASCII`: Ctrl-S `$93`, Esc `$9B`, Space `$A0`, digits `$B0..`, Return `$8D`, Backspace/Ctrl-H `$88`) and from WM_KEYDOWN (Left `$88`, Right `$95`; **Up = `$C1` ('A'), Down = `$DA` ('Z')**, an addition: the Apple II had no up/down arrow keys and the game's keys for those are A and Z).
**Hotkeys** (`handleHotkey`, not forwarded to the game): F11 or Alt+Enter full screen; Ctrl+Plus/Ctrl+Minus scale up/down (also numpad); F9 mute flag (the flag exists, sound comes with M7). Esc, A, Z, arrows, Space, Return, digits, Ctrl-S are never hotkeys; Alt+F4 is left to Windows.
**Tests (V):** `port/tests/test_gamewindow.cpp`, 6 tests: sizes and limits; hotkeys incl. "game keys are not hotkeys"; full screen equals the monitor rectangle, effective scale, remembered scale, Alt+Enter restore (window 560x384 after scale 2); all key codes above, the 7-bit filter and the bounded queue; the page is really painted (PrintWindow of this window: lit block white, elsewhere black) and a new page replaces it; a second thread feeding 300 alternating pages while the window pumps, last page wins. They drive the windows with scripted messages and calls (modifier keys are passed in as arguments, not read from the real keyboard). A deliberate mutation (Up key code changed) made the key test fail, and reverting made it pass. C++ suite: 51 tests, 0 failed, on the `build.bat` build and with the optional vectors, dots and cartoon scripts; the CMake route builds and ctest passes 2 of 2.
**Not done / limits:** the game window is not yet wired to a program (no `main` uses it; the viewer still has its own window code); the real keyboard path (physical Ctrl/Alt state through `GetKeyState`) is not exercised by the tests; Alt+Enter and F11 were tested through `handleHotkey`, not through real key messages; no DPI-change handling; no window-position memory; no speed indicator; the mute flag has no effect yet. Tests briefly show up to two real windows on screen.
