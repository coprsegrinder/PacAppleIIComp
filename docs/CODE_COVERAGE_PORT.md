# C++ line coverage of the port

Made by `tools/coverage_cpp.sh` (runs the whole test suite with every optional recording under `tools/linecov`, a small debugger-based runner) and `tools/coverage_report.py`. Microsoft dotnet-coverage was tried first and cannot measure a native program (its native instrumentation is a Visual Studio component). A **line with code** is a line that the PDB lists; it is **covered** when any instruction of it ran during the test run. Line coverage says a line ran, not that its result was checked: the equality tests against the oracle are what check results.

**Total: 10388 of 10938 lines with code ran (95.0 %).**

## Game logic (src/core): 6478 of 6571 lines (98.6 %)

| File | Lines | Ran | % |
|---|---|---|---|
| `core/achievementfacts.h` | 22 | 22 | 100.0 |
| `core/achievements.cpp` | 98 | 98 | 100.0 |
| `core/achievements.h` | 3 | 3 | 100.0 |
| `core/attract.cpp` | 1926 | 1886 | 97.9 |
| `core/attract.h` | 12 | 12 | 100.0 |
| `core/attractflow.cpp` | 516 | 508 | 98.4 |
| `core/attractflow.h` | 32 | 32 | 100.0 |
| `core/cartoon.cpp` | 198 | 196 | 99.0 |
| `core/cartoon.h` | 1 | 1 | 100.0 |
| `core/cpu6502.cpp` | 223 | 219 | 98.2 |
| `core/cpu6502.h` | 13 | 13 | 100.0 |
| `core/cycleclock.cpp` | 51 | 51 | 100.0 |
| `core/cycleclock.h` | 12 | 12 | 100.0 |
| `core/deathtiming.cpp` | 20 | 20 | 100.0 |
| `core/eating.cpp` | 43 | 43 | 100.0 |
| `core/gamecore.cpp` | 272 | 272 | 100.0 |
| `core/gamecore.h` | 20 | 20 | 100.0 |
| `core/gamedriver.cpp` | 227 | 219 | 96.5 |
| `core/gamedriver.h` | 23 | 22 | 95.7 |
| `core/gamestate.cpp` | 27 | 27 | 100.0 |
| `core/gamestate.h` | 21 | 21 | 100.0 |
| `core/ghostdecide_gen.cpp` | 1211 | 1203 | 99.3 |
| `core/ghostjunction.cpp` | 203 | 203 | 100.0 |
| `core/ghostmode.cpp` | 61 | 61 | 100.0 |
| `core/ghosttick.cpp` | 37 | 37 | 100.0 |
| `core/input.cpp` | 145 | 145 | 100.0 |
| `core/input.h` | 3 | 3 | 100.0 |
| `core/junction.cpp` | 64 | 63 | 98.4 |
| `core/junction.h` | 9 | 9 | 100.0 |
| `core/lifestart.cpp` | 45 | 45 | 100.0 |
| `core/lives.cpp` | 42 | 42 | 100.0 |
| `core/monitor.cpp` | 146 | 141 | 96.6 |
| `core/monitor.h` | 18 | 18 | 100.0 |
| `core/pacstep.cpp` | 67 | 67 | 100.0 |
| `core/screen.cpp` | 86 | 86 | 100.0 |
| `core/sound.cpp` | 82 | 81 | 98.8 |
| `core/sound.h` | 2 | 2 | 100.0 |
| `core/squarewave.cpp` | 65 | 65 | 100.0 |
| `core/steptimer.cpp` | 50 | 50 | 100.0 |
| `core/steptimer.h` | 2 | 2 | 100.0 |
| `core/textscreen.cpp` | 57 | 49 | 86.0 |
| `core/textscreen.h` | 9 | 9 | 100.0 |
| `core/tickdraw.cpp` | 313 | 313 | 100.0 |
| `core/tickdraw.h` | 2 | 2 | 100.0 |
| `core/ticktrace.cpp` | 87 | 80 | 92.0 |
| `core/ticktrace.h` | 5 | 5 | 100.0 |

## Windows program (src/app): 3204 of 3637 lines (88.1 %)

| File | Lines | Ran | % |
|---|---|---|---|
| `app/achievementbook.cpp` | 118 | 105 | 89.0 |
| `app/achievementbook.h` | 4 | 4 | 100.0 |
| `app/audio.cpp` | 130 | 119 | 91.5 |
| `app/audio.h` | 1 | 1 | 100.0 |
| `app/background.h` | 28 | 26 | 92.9 |
| `app/background_art.cpp` | 192 | 192 | 100.0 |
| `app/colourmode.h` | 17 | 17 | 100.0 |
| `app/crtfilter.cpp` | 173 | 167 | 96.5 |
| `app/crtfilter.h` | 3 | 3 | 100.0 |
| `app/d3drenderer.cpp` | 365 | 360 | 98.6 |
| `app/display_layout.h` | 17 | 17 | 100.0 |
| `app/fpsmode.h` | 41 | 36 | 87.8 |
| `app/framed_layout.h` | 66 | 65 | 98.5 |
| `app/gamepad.cpp` | 455 | 181 | 39.8 |
| `app/gamewindow.cpp` | 1159 | 1110 | 95.8 |
| `app/gamewindow.h` | 87 | 87 | 100.0 |
| `app/glow.h` | 84 | 84 | 100.0 |
| `app/motion.h` | 37 | 37 | 100.0 |
| `app/pacing.h` | 15 | 15 | 100.0 |
| `app/pausegate.h` | 12 | 12 | 100.0 |
| `app/pausemenu.cpp` | 377 | 322 | 85.4 |
| `app/pausemenu.h` | 7 | 7 | 100.0 |
| `app/precisesleep.h` | 21 | 19 | 90.5 |
| `app/resolutions.h` | 17 | 17 | 100.0 |
| `app/settings.cpp` | 158 | 153 | 96.8 |
| `app/settings.h` | 1 | 1 | 100.0 |
| `app/vibration.h` | 52 | 47 | 90.4 |

