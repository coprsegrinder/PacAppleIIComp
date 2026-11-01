# Pac-Man side: dots, energizers, scoring, collision, lives, speed, turning (TASK-031a, parts 1 and 2)

Tools: `tools/trace_pacman.py` (event trace), `tools/trace_eatscore.py` (ghost-eat score, bonus life), `tools/trace_speed.py` (step-delay rule), tests/test_pacman_tools.py. V = verified by running the original code on the oracle, I = inferred from reading code, U = unknown. Names are mine.

## 1. Dots and energizers (code $7A62-$7B2E)
* Pac-Man's position maps to a maze cell through the pointer `$16/$17` (computed from the position by $7A6B-$7A89). A cell holds 0 (empty), 1 (plain dot) or a larger value with bit 1+ set for an energizer: the code does `ROR A` and branches on zero (I). Maze data sits in RAM from about `$6200` and is rewritten at the start of each level.
* **Plain dot (V, 38 events):** score `$52` (BCD, tens/units) `+= $10` with carry into `$53`, `$54` -> **10 points**; cell cleared (`$7B10`); `$57` and `$56` decremented. **Initial `$56` = 248 = 244 dots + 4 energizers (V)**; when `$56` reaches 0 the level ends (`JMP $6CF4`, I from code).
* **Energizer (V by injecting an energizer cell, one run):** score `+= $50` -> **50 points**; frightened state starts exactly as in GHOST_MODES.md (`$E4 = 1`, all four `$78 = 1`, timer **352** at level 1, `$68 = 2`).
* The score is 3 BCD bytes `$52/$53/$54`; the high score is compared and copied by `$7091` (I).
* Level >= 21: the energizer gives its 50 points but no frightened state (`$6D60` compares `$55` with `$15`) (I).

## 2. Ghost collision (I, code read; one real catch/eat observed)
* `$8B34`: a ghost (position `$70/$71`) touches Pac-Man (`$60/$61`) when one coordinate is equal and the other differs by less than **6** units (first test <10, then <6) (I). From level 5 on, an extra rule can add 1 to Pac-Man's countdown `$59` under a bit condition (I: not understood).
* Then `$8B9D`: ghost frightened (`$78 <> 0`) -> eaten (V: `ghost_eaten` events seen in real play); not frightened -> Pac-Man caught (`$8BDD`) (V: 3 catches traced).
* **Eating a ghost (I):** `$78 := 0`, `$79 := 1` (eyes), frightened timer `$E1/$E2 +=` table `$6E19[min(level,15)]` (64, 48, 32, 32, 24, 24, 24, 24, then 16: purpose U), score `$53 += $68` in BCD, `$68` doubled. **V (part 2, tools/trace_eatscore.py, demo excluded): the first ghost eaten gave exactly +200 (`$68` 2 -> 4).** The 400/800/1600 continuation is I (the doubling is in the code; only one ghost was eaten per run before the game ended).

## 3. Lives (V for the numbers, I for the rules)
* `$58` = lives counter: starts at **`$83`** = bit 7 set + 3. **Each catch decrements it (V: `$83 -> $82 -> $81 -> $80`, then `0` = game over, three catches in one run).** So the player has 3 lives including the one in play.
* **Bonus life (V, one run):** while bit 7 of `$58` is set and the score byte `$54` is >= 1 (>= 10,000 points), a sound plays, `$58` is incremented and bit 7 cleared (one per game). Observed `$83` -> `$04` (4 lives, flag gone) after the score crossed 10,000. **Caveat:** to reach 10,000 quickly I preset the score to 9,980 in the experiment; the rule itself ran in the original code.

## 4. Fruit (I, code $6A7C)
`$57` counts down once per dot (starts at 64 in `$532B`/`$54F2`); at 0 the fruit parameters for level `min($55, 26)` are loaded from 4-byte entries at `$6AFE` into `$5C/$5D` and `$5A/$5B` (appearance/lifetime counters), `$57 := $60` (96 more dots) and `$E7 := 1`. Fruit timing, sprites and values are not analysed yet (-> TASK-031).

## 5. Pac-Man's speed (part 2)
* `$59` is a countdown decremented once per main-loop pass; Pac-Man acts when it hits 0, then it is reloaded (`$7874-$789B`). **Reload = `$7851[min(level,15)]` + (number of frightened flags among ghosts 2 and 4) - (number of eyes flags among ghosts 1 and 3).** Level 1 base = 9; table: 9 9 8 7 7 6 6 6 6 5 5 5 5 4 4 4. **V: 797 of 797 reloads matched the formula; observed values 9 (calm) and 11 (ghosts 2 and 4 frightened).** The eyes term and the ghost 1/3 asymmetry were never exercised (always 0) -> I. A larger value means slower, so frightened ghosts make Pac-Man *slower* by this count (I: note the odd asymmetry; the byte addresses come straight from the code).
* **Dot penalty (I):** eating a dot or energizer adds `$7A8C[min(level,15)]` to `$59` at once and sets `$F3 := $7A9C[...]`; each following reload adds 1 more while `$F3 > 0`. Tables: `$7A8C` = 2 2 2 2 2 1 1 1 1 1 1 0 0 0 0 0, `$7A9C` = 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 0. So at level 1 eating costs Pac-Man time; levels >= 12 have no immediate penalty.

## 6. Turning and the tunnel (I, code read $78A5-$7965, $7B2F-$7B79; not traced)
* Pac-Man moves along the same junction graph. Between junctions: if the requested direction `$63` is the exact opposite of the current `$62` (same pair 0/1 or 2/3) it is taken at once (reverse anywhere). At a junction (`$60/$61` equals the junction's coordinates): if the graph has an exit in the requested direction, `$62 := $63`; else if the current direction has an exit he continues; else he stops (`$66 := 2`, redraw).
* Junctions 26 and 29 are the tunnel ends: moving out the far side wraps X (`$DD` -> `$3D`) and re-enters through the other end. Step size 2 units, as for ghosts.
* In demo mode (`$E8 <> 0`) `$63` comes from the recorded stream instead of the keys.

## 7. Still unknown
What `$50/$51` are, the energizer cell value bits, the exact `$8B34` collision conditions (incl. the level-5 tweak), the death animation, fruit values and sprites, and the 400/800/1600 chain in practice.
