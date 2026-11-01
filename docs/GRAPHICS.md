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
| `$6985` | 36,384 | **box clear** `$696A`: fills 12 rows x 8 bytes with `$80` (black, palette bit set) at the (column,row) pair `$6992[2*$50]`: (0,94) (0,16) (32,16) (0,51) (32,51) (32,94) for `$50` = 0..5 (**corrected in TASK-031c: I had listed a 7th pair (32,154); those bytes are the code `JSR $7D9A` at `$699E`**) | V (read); I: label areas of the score/HUD; called from `$70D8` (162 times, `$50` = 0 and 1) |
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

## 4. Addressing and pixel model (TASK-027c)
Test: tests/test_hires_addressing.py (4 tests, V against the tables in the original).
* **Row address** (`$5B58`: row `$10` -> `$12/$13`): the tables `$5B67` (low) and `$5C27` (high, ORed with `$20` = page 1) equal the standard Apple II formula `$2000 + (y and 7) * $400 + ((y >> 3) and 7) * $80 + (y >> 6) * $28` for all 192 rows (V). **The port computes it; no table needed.**
* **Pixel x to byte and shift** (`$5933`): `column = x div 7`, `shift = x mod 7` (tables `$5A58` and `$5958`, V for x = 0-255). For x >= 256 (high byte `$0D` nonzero) the routine uses `X = x_low + 4` and adds 36 to the column, which equals `x div 7` / `x mod 7` again (V for x = 256-507). Screen width is 280 pixels = 40 bytes, so x never exceeds 279.
* **Single pixel plot** (`$5913` set, `$591B` clear): masks `$5925` = `1 << shift` and `$592C` = `not (1 << shift)`; set = `OR`, clear = `AND mask` then `OR $1E` (`$1E` is `$80` or 0, i.e. the palette bit is kept or cleared) (V).
* **Sprite images** are stored as 7 pre-shifted copies (shift 0-6), one per x mod 7; a blit takes copy number `$03` (V for the 3x10 character frames, SPRITES.md; 16-byte tile and 2-byte dot tables use the same rule; the cartoon sprite shifts at run time instead, section 3).
* **Colour model (standard Apple II hi-res; V against AppleWin for one gameplay screen, section 9):** each byte holds 7 pixels (bit 0 leftmost) plus the palette bit 7. A lone lit pixel is violet (even screen column) or green (odd) when bit 7 = 0, blue or orange when bit 7 = 1; two adjacent lit pixels are white; unlit is black. The game fills the screen with `$80` (black with palette 1) and uses bit 7 per object, so colour is set by which table (bit 7) and by the pixel parity. For the port the safe rule is: **store the 8,192-byte page exactly like the original and convert page to colours in one separate display function** (so the colour model can be changed or checked without touching the game).

## 5. Golden screens (TASK-027c, V)
`tools/dump_hires.py golden` replays a fixed scenario (boot, `1` + Return, 14 attract checkpoints every 5M instructions up to 70M, then a game with keys from `random.Random(7)`, checkpoints after game blocks 50, 100, ... 400) and stores the **SHA-256 of the 8,192-byte page** at the 22 checkpoints in `tests/golden/hires_hashes.json` (hashes only, no pictures or original data). `tests/test_golden_hires.py` replays it and compares (about 3 minutes, passes: the scenario is deterministic, and all 22 pictures differ). The C++ hires layer will be tested by producing the same pages from the same drawing commands and comparing the same hashes.

## 6. Maze walls (TASK-027d, V)
Model: `tools/wall_model.py`. Test: tests/test_wall_model.py (3 tests, about 30 s). The routine `$5D53` (called once per maze draw from `$5662`, right after the page clear and the video-switch set-up) reads a **426-byte table of 142 triples**:
* 72 **horizontal** triples `x0, x1, y`: pixels from `x0` to `x1` inclusive on row `y` (`$5CF0`), and every touched byte gets bit 7 set (`$1E = $80`);
* 70 **vertical** triples `x, y0, y1`: one pixel per row from `y0` to `y1` (`$5D3D`); the palette bit is left as it is.
* **Flag `$1D` is the wall colour mode.** `$1D <> 0` (always, in the real game): table `$5F69`, and a horizontal wall plots **every second pixel** (step 2), which on a colour display gives a solid single-colour line. `$1D = 0` (the forced bytes of docs/FORCED_CODE.md): table `$5DBF` and step 1 (adjacent pixels, which show white). The two tables differ by one in a few coordinates (the same maze for the other colour mode).
* **Verified:** the model reproduces the oracle's 8,192-byte page right after `$5D53`, for the normal mode (1,327 non-black bytes) and for `$1D` forced to 0 (the previously unused path), byte for byte; the two modes' pages differ. So the 20 forced bytes at `$5D31-$5D3C` and `$5D66-$5D6D` are now specified and tested.

