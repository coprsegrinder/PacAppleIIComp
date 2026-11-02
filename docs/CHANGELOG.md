# Changelog

## 2026-10-29
* TASK-029d2: junction arrival, direction acceptance and tunnel flags rebuilt; equals the oracle on 5,468 acting ticks of 18 recordings.
* TASK-029d1b: step timer checked at levels 4-21 (14 new recordings); found and rebuilt a carry quirk at level >= 15; near-ghost rule from level 5 understood but not yet seen on the oracle.
* TASK-029d1: Pac-Man step timer rebuilt (reload rule, ghost-state terms, dot penalty); equals the oracle on 33,757 recorded ticks. 029d split into d1-d4.
* TASK-029c: input poll rebuilt (keys, Ctrl-S, Esc pause, demo Space, game-port switches); equals the oracle on 33,819 recorded decisions.
* TASK-029b: GameState with the golden layout (round-trips 33,832 recorded ticks) and the new-game state (equals tick 0 of all four scenarios; some values still oracle literals).
* TASK-029a: per-tick oracle recordings of the four golden scenarios (34,000 ticks) and a C++ reader; chain hashes equal the golden ones.
* TASK-029: M4 game core split into 029a-029i with a plan (docs/M4_PLAN.md).
* TASK-028l: virtual cycle clock (exact, drift-free, 100 ms catch-up cap, suspend/resume) with 9 tests.
* TASK-028k: game window class (scale, full screen, hotkeys, key codes, page copy from another thread) with 6 tests.
* TASK-028j: window, pacing and input design decided (docs/WINDOW_DESIGN.md): square pixels, virtual cycle clock, hotkeys only, GDI for now.
* TASK-028i: CMake route verified (all targets build, ctest passes, CMake-built viewer passes the window tests).
* TASK-028h: cartoons 1-3 replay exactly in C++ (vertical cartoon lines added); window shows a mid-cartoon picture (`--calls N`), pixel-exact.
* TASK-028g1: replay library; `pacman_view --script` shows a replayed original screen (0 differing pixels vs the original final page).
* TASK-028f: C++ replay of 29,422 recorded drawing calls (cartoon 1 run) reaches the original final page hash.
* TASK-028e: `pacman_view --page text` shows the 12 attract strings; window pixels equal the independent text model.
* TASK-028d: C++ dot pass (`drawDots`) equals 3 real oracle runs; the window now shows maze + dots + energizers (0 differing pixels vs an independent model).
* TASK-028c: `pacman_view --pack` shows the real maze from the local data pack; window pixels equal the independent model (0 of 483,840 differ).
* TASK-028b: Windows app window (GDI) showing a hi-res page at an integer scale; `pacman_view.exe`; window-only screenshot test compares every pixel (docs/APP_WINDOW.md).
* **Data pack (TASK-026, milestone M1):** `pacman_pack build/info`, a 60-range generated manifest (16,680 data bytes), loader that refuses any damaged or wrong-game pack (27 C++ tests).
* TASK-028a: C++ page-to-colour function with tests (class counts equal the verified Python model).
* TASK-027b3: the 3 failing vectors were a capture fault; 149 verified shifted-image pairs; all 1,891 C++ hires vectors pass.
* TASK-027 part 2 (partly): image dispatcher, big sprite and shifted image in C++; 1,741 real-call vectors byte-exact; 3 suspect shifted-image vectors open.
* TASK-027 part 1: C++ hires layer (page, lines, walls in both modes, blitters, clears, dots, tiles, text printer) with tests; walls and text match the verified models.
* TASK-031f: cartoon open questions answered (key during a cartoon is discarded, each player gets cartoons, frame tables identified).
* TASK-031e: cartoon 3 decoded and modelled exactly (814 calls); all three intermissions now have reference models.
* TASK-031d: cartoon 2 decoded and modelled exactly (482 drawing calls); golden cartoon chains now include line calls.
* TASK-031c: cartoon schedule verified, cartoon 1 modelled exactly, golden cartoon data (docs/CARTOONS.md); corrected the box-clear table (6 entries).
* TASK-034a: golden gameplay traces (4 scenarios, tick-keyed inputs, logical-state hash chain; docs/GOLDEN_TRACES.md).
* TASK-027e: hi-res colour model checked against an AppleWin screenshot (100% of 2,172 isolated pixels agree after fixing my sampling offset; the first 97.3% and its filter explanation were wrong).
* TASK-027d: wall routine and text printer modelled and verified against the oracle (including the forced $1D = 0 mode); drawing tables listed.
* TASK-027c: hi-res addressing and pixel model verified, colour model written, 22 golden screen hashes (docs/GRAPHICS.md).
* TASK-027b: all hi-res writers decoded, cartoon big-sprite routine found (docs/GRAPHICS.md).
* TASK-027a: inventory of screen writers and video switches (docs/GRAPHICS.md).
* TASK-021d: 100% of the static code map executed; the last 38 bytes under forced state.
* TASK-021c: Space in the attract delay loop and the one-digit rank prompt executed; the four carry fix-ups shown dead in practice; 38 unexecuted bytes left, none reachable.
* TASK-021b: five unexecuted runs proven dead, two more executed; corrected the sound labels (the long sound is the ghost-eaten sound).
* Unexecuted code (TASK-021): static-code coverage raised from 87.5% to 99.3%; the keyboard mode also reads a 4-switch digital joystick (`docs/UNEXECUTED_CODE.md`).
* Level tables and fruit (TASK-031 analysis): 15 level-indexed tables, fruit schedule, lifetimes, scores and sound (`docs/LEVEL_TABLES.md`, `tools/level_tables.py`, `tools/trace_fruit.py`).
* Sound analysis (TASK-032a): two tone engines, 12 note lists and a cycle-exact click-time model (`docs/SOUND.md`, `tools/sound_model.py`, `tools/trace_sound.py`).
* Ghost decision rules (TASK-030b): complete per-routine tables checked against the original code; the personalities depend on Pac-Man's direction (`docs/GHOST_RULES.md`, `tools/ghost_rules.py`).
* Pac-Man side analysis (TASK-031a): dot and energizer scoring, lives, bonus life, ghost-eating score, Pac-Man speed rule, turning (read), collision rules (`docs/PACMAN_RULES.md`, `tools/trace_pacman.py`).
* **Port skeleton (TASK-025, milestone M0):** `port/` with CMake and `build.bat`, a dependency-free test runner, SHA-256, a bounds-checked DOS 3.3 reader and the verification rule in C++ (16 tests; same answers as `tools/verify_image.py` on 7 of 8 comparison inputs, the 8th differs only in operating-system error wording). Project documents added (README, BUILDING, USER_GUIDE, CHANGELOG, KNOWN_ISSUES, LEGAL_AND_ATTRIBUTION).
* Port plan and architecture written; user decisions D1-A (local data pack) and D2-A (hand reconstruction checked against the oracle); constraint: the final result is based only on the supplied `.dsk`.
* Verifier rule confirmed by the user; release identity reconciled (Atari 1983); boot tracks found to be a modified DOS; gameplay memory compared with AppleWin.
* Earlier the same day: disassembler, 6502 core and Apple II model, coverage and rendering tools, control and table analysis (see `docs/TASK_LEDGER.md`).
* Correction: the first "read as data" statistics were inflated by the startup stub's own reads; recomputed (`docs/DATA_REGIONS.md`).
