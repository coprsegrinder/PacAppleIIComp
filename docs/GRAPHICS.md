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
| `$69BE` | 422,790 | the single biggest writer; not decoded yet | U: decode next |
| `$6985` | 36,384 | not decoded yet | U |
| `$6955`, `$695E` | 10,864 each | pair of stores (two bytes per iteration) | U |
| `$5918` | 72,467 | store inside the routine at `$5913` (called from the maze draw `$5D00` and `$5D45`) | I: wall cell plot |
| `$5D09` | 65,005 | wall/maze draw (`$5CF0`), `ORA $1E` then store, `$1E` = `$80` or 0 | V (read) |
| `$6183`, `$618C`, `$619C`, `$61A5` | 2,841 each | four stores of one routine near `$6113` (called from `$566F`) | U |
| `$6F02` | 1,044 | not decoded yet | U |
| `$6F50` | 122,880 | clears the whole page: 122,880 = 15 clears of 8,192 bytes | I: screen clear at `$6F46-$6F5D` |
| `$7830`, `$783A` | 4,872 each | the dot/energizer routine `$7819` (`AND ($12),Y / ORA #$80 / STA`, pair of bytes per row, 7 shifted masks at `$7843`) | V (read) |

## 3. Next sub-steps
* **TASK-027b:** decode the 6 undecoded writers ($69BE, $6985, $6955/$695E, $6183-$61A5, $6F02): inputs, what they draw, which tables they read.
* **TASK-027c:** the colour model and the row/byte addressing formulas ($5B67/$5C27 tables vs the arithmetic), the 7 shift positions, the palette bit; then the screen-dump oracle tool and golden hi-res screens.
* Also open: repeat the writer trace on the cartoons, rank prompt, two-player and game-over scenarios to confirm the list of 16 is complete.
