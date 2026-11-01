# Graphics and drawing spec (TASK-027a: inventory of screen writers)

Tool: `tools/trace_screen_writers.py` (about 2.5 minutes; writes scratch/screen_writers.json). Test: tests/test_screen_writers.py (3 tests; skipped without the json).
Scenario: boot, `1` + Return, attract (70M instructions), a game with random keys, a forced level end, 100 blocks of the next level. **V for this scenario; completeness over other scenarios (cartoons, rank prompt, 2 players, game over, death) is Unknown and is the next sub-step.** Earlier material: docs/SPRITES.md (frame formats, call sites).

## 1. Screen areas and video switches (V)
* The program lives at $4000-$A9FF, so **hi-res page 2 ($4000-$5FFF) is never a picture**: the only writes there are the game patching its own code operands ($55B2/$55B3, $5592/$5597, $4876).
* **Text page 1 ($0400) is never written.** Text page 2 ($0800-$0BFF) is written heavily, but most of that is the game using the memory as data (the maze/dot state around $0900-$0B00 and ghost state, written by $6E7E-$6EBA, $80D4, $8CAC and others), plus the text prompts. $0C00-$0FFF is also game data.
* Switches used: `$C050` graphics, `$C051` text, `$C052` full screen, `$C054` page 1, `$C055` page 2, `$C056` lo-res, `$C057` hi-res. **Mixed mode (`$C053`) is never used.**
  * **Game screen:** `$6F2F` sets graphics + full + page 1 + hi-res (seen 11 times).
  * **Prompt screens:** `$6F3C` sets text + page 2 (seen 3 times; "PLAYERS?" / rank prompts, I).
  * **`$52BB`:** sets graphics + lo-res + page 2 and fills $0800-$0BFF with `$DD` (the attract-sequence routine from TASK-021c; lo-res page 2, I).
* Final state after the game: graphics, full screen, page 1, hi-res.

## 2. The 16 instructions that write to hi-res page 1 ($2000-$3FFF) (V in the scenario)
| pc | writes | Role | Status |
|---|---|---|---|
| `$55B6` | 371,221 | OR blitter inner store (sprites, font, dots; `$55A6` routine) | V (SPRITES.md) |
| `$6A51` | 193,560 | AND-$7F blitter inner store (`$6A3F`, erases/clears palette bit) | V (SPRITES.md) |
| `$69BE` | 422,790 | inner store of the **masked clear** `$69A1`: `AND mask / ORA #$80 / STA` on a 3-byte x 10-row block; mask table = 7 shift positions x 4 bytes, base patched into `$69B8/$69B9` (`$69CD`, or `$6A04` in the pair routine `$69E9`) | V (read + entry trace): callers `$46DA $4701 $476D $47B0` (attract parade at row 100), `$69F6` (via `$69E9`, called from `$7E16`, `$7EFD`: erase around a character before/after drawing), `$6CFA` |
| `$6985` | 36,384 | **box clear** `$696A`: fills 12 rows x 8 bytes with `$80` (black, palette bit set) at the (column,row) pair `$6992[2*$50]`: (0,94) (0,16) (32,16) (0,51) (32,51) (32,94) (32,154) for `$50` = 0..6 | V (read); I: label areas of the score/HUD; called from `$70D8` (162 times, `$50` = 0 and 1) |
| `$6955`, `$695E` | 10,864 each | **tile OR** `$693A`: 8 rows x 2 bytes, `table[$0666+16*shift]` ORed onto the screen; table base patched into `$6951/$6952` (energizer tile at `$61CA`, 7 shifts x 16 bytes = `$61CA-$6239`; other bases seen: `$7191`, `$7201`, `$73C1` from caller `$7179`, attract screen, I) | V (read + trace): callers `$61C6` (energizer) and `$7179` |
| `$5918` | 72,467 | store inside the routine at `$5913` (called from the maze draw `$5D00` and `$5D45`) | I: wall cell plot |
| `$5D09` | 65,005 | wall/maze draw (`$5CF0`), `ORA $1E` then store, `$1E` = `$80` or 0 | V (read) |
| `$6183`, `$618C`, `$619C`, `$61A5` | 2,841 each | **small dot** `$6172`: 2 x 2 bytes (2 rows), `ORA` of the 7-shift table `$61A8` (2 bytes per shift: `03 00, 06 00, 0C 00, 18 00, 30 00, 60 00, 40 01`) | V (read + trace). `$6113` walks the maze map at `$623A` in a 6-pixel grid (28 columns, x from `$3B` step 6; rows from 11 step 6, 30 rows) and draws a dot when the cell's low bit is set, an energizer (`$61B6` sets the tile base `$61CA` and calls `$693A`) when it is not; called from `$566F`, `$614A`, `$445B`, `$897D` |
| `$6F02` | 1,044 | **12 x 3-byte image** `$6EE2` (`AND #$7F / ORA table,X`): base address in A/Y patched into `$6F00/$6F01`; the **fruit** images, 12 rows x 3 bytes each, stride 36 from `$6B6E` (level 1 = `$6B6E`, level 2 = `$6B92`); `$6F13` draws the level's fruit row at row `$B0`, column `$21` (index `4 x min(level,26)` into the table at `$6AFE`); also the fruit in the centre (col 19, row 102, from `$5315`) | V (read + trace) |
| `$6F50` | 122,880 | **page clear** `$6F46-$6F5D`: fills `$2000-$3FFF` with `$80` (black with the palette bit set), 15 clears of 8,192 bytes; called from `$43D9`, `$52DA`, `$5659` | V (read + trace) |
| `$7830`, `$783A` | 4,872 each | the dot/energizer routine `$7819` (`AND ($12),Y / ORA #$80 / STA`, pair of bytes per row, 7 shifted masks at `$7843`) | V (read) |

## 3. Other scenarios (TASK-027b, V): one more writer
Cartoons (hidden menu, intermissions 1-3): **17 writers = the 16 above + `$82A0`** (6,320 writes). `$82A0` is the **big cartoon sprite** routine `$8259`: animation frame = `$66 and 3` into the pointer table `$82D8` (`$82E0, $8330, $8380, $8330`), x pixel in `$60` and row in `$61`; it copies 80 bytes (4 bytes x 20 rows) to a buffer at `$7E70`, shifts the whole buffer left one pixel at a time (`$82B1`: `ASL/ROL/ROR` chains across the 4 bytes of each row, 7 bits per byte) `x mod 7` times, then **stores** (not ORs) the rows to the screen. Rank prompt scenario: the same 16 as play. Two-player and game-over scenarios are not traced separately (U), but they use the same drawing routines per the coverage work.
Self-modifying code summary (all V): the drawing routines get their table by patching operands: `$55B2/3` (OR blitter), `$6A4B/C` (AND blitter), `$6951/2 + $695A/B` (tile OR), `$69B8/9` (masked clear), `$6F00/1` (12x3 image), `$827B/C` (cartoon sprite). A C++ port replaces these with a pointer argument.

## 4. Next sub-steps
* **TASK-027c:** the colour model and the row/byte addressing (`$5B58` row address from `$10` into `$12/$13`; `$5933` converts pixel x in `$0C/$0D` into byte column `$04` and shift `$03`; tables `$5B67/$5C27`), the 7 shift positions, the palette bit, the wall-draw routine `$5D53`/`$5CF0` (cells of 6 pixels?) and its `$1D = 0` mode (docs/FORCED_CODE.md), then the screen-dump oracle tool and golden hi-res screens.
* Two-player and game-over writer traces (low priority).
