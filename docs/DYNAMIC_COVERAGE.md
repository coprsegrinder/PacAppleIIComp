# Dynamic Coverage (TASK-013c, part 1)

Tools: `tools/trace_coverage.py` (runs the extracted game headless with coverage), `tools/cov_report.py` (summary vs. static disassembly).
Commands: `python -I tools/trace_coverage.py 1N 80000000` then `python -I tools/cov_report.py` (~2 min). Raw output stays in git-ignored `scratch/` (cov.json, cov_detail.txt).
Method: the Python 6502 core + Apple II stubs (tools/apple2.py) run the program from the startup stub; keys `1` (keyboard) and `N` (normal keyboard) are typed at the two menus;
then the game runs freely for 80M instructions = 242,947,502 cycles (~4 min of Apple time at 1.023 MHz). No keys or joystick input after that.

## Result (V: measured by the headless run; not yet cross-checked against AppleWin for this scenario)
| Metric (game area $4000-$A9FF, 27,136 bytes) | Value |
|---|---|
| Bytes executed | 6,724 (3,340 instruction starts) |
| ...of which already found by static analysis | 5,693 |
| ...of which NOT found statically | **1,031 bytes in 84 runs, e.g. $8E1E-$96E7+** (list: scratch/cov_detail.txt) |
| Statically reached but not executed in this run | 2,513 (other game states: gameplay, death, sound, etc.) |
| Bytes read as data (not executed) | 9,687 in 364 runs (corrected 2026-10-29; first reported 15,149) |
| Bytes written by the game itself (after relocation) | 947 |
| Neither executed, read nor written in this run | 10,723 (39.5%) (corrected 2026-10-29; first reported 5,263 / 19.4%) |

Combined with the static result, executed + static-reached code is well over the 30% from TASK-005a, but "executed in one scenario" is not "all code": gameplay and sound paths have not run.

## Findings
1. **Self-modifying code is real but small (V).** Only 19 executed bytes (11 runs) are rewritten by the game at runtime (relocation excluded); all but one are instruction *operands*. Examples:
   `$55B2-$55B3` (by $6A62/$6A65), `$6951/$695A` (by $61B8-$61C3), `$69B8`, `$6A4B`, `$6EB8`, `$6F00`, `$4876` (by $4882: the screen-clear loop's page, restored afterwards).
2. **`JSR $0666` at $8CA6 is a patched call (V).** The file holds `20 66 06` but the game rewrites the operand ($8CA7-$8CA8, by $8C7E/$8C84) before it runs; at that point it is `JSR $901A`.
   So $0666 in the static listing is a **placeholder, not the text-page-table theory** from earlier notes (that theory is withdrawn for this call). The called code lives at $901A+, in the region the static pass never reached.
   Static analysis therefore missed code reached through patched JSR/JMP operands: a whole region ($8E1E-$96E7 and beyond) is only reachable that way.
3. **Writers of the runtime table $623A-$65B9 (V):** code at $5345 and $7B2A (previously unknown).
4. **The game runs by itself after the menus (V for what ran, U for what screen).** After `1`,`N` it enters hi-res graphics (text off, hires on, page 1) and never waits for a key through RDKEY again.
   It makes no speaker clicks in 4 minutes and does not execute the paddle ($6FD6), button ($7028) or the 2nd keyboard-poll ($6F9A) code; it does poll $C000 ($5292, $6F77).
   What is on screen (title? maze build? waiting for a key?) is **Unknown**: needs a rendered frame or an AppleWin comparison. Possibly it waits for a keypress or button to start.
5. **Stubs were enough (V):** no unknown soft-switch access and no unmodelled ROM call in 243M cycles.

## Not done yet (TASK-013c part 2)
Cross-check this scenario against AppleWin (same keys, a memory dump at a comparable point); render a frame to see what screen the game is on; feed more input (a key / button press) to reach gameplay, then re-run coverage.

# Gameplay run (TASK-017, headless only)

Tool: `tools/play.py` (scripted session: keys, run N instructions, screenshot, text page, info; `--cov` writes scratch/cov_play.json). Command used:
`python -I tools/play.py --cov key:1 key:cr 70_000_000 key:sp 5_000_000 key:1 5_000_000 key:cr 3_000_000 info shot:g1.png 10_000_000 info shot:g2.png 30_000_000 info shot:g3.png`
## Input flow (V: seen on the headless text page / screens)
`1`,Enter at the mode menus -> attract loop (maze draw, GAME OVER, Character/Nickname screen, "Press Space bar") -> **space** -> text screen "HOW MANY PLAYERS? (1 - 5)" (waits in RDKEY) -> `1` ->
"PRESS <RETURN> TO PLAY / OR <SPACE> TO GO BACK TO DEMO." (RDKEY) -> **Enter** -> the game starts.
## Result (V for measurements; descriptions of screens I)
* Sound: first speaker clicks appear now (**2,212 clicks** within ~8M cycles of starting play, 2,696 and 3,192 at later checkpoints), so the intro jingle/gameplay sounds run; attract is silent.
* Screens: the maze with "PLAYER #1" score 70 (score rose with no key presses, so the game ran its own movement: U whether Pac-Man keeps moving by itself), a spare-life icon and the cherry; later the full-screen "PACMAN" logo (title/after game over, U).
* Coverage: **7,451** game bytes executed in total, **727 new** versus the 243M-cycle attract run (6,724). **All 7,451 lie inside the static code map from TASK-013d: 0 contradictions** (strong evidence the static classification is right). 3,005 of the 10,456 static code bytes have still not run (death, levels, fruit, joystick, keyboard customisation, bonus screens: U).
* No unknown soft-switch access and no unmodelled ROM call in ~337M cycles.
## Not done
No keyboard-direction input was tried (controls not yet identified), the joystick path ('2'), nor an AppleWin gameplay comparison (only the attract screens were compared in TASK-016). Speaker click timings are logged in memory only (not saved).

# Rarely used paths (TASK-020, headless)
Tool: `tools/explore.py` (scenarios `cartoons`, `rank`, `custom`; each from a fresh boot with coverage), merged with `tools/cov_union.py`. Hidden menu and customised keyboard: docs/CONTROLS.md.
| Run | Executed bytes | New vs earlier runs |
|---|---|---|
| attract (243M cycles) | 6,724 | - |
| play, no steering | 7,451 | +727 |
| auto-play keyboard (2 games) | 7,818 | +379 |
| auto-play joystick (2 games) | 7,726 | +219 |
| intermissions 1-3 via hidden menu | 7,621 | +837 |
| start at rank 21 + random play | 7,571 | +80 |
| customised keyboard + Esc pause | 7,661 | +181 |
**Union: 9,147 executed bytes = 87.5% of the 10,456 static code bytes; 0 executed bytes outside the static code map.** No unknown soft-switch access in any run. Intermissions made no speaker clicks (V: 0 clicks during the three cartoons).
Not run: the final 1,309 bytes in 125 small runs (largest 76 bytes).

> Correction note: the read-based rows of the first table were recomputed after discarding the startup stub's own reads (see DATA_REGIONS.md, Correction). Execution and write results were not affected.
