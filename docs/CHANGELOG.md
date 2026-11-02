# Changelog

## 2026-10-29
* TASK-029g9: fruit eaten/erased, death and level-end draw sequences derived (equal to the oracle logs), level icon to level 7; scenarios A-D, F, G, H, J exact in calls and pages.
* TASK-029g8: white flash frame verified, fruit drawn, rebuilt screens after a death and after a level end exact; scenarios A (2,090 ticks), B, C (700), D (1,600), F (700) equal the oracle in calls and pages.
* TASK-029g7: ghost-eaten sequence, blue frame, respawn and energizer erase rebuilt; scenarios A, B, C, D, F: drawing calls and pages equal the oracle (B and C to the end of level 1).
* TASK-029g6: door palette rule understood; 1UP and energizer blinks rebuilt; scenario A exact in calls and 70 pages with nothing excluded; scenario D exact up to tick 465.
* TASK-029g5: score display, ghost dot redraw and door redraw; 700 ticks of scenario A: drawing calls and 70 pages equal the oracle (except the two blinks).
* TASK-029g4 (part 1): per-tick sprite calls (erase, image, blocked redraw, ghost image choice) equal the oracle log for 700 ticks; pages exact to tick 357.
* TASK-029g2/g3: score area, labels, dotted lines and icons drawn; the whole first play page equals the oracle (0 of 8,192 bytes differ).
* TASK-029g1: first play screen (walls, door, dots, Pac-Man) drawn from the state; equals the oracle page except score, text block and icons.
* TASK-029f2: start of a life rebuilt from the pack tables; the new game no longer uses oracle literals (except $78/$79); the state after every catch equals the oracle.
* TASK-029f1: ghost contact, ghost eaten, lives lost and the extra life at 10,000 points rebuilt; equal to the oracle in 9 catches, 4 eaten ghosts and 1 extra life.
* TASK-029d2d: natural tunnel recording (no pokes, 4 wraps) after fixing the controller; poked tunnel recording re-recorded with two identical replays.
* TASK-029d2c/029d4: demo stream in the C++ core (pointer equals the original on 4,000 ticks); natural tunnel recording; item test and eating rebuilt (6,294 acting ticks match).
* TASK-029d2b: tunnel (185 ticks, 8 wraps) and attract-demo recordings; Pac-Man step, tunnel transit and cell counters rebuilt; 5,657 acting ticks match.
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

- TASK-029g10 (partly): two-player first screen, scenarios K/L/M/P; player switch after a death still open.

- TASK-029g11: two-player play and player switch equal to the oracle (scenario P, 2,299 ticks).

- Door redraw low-bit OR: proven reached (D, K) and proven to matter (mutation check).

- TASK-029g13 (part): scenario Q equal for 4,699 ticks (player 2 death + switch back); R recorded, not yet usable.

- TASK-029g13: scenario R (5,567 ticks) equal; GAME OVER sequence (21 calls) equal.

- TASK-029g14: player game-ending rule observed in both orders (scenarios R, S), all equal.

- TASK-029h0: apple2tc-master (user-provided, MIT) built locally with MSYS2 gcc; sample decompiles. Plan 029h1-029h5.

- TASK-029h1: tools/make_b33.py; image runs in a2run, game area equals the oracle at $4027.

- TASK-029h2 (part): own collector for apple2tc run data (a2tc_collect/scenarios/merge/validate), A-D recorded and validated against a2run on the game area.

- TASK-029h2b: run data of 30 scenarios, replays equal to the recordings; merged file 1,111 targets.

- TASK-029h3: apple2tc asm/C/coverage for PAC-MAN; code set equals ours except 76 never-run bytes.

- apple2tc method scrapped by the user; 029h0-h5 marked abandoned, tools removed. Back to the M4 plan (original 029h = integration).

- Full-suite checkpoint (2026-11-02): C++ 102/0 with all inputs, Python 40 files/172 tests OK. New --drawticks flag and tools/full_suite.sh.

- TASK-029g15: high score ($00-$02) survives into the next game; second-game first page equals the oracle (test added).

- TASK-029g16: player 2 level end equal to the oracle (scenario U).

- TASK-029h: GameCore/GameDriver; 53,827 ticks free-running equal the oracle (ghosts fed); pages equal end to end in 11 scenarios; first playable program pacman_play.exe (no ghosts yet). D5: 4,870 cycles per tick.

- TASK-030e planned (docs/M5_PLAN.md, rows 030e1-e9); 030e1 recorder done for A-D.

- TASK-030e3: ghost tick head (countdown, frightened timer, speed tables) equals the oracle on 33,796 ticks.

- TASK-030e4: ghost decision routines (generated C++ transliteration, 96,000 oracle vectors), junction detection and move queue equal the oracle. Decision D6.

- TASK-030e5/e6 (+ house move): special junctions, ghost move, tunnel, house release equal the oracle on 198,588 ghost records of 21 recordings.

- TASK-030e7: corner/chase countdown and reversal, energizer and eaten-ghost timer effects, life-start queues equal the oracle.

- TASK-030e9 / M5 done: ghost logic inside GameCore; 54,428 ticks free-running equal the oracle; pacman_play.exe has moving ghosts. dist/ rebuilt.
