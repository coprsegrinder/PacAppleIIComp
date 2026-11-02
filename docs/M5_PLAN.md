# M5 plan: the ghosts (TASK-030e, written 2026-11-02)

Goal: replace the *fed* ghost records of `GameCore` (see M4_PLAN.md, TASK-029h) by C++ ghost logic, so that the game plays with moving ghosts. Same method as for Pac-Man: rebuild from the original code, test piece by piece against the oracle (the original running on our headless machine), then free-running against all recordings.

## What the ghost loop is (read from the code, V = verified by running, R = read from the listing)
* `$86EA` (R): for `$E0` = 4, 3, 2, 1: copy the ghost's 20-byte record (`$84 + 20 (n-1)`) into the working registers `$70-$83` (`$86FC`), call the ghost tick `$8763`, copy the registers back (`$8712`). So ghost 4 is processed first, ghost 1 last.
* `$8763` (R), the ghost tick, in this order:
  1. `DEC $80` (the ghost's step countdown): not zero -> return (the common case);
  2. zero: if `$E4` (frightened active): count down the shared 16-bit frightened timer `$E1/$E2` (once per *acting* ghost, not once per tick) and call `$6D49` (end of the frightened period) when it reaches zero;
  3. reload `$80` from the speed tables (`$8723` normal, `$8743` frightened, `$8753` eyes; index = min(level, 15); plus a ghost term; doubled when `$7A`);
  4. on a junction (`$87C0-$87EA`: position equals junction `$73` or `$74`): the decision `$8C50` (rules as data: GHOST_ANALYSIS.md, 48 routines, 030b), the move queue, the next junction, special junctions 23/24/26/29 (`$8813-$8864`: house door, tunnel, `$7A`, `$7B/$7D`);
  5. the move (`$8874-$8957`): erase/draw calls (already rebuilt as drawing: tickdraw), `X/Y +- 2`, sub-step counters `$7B/$7D`, the dot redraw `$895F`;
  6. the contact test `$8B34` (already in GameCore) for ghosts that are not eyes.
* Shared variables outside the ghost records (R): `$E1/$E2` frightened timer, `$E4` frightened active, `$E9` corner/chase phase, `$EA/$EB` mode countdown (decremented by `$808C`, called from the main loop `$7874`), `$68` ghost score step, `$1F`, `$F0/$F1/$F3/$F4`, `$E0`. The ghost move queues are RAM pages **`$0800`, `$0900`, `$0A00`, `$0B00`** (V: the queue pointers `$75/$76` of the four records are always those values) - outside the logical state of the recordings.
* Already known and measured (030a-d, docs GHOST_ANALYSIS / GHOST_MODES / GHOST_RULES): the junction graph, the 48 decision routines and their per-junction table, the mode cycle (corner 160, chase 600, corner 140, ... ticks), the energizer routine `$6D60`, the speed table shapes, the eaten-ghost code, the house release at level 1.

## Test method
The recordings (`scratch/ticks_X.bin`) hold the ghost records and `$78 $79 $E9` at every tick, but not the queue pages and not the shared variables `$E4 $EA $EB $F0...`. Task 030e1 records them: the whole zero page and the four queue pages per tick (`scratch/ghostram_X.bin`, 1,280 bytes per tick). With them each piece is a function (state at tick k) -> (state at tick k+1) restricted to the fields it owns, compared on 30+ recordings; the other fields are copied from the oracle. The last task removes all feeding: the C++ ghost logic free-runs and must equal the oracle's ghost records at every tick (A-D, 50,000 ticks).

## Sub-tasks (ledger rows 030e1 ... 030e9; each ends with its own tests and a commit)
| ID | Piece | Test (oracle) |
|---|---|---|
| 030e1 | Recorder: zero page + queue pages per tick (`tools/record_ghost_ram.py`); consistency with the existing recordings | replay chain hash equals the recording; all records equal in the overlapping fields |
| 030e2 | `GhostWorld`: ghost records + queues + shared variables as C++ state, with the working-register swap `$86FC/$8712`; round trip through the recording | state(k) -> model -> state(k) exact on all ticks |
| 030e3 | The countdown, the frightened timer count and its end `$6D49`, the speed tables | `$80`, `$E1/$E2`, `$E4`, `$78` at k+1 for every ghost, all ticks |
| 030e4 | Junction detection and the decision `$8C50` with the 48 routines (data from the pack/030b) and the queue | `$72-$77`, queue bytes at k+1 on all acting ticks |
| 030e5 | The special junctions (door, tunnel, `$7A`, `$7B/$7D` start values) | the same fields on the ticks that touch them |
| 030e6 | The move itself (`+-2`, sub-step counters, wrap in the tunnel) | `$70/$71`, `$7B-$7D` at k+1 |
| 030e7 | Mode timers `$808C`, the flips and reversals `$80C1`, the energizer routine `$6D60` (ghost side) | `$E9 $EA $EB`, direction/queue changes |
| 030e8 | Eyes, return to the house `$8839`, house exit script `$6D30/$6DA1`, release timeline | the eaten-ghost scenario D, house exits at level 1 and higher levels |
| 030e9 | Integration: `GhostSource` implementation inside `GameCore`; no fed fields | free-running A-D (+ special scenarios): ghost records, scores, lives and pages equal at every tick; `pacman_play.exe` with moving ghosts |

## Honest risks
* The per-tick call order inside `$8763` interleaves drawing with logic; the logic is separated from the drawing already (tickdraw), but a drawing call that happens BEFORE a position change (the erase) must keep seeing the old position - the harness compares states, the end-to-end page test (029h) guards the drawing.
* `$7A`, `$7B/$7C/$7D` and the unknown bytes of the record (b[13..15], b[18], b[19]) are not understood semantically (030c, section 6); they will be reproduced from the code, not interpreted.
* Never-executed decision routines (10, 12, 13, 39, 41, 45, 46, 47) are known only from standalone runs.
* The frightened-duration table has an `$FFFF` entry whose meaning is unknown; it is data and is copied as it is.

## 030e1 result (done 2026-11-02): what really changes outside the logical state
Recorded for A, B, C, D (`scratch/ghostram_X.bin`: 8,090 / 4,610 / 6,195 / 14,937 ticks); each replay reproduces its recording's chain hash and every one of the 33,832 states equals the recording (V). In scenario A the zero-page bytes OUTSIDE the logical state that change are: the ghost working registers `$70-$74 $77 $7B-$7E $80 $81` (leftovers of the last ghost processed: copies of a record), the shared ghost variables `$E1 $E2` (frightened timer; changes in 342 ticks), `$E4` (2), `$EA $EB` (mode countdown; 848 / 8), `$F0 $F3` (`$F3` = Pac-Man's post-eating delay, already in `StepTimer`), the blink variables `$E5 $E6` (already in `GameCore`), and the scratch/drawing variables `$00-$17 $1A $1E $1F` (the high score `$00-$02`, drawing parameters). The four queue pages change in only 27 / 39 / 32 / 23 ticks (they are written at decisions and at life starts). So the ghost logic needs, besides the 20-byte records: `$E1/$E2`, `$E4`, `$EA/$EB`, `$E0`, `$68`, `$E9`, `$F3` (done), and the queue pages.

