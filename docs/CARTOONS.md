# Intermission cartoons (TASK-031c, in progress: scheduler and cartoon 1 done)

Tools: `tools/trace_cartoon.py` (records every drawing call on the oracle), `tools/cartoon_model.py` (reference model of cartoon 1). Tests: tests/test_cartoon_schedule.py (6 s), tests/test_cartoons.py (about 5 minutes). Golden data: tests/golden/cartoon_hashes.json (hashes and counts only). V = verified on the oracle, I = inferred.

## 1. Which cartoon after which level (V)
The scheduler `$8155` reads the level `$55`: 2 -> cartoon 1 (`$81AE`), 5 -> cartoon 2 (`$83D0`), 9 -> cartoon 3 (`$8486`); above 9 it subtracts 13 until the value is 2, 5 or 9 or below. **Result for every level 1-255 (run on the original code): cartoon 1 after levels 2, 15, 28, 41, ...; cartoon 2 after 5, 18, 31, ...; cartoon 3 after 9, 22, 35, ... (level mod 13 = 2, 5, 9); no cartoon after any other level, including 10, 11, 12 and 13.** Cartoons are also reachable from the hidden menu (Ctrl-V Ctrl-C, `C`, 1-3; CONTROLS.md).

## 2. Common machinery (V)
* A cartoon first clears the page (`$6F46`) and switches to hi-res (`$6F2F`), then runs a loop with **no keyboard polling at all** (V: the loops contain no `$C000` read), so a key press does not skip it. Whether a key pressed during a cartoon is seen afterwards is U.
* **Zero speaker clicks in all three cartoons (V)**: the "cartoon sound" call sites listed in SOUND.md are not reached in the cartoons as run from the hidden menu.
* Helper routines: `$7D80` copies Pac-Man's `$60/$61` into the draw position `$0C/$0D, $10`; `$7D8D` does the same for the ghost registers `$70/$71`; `$7D9A` subtracts (4, 4) from the position and `$7DA9` subtracts (5, 4); `$699E` = `$7D9A` followed by the **masked clear** `$69A1` (a 3 x 10-byte block, the erase before moving a sprite). Delay loops: `$6CE6` (one short delay per loop) and `$6CE8` (X x delay).
* A moving character is drawn by `$79A4` (Pac-Man: frame pointer from the table `$96E8`; index from the animation counter `$66` and the direction `$62`: `$66 and 3` = 0 gives frame 0, 1 or 3 give `$62 + 1`, 2 gives `$62 + 5`) and `$88A2` (ghost: frame pointer from `$9E5C`, index `2 x ghost number + 12` for the normal-colour variant when `$1D <> 0`; the frightened and eyes variants use other indexes of the same table). The big sprite is `$8259` (GRAPHICS.md section 3).
* Rows: Pac-Man and the ghosts run along row `$7A` (122). Positions are pixel x (0-279) in the registers; the masked clear is at (x - 4, y - 4).

## 3. Cartoon 1 (`$81AE`-`$8258`, V: the model equals the oracle call for call)
Phase 1: Pac-Man starts at x = `$C1` (193), the ghost at x = `$E5` (229); the loop counters `$59` and `$80` start at 1. Every loop: decrement `$59`; when it reaches 0 reload 32, erase Pac-Man, `x -= 2`, and if `x < $21` leave the phase, else `$66 += 1` and draw Pac-Man. Then a delay; decrement `$80`; when it reaches 0 reload 29, erase the ghost, `x -= 2`, draw it. So both walk left, the ghost slightly faster (2 pixels per 29 loops against 2 per 32), and catch up.
Pause: erase both, then 8 x 100 delay loops. Phase 2: the big Pac-Man sprite starts at x = 0, y = `$7A - $0E`; the ghost restarts at x = `$38`. Every loop: decrement `$59` (counting on from phase 1, reload 6): `x += 2`, leave when `x >= $A0`, else draw the big sprite (`$66 += 1`); decrement `$80` (reload 7): erase the ghost, `x += 2`, draw it. So the giant Pac-Man chases the ghost to the right (2 pixels per 6 loops against 2 per 7). The routine returns to the caller after the sprite passes x = `$A0`.
Check: 562 drawing calls (page clear, hi-res on, 242 masked clears, 239 frame draws, 79 big sprites) equal the model's list exactly; 5,597,305 cycles (about 5.5 s on a real Apple II at 1.023 MHz).

## 4. Golden data (V, replay deterministic)
| Cartoon | Cycles | Calls logged | Draw-call chain (first 12 hex) |
|---|---|---|---|
| 1 | 5,597,305 | 4,076 | 59d0d67df31f |
| 2 | 4,347,216 | 2,509 | c4367efc21b0 |
| 3 | 8,211,963 | 5,505 | 4e4321a03447 |
Final-page hashes and per-routine counts are in tests/golden/cartoon_hashes.json. Cartoon 2 draws with the frame tables `$9A42`, `$A472`, `$9D8A`, `$96FA` and has no big sprite; cartoon 3 adds nine more frame tables (`$7E16`, `$9970`, `$7DDA`, `$9CB8`, `$7E70`, `$7E34`, `$7DF8`, `$7DBC`, `$7E52`) and uses the other wrapper `$7DA9`.

## 5. Still to do
* **TASK-031d:** cartoon 2 (`$83D0`-`$8485`) decode and model. **TASK-031e:** cartoon 3 (`$8486`-end) decode and model.
* The meaning of each frame table (Pac-Man, ghost, torn-cloth ghost, ...) is U; the port only needs the pointers and the images from the data pack.
* Whether the cartoon is shown for each player in two-player games is U.
