# Ghost movement: structure (TASK-030a)

Tools: `tools/listing.py` (print a range of the program), `tools/trace_ghosts.py` (log every decision during play), `tools/ghost_routines.py` (run the 48 routines on a grid of inputs), `tools/junction_graph.py` (decode and check the maze graph). Tests: tests/test_ghost_tools.py, tests/test_junction_graph.py.
Status legend: V = verified by running/inspecting, I = inferred from code or data, U = unknown. Names are mine, not the original's. Nothing here reproduces original bytes beyond counts and coordinates.

## 1. The big picture (V unless marked)
Ghost steering is **junction based**. The maze is a graph of 64 junctions; when a ghost stands on a junction and has no queued move, the game picks the next direction with a small routine chosen by (junction, ghost). The routine compares the ghost's position with a **target** and chooses among the junction's real exits. Between junctions the ghost just follows the queued direction.

## 2. Pieces
| Piece | Address | What it is |
|---|---|---|
| Junction graph | $760A-$7809 (512 bytes) | 64 records x 8 bytes: X, Y, neighbour junction for down/up/right/left ($FF = none), then 2 bytes of unknown meaning (two equal values, multiples of $11: U). Positions lie on a 10 x 10 grid (X in $41-$D7, Y in $0B-$B3). |
| Routine index table | $8CBE-$8DBD (256 bytes) | `index[junction*4 + ghost - 1]` = routine number (0-47). 54 of 64 junctions give all four ghosts the same routine; **10 junctions have one ghost-specific variant** (routines 38-47). |
| Routine address table | $8DBE-$8E1D (48 words) | address of each decision routine |
| Decision routines | $8E1E-$96E7 | 48 small routines (1-243 bytes) |
| Corner targets | $8CB4 (pairs Y,X for ghost 1-4) | (X,Y) = ($C9,$19), ($51,$19), ($51,$87), ($C9,$87) for ghosts 1-4: the four corners of the maze (I) |
| Fixed-direction table | $A360 (indexed by junction) | one direction code per junction (values 0-3); used when `$79` is set (I: probably the route of a ghost returning home; not yet run) |
| Move queue | pointer `$75/$76`, index `$77` | list of direction codes ending with `$FF` (V from code and traces) |
| Ghost records | $84-$D3 | four 20-byte records (ghost 1: $84-$97, 2: $98-$AB, 3: $AC-$BF, 4: $C0-$D3) copied in and out of the working registers `$70-$83` by the routines at $86FC and $8712 (V: copy loops; `$E0` = ghost number, offsets from the table at $870D) |
| Working registers | `$70/$71` ghost X/Y, `$72` current direction, `$73` current junction, `$78/$79/$E9` flags, `$60/$61` Pac-Man's X/Y (I), `$E0` ghost number 1-4 | roles from code and from the traces below |

Direction codes (same as the player's): **0 = down, 1 = up, 2 = right, 3 = left** (V: customised-key prompts name 1 = up and 0 = down; in 65 decisions answering 0 the target was lower on the screen in 61, so Y grows downward).

## 3. How a decision is made (the routine at $8C50, read from the code; V for the dispatch formula)
1. `$87ED-$87EF` (caller): the ghost's position ($70,$71) is compared with the junction record found by the lookup routine at $75F1 (`$7600 + 8 x index + $0A`); on a match `$73` = that junction number and the decision routine is called. (I: `$73`/`$74` are the junction the ghost is on and the one it is heading to.)
2. If `$79` is set: the next move is simply `$A360[$73]` and the queue is terminated with `$FF`.
3. Else if the queue already has another move queued: return.
4. Else: routine = `routineTable[ index[ $73 x 4 + $E0 - 1 ] ]`, written into the operand of the `JSR` at `$8CA6` (the patched call found in TASK-013c).
5. Target = the ghost's own corner (pair from `$8CB4`) when `$78` or `$E9` is non-zero, otherwise Pac-Man's position `$60/$61`. A = current direction `$72`, X,Y = target. The routine returns A = new direction, appended to the queue.
**Verified on gameplay traces:** 323 entries, 183 decisions across all four ghosts and 42 different junctions; the routine called equalled the table lookup in **183 of 183**. 30 decisions used the corner target (both flags set), 153 the Pac-Man target. **No decision ever reversed the ghost's direction (0 of 183).**

## 4. What the 48 routines are (V: each was run on the oracle over 4 directions x 3 X-relations x 5 Y values)
* **Answers are always real exits:** across all 256 (junction, ghost) pairs, every answer a routine can give is an exit of that junction in the graph: **256 of 256, 0 violations**. So the graph says what is *possible* and the routine says what is *preferred*.
* **Graph checks:** 64 junctions, 168 directed links, none missing, none one-way; 30 junctions have 2 exits (corners), 28 have 3, 6 have 4. The only geometry exceptions are the **side tunnel** (junction 26 at the left end of the middle row links left to junction 29 at the right end and back: the maze wraps).
* **Kinds:** 4 routines (0-3) depend on the incoming direction only: forced turns at corners (120 of the 256 slots); 6 depend on one axis (7, 13, 16, 17, 21, 45); 38 compare both axes; routine 19 also changes X/Y; routines **16 and 17 are the same code** (16 is a 1-byte entry at $905E that falls into 17 at $905F; identical answers on the grid, the byte's purpose is U).
* **Ghost-specific variants (the "personalities"), V:** at 10 junctions one ghost uses a different routine from the other three. Differences measured on the 60-entry grid: junction 8 (ghost 2) 3 entries, junction 12 (ghost 1) 4, junction 33 (ghost 3) 12, junction 26 (ghost 2) 37, junction 20 (ghost 4) 57, and **60 of 60 (a completely different rule)** at junctions 10 (ghost 4), 30 (ghost 4), 37 (ghost 2), 44 (ghost 3) and 62 (ghost 3).
* **Coverage:** 40 of the 48 routines have been executed in our runs; not executed: 10, 12, 13, 39, 41, 45, 46, 47 (their grid tables above come from running them standalone, so they are known, but never seen in play).

## 5. Not yet understood (next steps)
* **030b:** exact, complete decision rules per routine (my grid uses 3 X relations and 5 Y buckets; constants such as 34 for Y appear in some routines, so a full proof of equivalence needs every compared constant covered) and the rule for ties/equalities; then express them as data for the C++ port.
* **030c:** the modes: what `$78`, `$E9`, `$79` mean (chase/scatter/frightened/eyes, timers, switching), why decisions in corner mode appear only when both `$78` and `$E9` are 1, how `$80` (a countdown at $8763) sets speed.
* **030d:** ghost house exit and the eyes' return route (`$A360`), the junction record's last two bytes, who sets the queue pointer `$75`, how a ghost moves between junctions (per-frame step, speed per level).
* Pac-Man's own movement uses the same junction idea? (U)

## 6. Consequences for the port
The ghost logic can be reconstructed as **data plus one generic routine**: the junction graph (64 records), the index table (64 x 4), 48 small rules expressed as tables or compact decision trees, and the move queue. The table values must come from the data pack (D1-A); the C++ routines must be checked against the oracle with the grid tests (and bigger ones from TASK-030b).
