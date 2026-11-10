# Overview: how the port was made, and how close it is to the original

This is the one-document story of the project. Every number below comes from a note or a test in this repository; the notes (start with [`INDEX.md`](INDEX.md)) say how each was measured and whether it is *Verified*, *Inferred* or *Unknown*.

## What the project is
The 1983 Atari release of **Pac-Man for the Apple II** (a DOS 3.3 disk) was taken apart, understood, and rebuilt as a **native Windows 10/11 program** written in C++. It is not an emulator and it does not read the disk image while playing: the game's logic, timing, drawing and sound were rebuilt routine by routine. The game's *data* (graphics, tables, level data) is never part of this repository; the program builds it, once, from the player's own disk image and refuses any other image.

The project ran from 2026-10-29 to 2026-11-10 (the first and last commits of this repository).

## How it was made
1. **Taking the original apart.** The disk image was read (DOS 3.3, the boot tracks, the loaded program), the code was told apart from the data by recursive disassembly and dispatch tables (10,456 code bytes), and the findings were written down with their certainty (`REVERSE_ENGINEERING.md`, `DATA_REGIONS.md`, `TABLES.md`).
2. **The oracle.** A 6502 core and a model of the Apple II were written in Python, checked against the AppleWin emulator at checkpoints, and used to **run the original program**. From it the project records, per game tick, the state, the drawing calls, the sound clicks and the CPU cycles. These recordings are the ground truth the port is compared with (`GOLDEN_TRACES.md`, `TESTING.md`).
3. **Rebuilding in C++, milestone by milestone.** The game core (Pac-Man's step timer, junctions, eating, scoring, lives), the four ghosts (their modes, timers, decisions and movement, from the original's own tables), levels, fruit and intermissions, the sound, then the whole program flow: boot menus, players prompt, attract loop, one to five players (`M4_PLAN.md` to `M8_PLAN.md`).
4. **Closing the gaps.** Code that only runs in unusual paths (the 38 "forced" bytes), the analogue paddle and joystick routines, the exact cycle cost of every kind of tick, the ghost-eaten pause with its extra ghost steps, the drawing of every special case. Each gap was found by comparing with the oracle, not by reading the code alone (`UNEXECUTED_CODE.md`, `ROUTINE_AUDIT.md`, `FORCED_CODE.md`, `M8_PLAN.md`).
5. **Modernization.** Options a player expects today, added around the game without changing its rules (below).
6. **Packaging.** One zip with the program, the two Apple II ROM files it needs, a readme and notices; the player's disk image is not in it (`RELEASE_NOTES.md`).

The work was recorded as it happened: [`TASK_LEDGER.md`](TASK_LEDGER.md) (every task and its result), [`DECISIONS.md`](DECISIONS.md), [`CHANGELOG.md`](CHANGELOG.md) and the git history.

## How close is it to 1:1?
Short answer: **everything that happens in play, drawing and timing equals the original on every scenario recorded; a short list of rare moments is timed from measurements instead of being counted instruction by instruction.**

| What | How close | Where it is shown |
|---|---|---|
| The program's code | every one of the 10,456 code bytes has a counterpart in the port; 10,386 of them (99.3 %) were executed on the oracle, the other 70 bytes (16 runs) are corner paths, 6 of them proven dead | `ROUTINE_AUDIT.md`, `UNEXECUTED_CODE.md` |
| Game state | the state of every tick equals the oracle's when the port runs free (its own ghosts, its own timers); only at the ticks where a life restarts or a level changes is the port's state replaced by the oracle's, because those set-ups are checked separately | `test_ghostworld`, `GOLDEN_TRACES.md` |
| Time | all 29,494 ordinary game ticks of 41 recordings take exactly the same number of CPU cycles as the original; so do all 7,918 ticks of a recording in which ghosts are eaten in a row (including the ghost-eaten pause with its eyes ghosts); the attract game: 7,892 of 7,893 ticks | `M8_PLAN.md` |
| Drawing | the drawing calls of every tick equal the oracle's log, and the oracle's screens, taken every tenth tick, equal the port's pixel for pixel, on every drawing scenario recorded (keyboard games, poked special cases, the level icons of levels 2 to 26, one to five players, deaths, level ends) | `test_tickdraw`, `test_gamedriver` |
| Sound | the original's sound lists and timing, compared with the oracle's recorded speaker clicks | `SOUND.md` |
| Intermissions, menus, attract loop | rebuilt from the original's flow and compared with the oracle's recordings | `CARTOONS.md`, `ATTRACT_FLOW.md` |
| The 6502 core that runs the Apple II monitor ROM | every documented opcode compared with the core that ran the original | `MODERNIZATION.md` (section TASK-034-CPU) |

