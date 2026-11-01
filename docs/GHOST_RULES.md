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