## Hi-res drawing (src/hires): 280 of 282 lines (99.3 %)

| File | Lines | Ran | % |
|---|---|---|---|
| `hires/hires.cpp` | 203 | 203 | 100.0 |
| `hires/hires.h` | 15 | 15 | 100.0 |
| `hires/replay.cpp` | 64 | 62 | 96.9 |

## Colour and monochrome picture (src/video): 116 of 132 lines (87.9 %)

| File | Lines | Ran | % |
|---|---|---|---|
| `video/colour.cpp` | 40 | 40 | 100.0 |
| `video/colour.h` | 1 | 1 | 100.0 |
| `video/monochrome.cpp` | 75 | 75 | 100.0 |
| `video/screens.cpp` | 16 | 0 | 0.0 |

## Data pack (src/datapack): 106 of 108 lines (98.1 %)

| File | Lines | Ran | % |
|---|---|---|---|
| `datapack/datapack.cpp` | 105 | 103 | 98.1 |
| `datapack/datapack.h` | 2 | 2 | 100.0 |
| `datapack/manifest.cpp` | 1 | 1 | 100.0 |

## Disk image check (src/verify): 204 of 208 lines (98.1 %)

| File | Lines | Ran | % |
|---|---|---|---|
| `verify/dos33.cpp` | 68 | 68 | 100.0 |
| `verify/sha256.cpp` | 68 | 68 | 100.0 |
| `verify/verify.cpp` | 71 | 67 | 94.4 |
| `verify/verify.h` | 1 | 1 | 100.0 |

## Lines that did not run

* `app/achievementbook.cpp` (13 of 118 lines not run): 24-30, 60, 86-88, 92-93
* `app/audio.cpp` (11 of 130 lines not run): 43-44, 107-108, 147-153
* `app/background.h` (2 of 28 lines not run): 20, 30
* `app/crtfilter.cpp` (6 of 173 lines not run): 25, 38, 73-77
* `app/d3drenderer.cpp` (5 of 365 lines not run): 203-204, 211, 470-471
* `app/fpsmode.h` (5 of 41 lines not run): 28, 38-40, 49, 66
* `app/framed_layout.h` (1 of 66 lines not run): 26
* `app/gamepad.cpp` (274 of 455 lines not run): 66, 235-415, 428-543, 550, 562-565, 569, 600-609, 617-618
* `app/gamewindow.cpp` (49 of 1159 lines not run): 248-251, 432, 490-495, 499-503, 640-642, 711-716, 769, 790, 796-797, 808-809, 898, 1122, 1163, 1260, 1277-1282, 1287-1288, 1337-1340, 1396-1397, 1407
* `app/pausemenu.cpp` (55 of 377 lines not run): 43-58, 150, 260, 302, 383-396, 405-417, 440-441, 455-469
* `app/precisesleep.h` (2 of 21 lines not run): 30-31
* `app/settings.cpp` (5 of 158 lines not run): 23, 31, 200-202
* `app/vibration.h` (5 of 52 lines not run): 23, 29-33, 66
* `core/attract.cpp` (40 of 1926 lines not run): 425-426, 442, 641-656, 665-667, 1053, 1097, 1215, 1260-1263, 1324-1325, 1329-1330, 1339, 2041-2043, 2130-2133
* `core/attractflow.cpp` (8 of 516 lines not run): 64-68, 396-397, 614
* `core/cartoon.cpp` (2 of 198 lines not run): 221-222
* `core/cpu6502.cpp` (4 of 223 lines not run): 21, 26, 169, 278
* `core/gamedriver.cpp` (8 of 227 lines not run): 40-47
* `core/gamedriver.h` (1 of 23 lines not run): 18
* `core/ghostdecide_gen.cpp` (8 of 1211 lines not run): 824, 859, 1135-1138, 1219-1221
* `core/junction.cpp` (1 of 64 lines not run): 40
* `core/monitor.cpp` (5 of 146 lines not run): 51, 58-59, 77-78
* `core/sound.cpp` (1 of 82 lines not run): 80
* `core/textscreen.cpp` (8 of 57 lines not run): 14-16, 20-27
* `core/ticktrace.cpp` (7 of 87 lines not run): 49-50, 56-57, 69-71
* `datapack/datapack.cpp` (2 of 105 lines not run): 73-74
* `hires/replay.cpp` (2 of 64 lines not run): 44, 81
* `verify/verify.cpp` (4 of 71 lines not run): 24-25, 29, 113
* `video/screens.cpp` (16 of 16 lines not run): 9-25