## 7. Text printer (TASK-027d, V)
Model: `tools/text_model.py`. Test: tests/test_text_model.py (about 35 s; at least 10 distinct strings from the attract screens, each compared page-before + model = page-after). Entry `$42D0`: string record = pointer `$14/$15` + Y; record = x column, y row, then codes until a code with bit 7 set. Code `$20` = blank (advance only); `$21` = glyph 32 (the image at `$4673`); codes 1 and 2 = glyph drawn 2 rows lower; any other code c = glyph c. A glyph is 1 byte x 9 rows at `$4553 + 9c`, ORed on the page with the OR blitter at shift 0. Column +1 per code. After each glyph the game polls the keyboard (`$6F5E`) and waits (`$6CE8`, X = $50): the typing delay; the picture does not depend on it.

## 8. Data tables the drawing code reads (all outside the code map, so inside the data pack; test: tests/test_graphics_tables.py)
| Table | Address | Size |
|---|---|---|
| font glyphs (33 x 9) | `$4553` | 297 |
| wall tables (`$1D = 0` / `$1D <> 0`) | `$5DBF` / `$5F69` | 426 each |
| dot image shifts / energizer tile shifts | `$61A8` / `$61CA` | 14 / 112 |
| maze dot map (28 columns x 32, I: columns of cells) | `$623A` | 896 |
| masks for the masked clear | `$69CD`, `$6A04` | 28 each |
| fruit-by-level pointers / fruit images (8 x 36) | `$6AFE` / `$6B6E` | 108 / 288 |
| cartoon big sprite frames (3 x 80) and pointers | `$82E0`, `$82D8` | 240 / 8 |
| hi-res row tables, pixel column/shift tables | `$5B67`, `$5C27`, `$5A58`, `$5958` | **not needed by the port: replaced by formulas (section 4)** |
Character/ghost frames, dot and label images and the `$7191/$7201/$73C1` tile bases are listed in SPRITES.md.

## 9. Colour check against AppleWin (TASK-027e, V for one screen; corrected after a re-check)
Tool: `tools/colour_check.py`; test: tests/test_colour_model.py (skipped if the files are missing). No new emulator session was needed: the 2026-10-29 comparison (EMULATOR_COMPARISON.md) had saved a **window-only screenshot** (`scratch/aw_play.png`, 1175 x 776) and a **memory dump of the same Esc-paused state** (`scratch/gp.bin`, hi-res page at `$2000`).
Rule tested: lit pixel with a lit horizontal neighbour = white; isolated lit pixel = violet (even x) / green (odd x) with palette bit 0, blue (even x) / orange (odd x) with palette bit 1.
**Correction.** The first version of this check reported 97.3% (58 misses) and I said the misses were "consistent with AppleWin's colour-blending filter". That explanation was **wrong**: re-examining the 58 misses showed they were clustered in text and sprites and the crude NTSC model I tried did not remove them; the real cause was my screenshot sampling offset, which was 4 pixels (one hi-res pixel) off because the first fit used brightness. With the offset calibrated by agreement (offset (11, 4), scale 3.99), **the rule gives 2,172 of 2,172 isolated lit pixels right (100%)**: blue 1,953, orange 128, violet 52, green 39. The control is fair: the flipped parity convention, calibrated the same way, reaches only 2.2% of 1,276 classified pixels.
White class: lit pixels with a lit neighbour were observed white in 1,198 of 1,408 (85.1%). The split is telling: **interior pixels (both neighbours lit) 254 of 256 white (99.2%); run-end pixels (only one neighbour lit) 944 of 1,152 (81.9%)**. So the non-white cases are almost all at the ends of lit runs, which is where an NTSC-type colour fringe would appear. This is an observed pattern; I did not identify AppleWin's filter or model it (a crude NTSC decoder I tried did not improve the agreement), so the mechanism stays Unknown and affects only the look of run ends, not the model of which pixels are lit.
Maze walls: isolated pixels, palette bit 1, even x = **blue**; this is why `$1D <> 0` walls look solid blue and `$1D = 0` walls would look white.
Limits (U): one screen, one moment; the half-pixel shift AppleWin applies to palette-1 pixels and its blur are filter effects, not part of the model; the attract screens and the cartoons are not colour-checked. The golden hashes cover the page contents, not the colours.

## 10. Next
The graphics specification is complete enough to start the C++ hires layer (TASK-027 proper): row formula, pixel plot, OR/AND blitters with a table pointer, box and masked clears, tile/dot/12x3/cartoon drawing, wall routine (both modes), text printer, page clear, and a separate page-to-colour display function; all with Python models or oracle traces to test against, plus the 22 golden hashes. Open (low priority): two-player and game-over writer traces; colour check of attract and cartoon screens.
