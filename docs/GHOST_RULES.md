# Ghost decision rules: complete tables (TASK-030b)

Tool: `tools/ghost_rules.py` (writes `scratch/ghost_rules.json`, git-ignored). Tests: tests/test_ghost_rules.py. Builds on GHOST_ANALYSIS.md.
V = verified by running the original code on the oracle, I = inferred. Names are mine. Only counts are quoted here; the table values themselves go to the data pack (D1-A).

## 1. What the 48 routines can see (V, static scan of all routines and every routine they jump into)
Every routine reads **only**: A (incoming direction), X/Y (the target), the ghost position `$70/$71`, and three more bytes:
* **`$62` = Pac-Man's current direction** - read by **16 routines** (24-29, 32-37, 39, 42-44). The earlier grid (GHOST_ANALYSIS.md section 4) never varied it, so it missed this: **the ghosts' "personalities" at those junctions depend on which way Pac-Man is heading.**
* **`$69` = Pac-Man in the tunnel flag** (I) - read by routines 18, 19, 40. **`$72`** = the ghost's own direction (18, 19, 40; in real calls it equals A, I from the dispatch code).
No other byte is read, and no routine uses arithmetic beyond subtraction for an absolute difference (routines 38 and 40: both |dx| and |dy| < `$20` hands over to routine 9 / 18). Comparisons are only: A against a constant, X against a constant or `$70`, Y against a constant or `$71`. So the answer is a function of (A, `$62`, `$69`, `$72`, class of X, class of Y), where a class is "equal to a breakpoint" or "a gap between breakpoints".

## 2. Complete tables (V)
* Each of the 48 routines was run on the oracle for **every class of X and Y** (each breakpoint, ties included, and one value in each gap) x all 4 directions x all values of the extra inputs it reads, at **every real ghost position** that uses it: **18,268 table cells, all 48 routines returned to RTS.**
* The tables were checked against the oracle on **14,468 random full-range inputs: 0 mismatches.** A test shows the check can fail (dropping the constants gives 171 mismatches of 453).
* Which routines are used where (V): routines 0-3 are shared by many junctions (9, 9, 6, 6 positions) and depend on direction only. **Routines 4-47 are each used at exactly one junction position**, i.e. they are hand-written per junction (plus the ghost-specific variants). Dependence on the target: all of 4-47 use X and Y except 16/17 (X only).
* The eight routines never executed in play (10, 12, 13, 39, 41, 45, 46, 47) are covered the same way, standalone.
* Ties (target equal to the ghost coordinate or to a constant) are separate classes in the tables, so their rules are exact.

## 3. Checks on the tables
* **Every answer in every cell is a real exit of its junction: 46,304 of 46,304 (V).**
* **Reversals:** the 183 recorded decisions never reversed. The tables, though, allow an answer opposite to the incoming direction in 18 (junction, ghost, incoming-direction) combinations, i.e. 10 (junction, ghost) slots: junction 20 (ghost 4), junction 26 (ghosts 1-4: the tunnel junction, where the table also varies `$72` independently of A, a combination that probably cannot occur), junction 38 (all four ghosts), junction 62 (ghost 3). Whether any of these can really happen in play is **not established** (U); the C++ must simply follow the tables, not forbid reversals.

## 4. For the C++ port
Store per routine and position: the X and Y breakpoint lists and the answer cells (generated from the oracle into the data pack), plus one lookup function `class_of(value, breakpoints)`. This is exactly what `tools/ghost_rules.py` does, so the C++ lookup can be tested cell by cell and on random inputs against this JSON (TASK-030e).

## 5. Still unknown
Why 16 starts with a NOP, the meaning of the two extra bytes of each junction record, and the reachability of the reversal cases.

## Every branch of the 48 decision routines taken both ways on the oracle (TASK-034-DECIDE, 2026-11-09)
The line-coverage run showed the generated C++ of the decision routines (`ghostdecide_gen.cpp`) had unrun lines: the block `$9650-$9656` of the routine at `$9627` (reached only when the target's Y is exactly `$6B`, one value in 256) and some branch directions. `tools/record_decide_edges.py` searches inputs on the oracle instead of drawing them: values on both sides of every constant a routine compares X or Y with, the ghost's own coordinates and one off them, all directions; a vector is kept when it takes a branch direction no earlier vector of its routine took (477 vectors, `scratch/decide_vectors_edges.bin`, passed as the second file of `--decide`).
* **Result (V):** the C++ routines equal the original on all 96,477 vectors (96,000 random + 477 branch-directed). Every conditional branch of 39 of the 48 routines goes both ways. **Nine branch directions in nine routines never happen, and the original's own code explains why** (a search over every value of X, with Y at every constant, all A, all Pac-Man directions and four ghost positions, also never takes them: `--prove`): `8FEB N`, `925E N`, `929B N`, `93DC N`, `961F N`, `966A N` are the third branch of a three-way compare (`CPX/CPY`, `BCC`, `BEQ`, `BCS`: after the first two have not branched, the carry is set, so the `BCS` always branches); `9180 T` and `91DC T` test `X = $70` after an earlier `CPX $70 / BEQ` in the same routine has already left on equality; `953F T` tests `X < $37` after `CPX #$93 / BCC` has left for `X < $93`. So the C++ has nothing to port there.
* **Coverage after this:** `ghostdecide_gen.cpp` 1,209 of 1,211 lines; the other two are the fall-back `default` of the dispatcher (an address outside the routines: "never reached with a valid entry").

## The near-ghost delay seen firing on the oracle (TASK-034-NEARGHOST, 2026-11-09)
The rule at `$8B34-$8B65` (in the contact test of every ghost that moved: from level 5 on, a ghost in Pac-Man's row or column 6-9 pixels away may add one tick to his step countdown `$59`; it fires when the coordinate it looks at has bit 1 clear) was rebuilt from the code, but all recordings together held one tick where it fired. `tools/record_nearghost.py` starts games at ranks 5, 9, 13 and 21 through the hidden menu and, every 6 ticks, puts a ghost (in turn) in Pac-Man's row or column, 6-9 pixels away, moving along it (the lives are topped up every 10 ticks); 1,500 ticks each (`scratch/ticks_Y05/Y09/Y13/Y21.bin`, passed as `--startticks`).
* **Result (V):** the rule added a tick **47 times** in the four recordings (and was correctly not applied in the many near misses), and Pac-Man's step countdown `$59` equals the oracle's in every one of the 6,000 ticks; the 135 catches in those games restart as the oracle's do.
* **A pitfall of the method, found and fixed in the test:** a ghost that a poke puts somewhere is not a ghost that "moved", and only a moved ghost runs the contact test; the replay test therefore applies the poke to the ghosts before the tick (otherwise it saw 13 false extra delays).
* Counter: `GameCore::nearGhostDelayCount()`; the test requires at least 10 firings per recording.
