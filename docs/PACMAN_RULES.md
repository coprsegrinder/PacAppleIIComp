# Pac-Man side: dots, energizers, scoring, collision, lives (TASK-031a, part 1)

Tools: `tools/trace_pacman.py` (event trace), tests/test_pacman_tools.py. V = verified by running the original code on the oracle, I = inferred from reading code, U = unknown. Names are mine.

## 1. Dots and energizers (code $7A62-$7B2E)
* Pac-Man's position maps to a maze cell through the pointer `$16/$17` (computed from the position by $7A6B-$7A89). A cell holds 0 (empty), 1 (plain dot) or a larger value with bit 1+ set for an energizer: the code does `ROR A` and branches on zero (I). Maze data sits in RAM from about `$6200` and is rewritten at the start of each level.
* **Plain dot (V, 38 events):** score `$52` (BCD, tens/units) `+= $10` with carry into `$53`, `$54` -> **10 points**; cell cleared (`$7B10`); `$57` and `$56` decremented. **Initial `$56` = 248 = 244 dots + 4 energizers (V)**; when `$56` reaches 0 the level ends (`JMP $6CF4`, I from code).
* **Energizer (V by injecting an energizer cell, one run):** score `+= $50` -> **50 points**; frightened state starts exactly as in GHOST_MODES.md (`$E4 = 1`, all four `$78 = 1`, timer **352** at level 1, `$68 = 2`).
* The score is 3 BCD bytes `$52/$53/$54`; the high score is compared and copied by `$7091` (I).
* Level >= 21: the energizer gives its 50 points but no frightened state (`$6D60` compares `$55` with `$15`) (I).

## 2. Ghost collision (I, code read; one real catch/eat observed)
* `$8B34`: a ghost (position `$70/$71`) touches Pac-Man (`$60/$61`) when one coordinate is equal and the other differs by less than **6** units (first test <10, then <6) (I). From level 5 on, an extra rule can add 1 to Pac-Man's countdown `$59` under a bit condition (I: not understood).
* Then `$8B9D`: ghost frightened (`$78 <> 0`) -> eaten (V: `ghost_eaten` events seen in real play); not frightened -> Pac-Man caught (`$8BDD`) (V: 3 catches traced).
* **Eating a ghost (I):** `$78 := 0`, `$79 := 1` (eyes), frightened timer `$E1/$E2 +=` table `$6E19[min(level,15)]` (64, 48, 32, 32, 24, 24, 24, 24, then 16: purpose U), score `$53 += $68` in BCD, `$68` doubled: **200, 400, 800, 1600** (I; the score change itself was not observed: my second experiment ran into the attract demo, which eats ghosts without scoring, and I stopped rather than try a third variation).

## 3. Lives (V for the numbers, I for the rules)
* `$58` = lives counter: starts at **`$83`** = bit 7 set + 3. **Each catch decrements it (V: `$83 -> $82 -> $81 -> $80`, then `0` = game over, three catches in one run).** So the player has 3 lives including the one in play.
* **Bonus life (I, code $7124-$713B):** while bit 7 of `$58` is set and the score in `$54` is >= 1 (i.e. >= 10,000 points), a sound plays, `$58` is incremented and bit 7 cleared (one bonus life per game). Not run.

## 4. Fruit (I, code $6A7C)
`$57` counts down once per dot (starts at 64 in `$532B`/`$54F2`); at 0 the fruit parameters for level `min($55, 26)` are loaded from 4-byte entries at `$6AFE` into `$5C/$5D` and `$5A/$5B` (appearance/lifetime counters), `$57 := $60` (96 more dots) and `$E7 := 1`. Fruit timing, sprites and values are not analysed yet (-> TASK-031).

## 5. Still unknown for 031a
Pac-Man movement/turning details and the `$59` speed rule ($7861-$7A60 only read), what $50/$51 are, death animation, the energizer cell value (bit meaning), the exact `$8B34` conditions, observed ghost-eat scores.