## 030e2/030e3 result (done 2026-11-02): the head of the ghost tick
* `port/src/core/ghosttick.{h,cpp}`: `GhostShared` (`$E1/$E2 $E4 $EA/$EB`), `GhostTables` (the four 16-byte countdown tables `$8723 $8733 $8743 $8753`, all from the data pack), `ghostHead()` = `$8763-$87BF`: `DEC $80`; for an acting ghost the shared frightened timer counts down once (16 bit) and ends exactly at zero (`$6D49`: all ghosts' frightened flags cleared, `$E4 = 0`, `$E9 = 0`, `$68 = 2`); the next countdown = frightened table, else eyes table, else normal table, indexed by min(level, 15); plus the ghost term `(E0 >> 1) - 1` + `(E0 and 1)` (ADC takes the carry that `ROR` left: ghosts 1..4 get 0, 0, +1, +1) when bit 1 of the looked-at coordinate (Y for a ghost moving left/right, X if Y = 0; else X) is clear; doubled when `$7A` is set. The state model needs no separate class: the records are `GameState::ghosts`, the shared variables `GhostShared`; the queue pages are added with 030e4 (so 030e2 is merged into 030e3/e4).
* Test (`tests/test_ghosthead.cpp`, needs `--ticks` with `ghostram_X.bin` next to the files): 33,796 ticks of A-D (32 left out: pokes, eaten ghosts, energizers, catches, level changes): **123,421 waiting ghost-ticks, countdown wrong in 0; 11,763 acting ghost-ticks: reload as chosen 11,411, turn table `$8733` 352 (after a change of direction the junction logic replaces the countdown, as read at `$8806`), other 0; shared timer wrong in 0 ticks; 4 ends of the frightened period, 0 wrong.**
* Not covered by data: the ghost-eaten tick (adds to `$E1/$E2` at `$8BA9`) and the energizer reload (`$6D60`) - those are 030e7/e8; the `$E1/$E2` end during a tick in which a ghost is also eaten.
