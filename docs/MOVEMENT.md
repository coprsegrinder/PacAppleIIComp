# Movement, the ghost house and the eyes' route (TASK-030d)

Tools: `tools/trace_house.py` (release timeline), `tools/trace_eyes.py` (turn a ghost into eyes and follow it), `tools/junction_graph.py` (now also checks the eyes table), `tools/listing.py`, `tools/zp_refs.py`. Tests: tests/test_movement_tools.py.
V = verified by running the original code on the oracle (or by a full static check), I = inferred from reading code, U = unknown. Names are mine. Only counts, coordinates and level-1 numbers actually measured are quoted; the full tables come from the data pack.

## 1. One pass of the main loop (V: the loop at $530F; I for what each part does)
`JSR $7861` (Pac-Man's step timer, key poll, mode tick) -> `JSR $86EA` (ghosts 4 down to 1: swap the ghost's record into $70-$83, call its step timer $8763, swap it back) -> `JSR $6A7C` (bonus fruit timing, per-level table at $6AFE) -> `JSR $570C` (energizer blink, countdown $5E from a table) -> repeat. Cost of a pass: 2,987 cycles when nothing steps, up to 27,950 when sprites are drawn (ARCHITECTURE.md).

## 2. Pac-Man (I: read from $7861-$78F5, not traced)
* Position `$60/$61`, current junction `$64`, next junction `$65` ($FF = none), requested direction `$63`. **Pac-Man uses the same junction graph as the ghosts** (positions are compared with junction records through `$75F1`).
* Step countdown `$59` is decremented once per pass; at zero the key poll runs, the mode tick runs, and `$59` is reloaded from a level-indexed table (`$7851[min(level,15)]`) adjusted by the number of ghosts in certain states (the exact rule is U; it is why Pac-Man's speed depends on the ghosts).
* **Demo mode** (`$E8 <> 0`): the requested direction `$63` is not read from the keyboard but from a byte stream through the pointer `$1B/$1C`, advanced at each arrival: the attract-mode demo is a recorded sequence of moves replayed through the normal code (I).

## 3. How a ghost takes a step (V: code read; the first part also traced)
* A ghost steps when its countdown `$80` reaches zero (speed: GHOST_MODES.md). One step moves its position by **2 units**: Y for directions 0/1 (down +2, up -2), X for 2/3 (right +2, left -2). Distances between junctions on the 10 x 10 grid are 12 or 18 units, i.e. 6 or 9 steps.
* Before moving, the code compares the ghost's position with junction `$73` and with `$74` (the junction it is heading to). If it is at `$74`, then `$73 := $74` and a decision is made (GHOST_ANALYSIS.md); after the decision `$74` = the graph neighbour of `$73` in the new direction (record byte 2 + direction). Otherwise the ghost just moves one step.
* Special junctions (V from the graph and the code): **23 and 24 are the two house-door junctions** (eyes re-enter there); **26 and 29 are the two tunnel ends** (the graph links them left/right); arriving at 29 heading right sets the slow flag `$7A` (I: the tunnel slows the ghost; `$7B` and `$7D` get set up as a tunnel step counter).
* Sprite choice: a frightened ghost flashes during the **last 128 ticks** of the frightened timer (bit 4 of the low timer byte toggles every 16 ticks, V from code); eyes use their own sprites.

## 4. The ghost house (V for the release timeline, I for the exit script)
* At the start of a life all four ghosts have `$81 = 1` (in the house) and the level-indexed countdowns `$82` of **ghosts 2, 3 and 4** are loaded from three 16-entry tables (`$54A4`, `$54B4`, `$54C4`, indexed by `min(level,15)`, code at `$5420-$5435`). Ghost 1 has no countdown and leaves first.
* Each time an in-house ghost's step timer fires, `$82` is decremented (`$8988`). At zero the ghost gets a scripted exit move queue written into its move queue (`$898C-$89D5`; the script depends on the ghost number and on whether the corner phase `$E9` is active) and leaves.
* **Level-1 values (V, table entries):** ghost 2 = 112, ghost 3 = 48, ghost 4 = 144 in-house steps. The tables fall with the level (for ghost 3: 48 down to 4; for ghost 2: 112 down to 8; for ghost 4: 144 down to 9 over the 16 level classes), so ghosts leave sooner at higher levels.
* **Traced at level 1 (V, one run, Pac-Man not caught):** ghost 1 out at 4.56 s after pressing Enter (when play starts), then **ghost 3 at +3.7 s, ghost 2 at +7.1 s, ghost 4 at +9.4 s**: the release order is 1, 3, 2, 4 (in my numbering, which is the game's `$E0`). After Pac-Man was caught (21.2 s) all ghosts went back in, and the three countdowns released them again in the same order.
* The start positions of the ghosts are in a table at `$6D3F` (X, Y per ghost number) and are used when a ghost is reset (routine `$6D30`).

## 5. The eyes' route home (V)
* An eaten ghost has `$79 = 1`. Its decisions do not use the 48 routines: the next move is simply the entry of the table at **`$A360`** for its current junction (GHOST_ANALYSIS.md, section 3).
* **Full static check:** following `$A360` from each of the 64 junctions always uses a real exit and always reaches a door junction (23 or 24): **64 of 64 junctions, longest walk 12 junctions, no loops, nothing broken**. So `$A360` is a complete "shortest way to the door" signpost table.
* **Traced experiment:** ghost 1 was made eyes at junction 7 heading right; it decided at junctions 8 (table: down), 16 (right), 17 (down) and 23 (door), then re-entered the house (`$79 = 0`, `$81 = 1`, back at its start position) after **0.66 s**: eyes are very fast (their step delay table is the shortest).

## 6. Still unknown
The in-house up/down bouncing and sprite drawing; the exact exit scripts per ghost; the slow-flag/tunnel counters `$7B-$7D`; the exact adjustments of Pac-Man's countdown `$59`; collision detection between Pac-Man and ghosts; dot eating, scoring and energizer detection (Pac-Man side, code at $7AB4-$7B23 and $8B9D); lives and level completion. These go to the scoring/level analysis and to later Pac-Man movement tasks.
