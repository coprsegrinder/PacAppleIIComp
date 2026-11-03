# M6 plan: levels, fruit, intermissions (TASK-031, written 2026-11-03)

Goal: make `GameCore` play beyond level 1: the level end, the next level's set-up from the level tables, the bonus fruit as game logic, and the three cartoons. Same method as M4/M5: rebuild from the original code, test piece by piece against the oracle (the original on our headless machine), then free-running against recordings.

## What exists already (from the ledger/headers; each sub-task re-checks the code first)
* Analysis done: docs/LEVEL_TABLES.md (15 level tables, fruit rules), docs/CARTOONS.md (schedule, all three cartoons modelled, golden data), docs/PACMAN_RULES.md, docs/GHOST_RULES.md.
* C++ done: drawing of fruit, death and level-end sequences (029g8/g9), `fruitTable_`/`fruitPoints_` arrays and `fruitEatenCheck()/fruit()` declared in `gamecore.h` (header says the bonus fruit logic is "not rebuilt yet; the caller may supply the bytes"), hires replay of cartoons 1-3 (028f-h), `startOfLife` and new-game set-up (029f2).
* Not done: level-end detection and the level change as game logic, per-level tables driving speeds/timers, the fruit logic without fed bytes, cartoons inside the game flow.

## Sub-tasks (each one turn, <= 15 min planned; split again if it overruns)
| Row | Content | Test (against the oracle) |
|---|---|---|
| 031-1 | Recorder: zero page + level bytes across a level end and the first ticks of the next level (levels 1->2, 4->5, 20->21; level end forced by poke) | recordings replay equal on the headless machine |
| 031-2 | Level-end detection (items left = 0) and the end-of-level flow up to the cartoon scheduler `$8155` (flash, rebuild) | state bytes equal at each tick of the recordings |
| 031-3 | Level advance: `$55`+1, maze reload (`$531E`, `$5659`), dots/items reset, `$57` fruit counter, life-start queues | state + page equal at the first tick of the new level |
| 031-4 | Per-level tables in `GameCore` (speed rule, frightened time, mode timings, ghost delays, collision rule from level 5) | 30 levels: derived values equal the oracle's |
| 031-5 | Bonus fruit logic: counter 64/96, kind by level, lifetime, pick-up spot (`$E7` side data), score, 48-tick display; level 23 "HAL" sprite | free-running equal on fruit recordings; levels 1, 5, 23, 26 |
| 031-6 | Cartoon trigger (levels mod 13 = 2, 5, 9) and cartoon 1 in the game flow (replay layer exists) | page hashes equal golden data |
| 031-7 | Cartoons 2 and 3 in the flow; key press during a cartoon is discarded (CARTOONS.md 7.1); per-player level count (7.2) | same |
| 031-8 | Integration: pacman_play.exe plays levels 1-N with fruit and cartoons; free-running multi-level recording equal | full-suite checkpoint |

Order is a recommendation: 031-1 first (without recordings nothing else can be tested). Small open items kept out of M6: READY pause, death animation, title/attract (029i), two-player core, sound (M7), pacing (M8).

## Risks
* Level 23 HAL sprite may be a table slip; port it as the original does (faithful default), document in KNOWN_ISSUES.
* The "near-ghost +1" rule at `$8B34` was never seen on the oracle (029d1b): may need a forced test.
* Real per-level speeds need the cycle model (M8); in M6 only tick counts are compared.

## 031-3 findings so far (2026-11-03, partial; no code changed)
Oracle state change across each of the 11 level ends in B, C, E04, E05, E09, E20, H, L, M, U (state bytes before the last-item tick vs the first tick of the new level):
* zero page: `$55` level+1; `$56` = $F8 (248); `$57` fruit counter = $40; `$5E` = 0 (energizer blink); `$60/$61` Pac-Man to $8B/$8F (only the bytes that differ); `$62`/`$63` = 3 (direction); `$64/$65` = $2C/$2D; `$66` = 0; `$6A` = $0D; `$6C` = 2; `$6B` = $17 where it differed; the score `$52-$54` is the old score plus the last item (not reset).
* 20-28 ghost bytes differ (start-of-life values), the maze map is the pristine copy (differences = the few cells eaten before).
* Plan for the test: after `levelComplete`, apply `startNextLevelUnverified()` to the core and compare with the oracle's next state (all bytes except 30/31); print the differing bytes, fix the stand-in until 0 differ, then rename it. The test hook was written but not committed: see the blocker below.
* BLOCKER (same compile error three times, rule "never a third variation"): an edit of tests/test_gamecore.cpp that inserted a printf kept ending up with a newline inside the string literal ("error C2001", lines 160/161). The edit was reverted (git checkout). Next time insert the hook with the Edit tool (not a Python/sed replace) and check the lines before building.
