# App window (TASK-028b, 2026-11-02)

**What:** `port/src/app/pagewindow.{h,cpp}` opens a plain Windows window (GDI, first version; Direct3D 11 can replace it later as a separate step) that shows one hi-res `Page` in colour through `video::renderArgb`. The picture is scaled by a whole number (1-8, default 3) with `StretchDIBits` in COLORONCOLOR mode, i.e. exact pixel replication, no smoothing. The window has a fixed size (chosen at start), closes with Esc or the close button, and the process declares itself DPI-aware so an integer scale stays integer. There is no game logic.

**Aspect ratio:** pixels are square (280 x 192 scaled equally). The real Apple II monitor showed 4:3; a 4:3 display option is a later, separate modern/faithful switch (not decided).

**Viewer:** `pacman_view.exe [--scale N] [--seconds N] [--shot out.bmp]` shows a built-in synthetic test page (no original data): isolated pixels in all four palette colours, a white block, and a frame. `--shot` makes the window capture only its own client area with PrintWindow, write a 32-bit BMP and exit (rule 10: never the desktop). Exit code 64 on a bad argument, 2 if the capture failed.

**Test:** `tests/test_window_shot.py` runs the viewer at scale 1 and 3 and compares EVERY pixel of the captured client area with the expected image (V: 0 differing pixels in 53,760 and 483,840). It also checks the bad-argument exit code. Build first with `port\build.bat`. Note: the test briefly shows a window on screen (not activated).

**Found while testing:** my first expected image treated a one-pixel vertical line as white; the colour rule makes it violet/green (no horizontal neighbour). The window was right, the expectation wrong; the expectation was corrected, not the window.

**Not verified:** the CMake route for `pacman_view` (CMake is not on PATH in this session; the CMakeLists entry is untested). Only the `build.bat` route was built (/W4 /WX clean). Not done: showing a real game page (needs a game core), resizing, full screen, HDR/high refresh.
