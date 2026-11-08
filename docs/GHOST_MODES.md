# Ghost modes, timers and speeds (TASK-030c)

Tools: `tools/zp_refs.py` (who reads/writes a zero-page variable), `tools/trace_modes.py` (mode timeline; can call the game's own energizer routine on the oracle), `tools/listing.py`. Tests: tests/test_trace_modes.py.
V = verified by running the original code on the oracle, I = inferred from reading the code, U = unknown. Variable names are mine. Only counts, addresses and the level-1 numbers actually measured are quoted; the full tuning tables come from the data pack.

## 1. Ghost record fields (offset from $70; each ghost has a 20-byte record at $84, $98, $AC, $C0 swapped in/out of $70-$83 by $86FC/$8712)
| Offset | Reg | Meaning | Evidence |
|---|---|---|---|
| 0, 1 | $70, $71 | X, Y position | V (traces) |
| 2 | $72 | current direction (0 down, 1 up, 2 right, 3 left) | V |
| 3 | $73 | current junction number | V (dispatch check) |
| 4 | $74 | second junction number (the one being compared at $87D5; likely "heading to") | I |
| 8 | $78 | **frightened (blue) flag** | V: set to 1 for all four ghosts right after the energizer routine |
| 9 | $79 | **eyes flag** (eaten ghost returning to the house) | I: set by the eaten-ghost code ($8BB9), cleared on arrival ($883F); not yet run in a trace |
| 10 | $7A | slow flag: doubles the step delay (probably the side tunnel) | I |
| 11, 12 | $7B, $7C | movement sub-step counters (incremented/decremented in $88FF-$8957) | I |
| 16 | $80 | **step countdown**: the ghost takes a step when it reaches 0 | V (code, traces) |
| 17 | $81 | "in/leaving the house" flag: such ghosts do not reverse at mode switches ($80C8) | I |
Global: `$E0` ghost number being processed, `$E4` frightened active, `$E9` corner phase, `$EA/$EB` mode countdown (16 bit), `$E1/$E2` frightened countdown (16 bit), `$55` level number (starts at 1), `$68` ghost-eaten score step, `$53/$54` score (BCD).

## 2. Corner / chase phases (V: code + traced timeline)
* `$E9 = 1`: **corner phase**: every ghost's target is its own maze corner (GHOST_ANALYSIS.md). `$E9 = 0`: **chase phase**: target is Pac-Man's position.
* The countdown `$EA/$EB` is decremented once per **mode tick** by the routine at `$808C` (called from the main loop at `$7874`). At zero (`$809D-$80C0`) the phase flips, **all ghosts reverse direction** (`$80C1`: `direction XOR 1`, pushed to the move queue; ghosts with `$81` set are skipped) and a new countdown is loaded:
  * corner -> chase: **600 ticks** ($0258); chase -> corner: **140 ticks** ($008C).
* At the start of every life ($5410-$5418) the game sets `$E9 = 1` and the countdown to **160 ticks** ($00A0). Traced: after the life reset (mode tick 260) the flip to chase happened at tick 420, exactly 160 ticks later.
* So the cycle at level 1 is: corner 160, chase 600, corner 140, chase 600, corner 140, ... The code shows no level dependence of these three numbers (no other writer of `$EA/$EB` besides the init, the energizer reset and these two reloads); U whether the cycle ever stops (the code reads as endless).
* Rate: measured once at level 1, **160 ticks took 7.93M cycles (about 7.8 s), i.e. one mode tick per about 49,600 cycles**. Over a whole traced run 473 mode ticks occurred while 18,341 ghost steps were taken (about 39 ghost steps per mode tick). Not yet shown to be constant.

## 3. Energizer ("frightened") (V for what is marked; routine $6D60, called from $7AEE and $7B23)
1. If the level number `$55` is **21 or higher the routine does nothing** (V: code `CPX #$15 / BCS`; not run).
2. Otherwise, for each ghost whose eyes flag is clear: `$78 = 1`; then `$68 = 2` (the 200-point step), `$E4 = 1`, frightened countdown `$E1/$E2` loaded from a table of 22 sixteen-bit values indexed by `min($55, 21)`, the mode countdown reset to **1028 ticks**, `$E9 = 1` and **all ghosts reverse** (`$80C1`).
3. **Verified on the oracle (level 1, energizer routine called between main-loop passes):** right after the call `$E4 = 1`, timer = **352**, all four `$78 = 1`, mode countdown = 1028. The timer then decremented 352 times and the end routine ran: all `$78` cleared, `$E4 = 0`, **`$E9 = 0` (chase)**, `$68 = 2`. It lasted 6.96M cycles = **6.8 s of Apple time** at level 1.
4. The frightened countdown is decremented in the ghost step routine ($8772) whenever a ghost's step countdown reaches zero while `$E4` is set; the end is `$6D49`.
5. **Frightened ghosts head for their corners** (`$78 <> 0` selects the corner target in `$8C87`); there is no random movement in this code path (V: 30 traced decisions had both flags set and used corner targets; I for the general claim).
6. The frightened durations per level are not monotonic in the table (an entry of $FFFF appears at one level index and several short ones later): meaning U; the numbers come from the data pack.

## 4. Eating a ghost (I: read from $8B9D-$8BD8; not yet run)
If the touched ghost has `$78 <> 0`: frightened flag cleared, **eyes flag `$79 = 1`**, the frightened countdown is increased by a level-indexed amount (table at $6E19; purpose U), and unless in demo mode (`$E8 = 0`) the score gets `$68` added in decimal (BCD) mode and `$68` is doubled: **200, 400, 800, 1600** for successive ghosts (I: 2 -> 4 -> 8 -> 16 in units of 100). If `$78` is clear the ghost is not eaten (Pac-Man is caught: code at $8BDD, not read yet).
Eyes: the step delay comes from a separate, much shorter table (fast). When the eyes reach the house (`$8839-$8851`) `$79 = 0`, `$81 = 1`, the ghost is put back at its start position (table at $6D3F, routine $6D30) and a scripted exit route is queued (`$6DA1`).

## 5. Speed: how a ghost's step delay is chosen (V: code read; table shapes read)
Each ghost has `$80`, a countdown decremented once per call of `$8763`; at zero the ghost takes one step and `$80` is reloaded:
* **base delay** from one of three tables indexed by `min($55, 15)` (`$6D1E`): normal (`$8723`), frightened (`$8743`) or eyes (`$8753`);
* plus a small per-ghost term (ghost number halved minus 1: -1, 0, 0, +1 for ghosts 1-4) added on every other unit of position (bit 1 of the coordinate is clear);
* doubled when the slow flag `$7A` is set.
Shapes: the normal delay falls from 10 to 4 over the 16 level classes, the frightened one from 19 to 7, the eyes one from 4 to 1: so ghosts speed up with level, are about twice as slow when frightened and much faster as eyes. The second half of the normal table (16 more entries, 19 down to 5) is used by something else (U).

## 6. Not yet understood
`$7A` and `$7B/$7C`; how ghosts are released from the house over time; the extra amount added on eating a ghost; what happens when Pac-Man is caught; why the table of frightened durations has an $FFFF entry; Pac-Man's own speed table (maybe the second half of the normal table); Pac-Man movement. Tasks: 030d and the scoring/level analysis.

## The chase -> corner switch seen on the oracle (TASK-034-MODEFLIP, 2026-11-08)
The line-coverage run (docs/CODE_COVERAGE_PORT.md) showed that no recording ever ran the natural chase -> corner switch of `$808C` (`modeTimerStep`, ghostmode.cpp): a natural chase phase lasts 600 steps of Pac-Man (about 5,400 ticks) and every recorded chase phase was ended early by an energizer. `tools/record_modeflip.py` replays the keys of golden game A and, at ticks 1500 and 3300 (in a chase phase), sets the countdown `$EA/$EB` to 3 / 0, so that the original's own code switches 3 steps later; `record_ghost_ram.py W01` records the memory beside it. Result on the oracle (V): chase -> corner at ticks 1522 and 3331, corner -> chase (natural, 140 steps later) at tick 2834; the port equals it in all shared variables, queues and ghost records, both fed (test_ghostmove: 4 flips) and free-running (test_ghostworld: 4,200 ticks, 0 wrong). The recording is `scratch/ticks_W01.bin` (+ `ghostram_W01.bin`); `tools/full_suite.sh` and `tools/coverage_cpp.sh` include it. ghostmode.cpp is now 100 % covered.