**Timed from measurements, not counted:** the tick in which Pac-Man is caught, the end of a level, the bonus-life award, the fruit eaten, the attract game's ghost-eaten tick, and the fixed part of the ghost-eaten pause (20,759 cycles, the same in every recording). The death animation's frames carry a page-crossing residue of up to 66 cycles.

**Not established:** which revision of the game the disk image is, whether its DOS is Atari's, and any comparison against real hardware: the oracle was compared with an emulator at checkpoints only, and there is no frame-synchronised or audio comparison with a real machine. Xbox (XInput) controller support is implemented from Microsoft's documentation only: nobody has tried a real Xbox controller. The full list is in [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md).

**The test suite:** 287 C++ tests. Many need the disk image and the recordings (see below); the line coverage of the port's own C++ was about 95 % when last measured (`CODE_COVERAGE_PORT.md`).

## Modernization (what is added around the original)
All of it is optional and none of it changes the game's rules. Details and how each was checked: [`MODERNIZATION.md`](MODERNIZATION.md).

* **Display:** Direct3D 11 renderer; borderless full screen or a resizable window; whole-number scaling of the 280 x 192 picture at any resolution; optional window frame.
* **Pause menu:** Esc opens it (it replaces the original's Esc pause); keyboard, mouse and controller.
* **Backgrounds:** black, an ambient glow that follows the picture, an Apple II monitor with its bezel, or the Apple II desktop (a movable, resizable window).
* **Colour:** the original's colour (solid colour for the every-second-dot patterns) or **Native Black & White**, a green-phosphor monitor with the half-dot positions of the real thing.
* **CRT filter:** scanlines, softness, transition, glow, colour blend, curvature, afterglow, brightness and contrast, with presets.
* **Controls:** keyboard, Xbox (XInput, in theory only), DualShock 4 and DualSense (USB or Bluetooth). Controllers steer through the original's own paddle routines.
* **Vibration** (Off / Low / Medium / High) on game events.
* **Light bar** on the DualShock 4 and DualSense, **in the Apple II screen's own colours**: white in play (Pac-Man is white), blue while the ghosts are blue, a flash in the ghost's colour (orange, violet, white, green) when one is eaten or catches Pac-Man; the DualSense's player LEDs show the lives.
* **Achievements:** the 22 achievements (190 points) of the RetroAchievements set for this game, checked locally; nothing is sent anywhere.
* Settings are kept between runs.

Ideas that were built and then dropped on purpose: exclusive full screen, HDR, VSync and the frame-rate options (the picture rate stays the original's).

## What is not in the repository, and why
* **The game itself** (the disk image, its code, graphics, tables and sounds). The repository holds no run of the original's bytes; the data pack is built on the player's machine.
* **The recordings** (`scratch/`, git-ignored). They are derived from the original program, so they stay out. They can be regenerated from the disk image alone with the Python tools in `tools/`, step by step in [`REGENERATING.md`](REGENERATING.md) (it takes hours, not minutes).
* **Third-party material** used only for checking (the AppleWin emulator, a DOS 3.3 reference disk).

## Rebuilding and checking everything
* Build and run: [`BUILDING.md`](BUILDING.md) and the README. Needs Windows 10/11 and the Visual Studio Build Tools; there are no third-party libraries.
* The tests that need no original data run anywhere; with your disk image they also check the data pack and the real-image comparisons ([`TESTING.md`](TESTING.md)).
* The release zip: `port\release.bat`.

## Licence and legal
The source code of this project is licensed under the **GNU General Public License v3.0** (see [`../LICENSE`](../LICENSE)). The licence covers the code and the notes written for this project; it does not cover the game, the Apple II ROM files, or the screenshots, which show the original game's graphics. It is an unofficial, non-commercial fan project, not affiliated with or endorsed by Atari, Namco, Apple or RetroAchievements. See [`../NOTICES.md`](../NOTICES.md) and [`LEGAL_AND_ATTRIBUTION.md`](LEGAL_AND_ATTRIBUTION.md).
