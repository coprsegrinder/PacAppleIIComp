# Shape frames (TASK-015)

Tool: `tools/render_frames.py` (hooks the two blitters, records every drawn shape, renders `scratch/frames.png` + `scratch/frames.txt`). Same scenario as DYNAMIC_COVERAGE.md.
**Images are never committed** (they come from the original data; `scratch/` is git-ignored). Names below are Inferred from looking at the rendered contact sheet (V only for the sizes and draw counts).
Rendering is a simple per-pixel NTSC-artifact approximation (isolated pixels coloured by column parity and palette bit, adjacent pixels white), not an exact emulator colour match.

## How drawing works (V from the code; I for names)
A caller sets the start row `$10`, hi-res column `$04`, width in bytes `$1A`, row count `$18`, and the table base in A (low) / Y (high), then `JSR $6A20`.
`$6A20` picks the **OR blit** ($6A62 -> patches $55B2/$55B3 -> routine $55A6) or the **AND-$7F blit** ($6A28 -> patches $6A4B/$6A4C -> routine $6A3F, `ORA (ptr),Y / AND #$7F / STA`, which clears the palette bit as it draws).
Both read one row = `$1A` bytes at a time, `X` running on from row to row, and address the screen via the hi-res row tables `$5B67` (low bytes) and `$5C27` (high bytes OR $20 = page 1) indexed by the row number in `$10`.
Frames are therefore stored row-major (width bytes per row, rows one after another).

## Frame types seen (139 distinct frames, 19,523 blit sequences in 243M cycles)
| Reader | Width x rows (bytes) | Pixels | Frames | Draws | Looks like (I) |
|---|---|---|---|---|---|
| $55B1 | 1 x 9 | 7 x 9 | 29 | 360 | **font glyphs** (letters, digits, punctuation, "Points"-style labels) from the 9-byte cell area $4553-$4673 |
| $55B1 | 3 x 10 | 21 x 10 | 80 | 7,648 | **moving characters**: Pac-Man in several directions and mouth stages; ghosts (normal, scared/white); "eyes"-like small shapes. The 210-byte images are **7 consecutive 30-byte frames** each ($96FA, $97CC, $989E, ... stride $D2) - explains the 210-byte extents in TABLES.md |
| $55B1 | 4 x 7 | 28 x 7 | 3 | 5,726 | round **dots/pellets**: $811D and $8139 drawn 2,859 times each (two states of a flashing item), $450A one more |
| $55B1 | 5 x 9 | 35 x 9 | 1 | 4 | a larger text/score shape ($4526) |
| $55B1 | 7 x 7 | 49 x 7 | 2 | 215 | **wide labels**: "PLAYER #", "HISCORE", "GAME OVER" style text ($55F7, $5628) |
| $6A4A | 3 x 10 | 21 x 10 | 24 | 5,570 | the same 30-byte character frames drawn through the AND-$7F routine (likely erase or second pass, U) |

## Corrections to TABLES.md
The "49-byte shapes" are 7 x 7-byte labels, the "28-byte shapes" are 4 x 7 dot images, the 9-byte cells are single-byte-wide font glyphs, and the 210-byte images are seven 3 x 10 frames each. Roles in TABLES.md remain Inferred.

## Leads (not conclusions)
* The font cells include glyphs that read like "Â©1983" in the contact sheet. If the game really prints a 1983 copyright line, that is evidence for TASK-008 (release identity), but a drawn glyph set is not proof of the printed string: check the actual screen (TASK-013c-2).
* Still unknown: which caller draws which frame (call sites of `$6A20`/`$55A6`), the exact pixel/colour layout versus real hardware output, and the frames not used in this scenario (death animation, fruit, other ghosts' eyes).

# Call sites and screens (TASK-016)

Tools: `tools/trace_callsites.py` (groups every draw call by the calling `JSR`), `tools/render_screen.py` (renders the whole hi-res page from the headless machine). Same scenario as DYNAMIC_COVERAGE.md.
Entry points: `$6A20` (dispatcher; A/Y = table base; called from `$467C $46E9 $477C $53E6 $5850 $799E $7B4E $7DEF $7EC7 $7EEF $88D9 $8BA1`) and `$55A6` (OR blitter; its loop-back at `$55C4` is not counted as a new call).
| Caller (JSR address) | Draws | Screen position (row / hi-res byte column) | Meaning (I) |
|---|---|---|---|
| $4344 | font glyphs $4553-$4673, 1x9 | rows 11-177, cols 13-32 | text printer for the attract screens |
| $4483, $44B2 | $450A 4x7 | rows 117 and 128, col 20 | the bullet "dots" in "10 / 50 Points" |
| $44D3 | $4526 5x9 | row 154, col 12 | the "Pacman" title line pieces (U) |
| $467C, $46E9, $477C | 96FA-set 3x10 frames (and via $6A20) | row 100, cols 10-27 | the parade of Pac-Man and ghosts on the character screen |
| $559B | $55F7, $5628 7x7 | rows 6-84, col 0 | left-margin labels ("PLAYER #1", "HISCORE") |
| $58BD, $5309 | $8139 4x7 | row 105, col 21 | centre item (flashing) |
| $80F2 | $811D 4x7 | row 105, col 15 | its partner state |
| $799E, $7B4E, $7DEF | 96FA-set, 7DBC-set 3x10 | rows 7-175, cols 8-30 | the moving characters over the maze (main loop) |
| $88D9 | $A28E-$A616 sets, 3x10 (and 1x?, via $6A20) | rows 7-179, cols 8-33 | second character set (ghost colours/eyes, U) |
| $53E6, $5850, $7EC7, $7EEF, $8BA1 | single frames | fixed positions | one-off sprites (U) |
The caller sets the width ($1A) and row count ($18) itself, so the dispatcher log shows rows 0 at its entry.

## Screens identified (V: seen in AppleWin and in the headless run)
After the menus ('1' then Enter, or '1','N'), both AppleWin 1.32.1.0 and our headless machine go through the same sequence: the maze is drawn (AppleWin draws it progressively), a **maze screen with PLAYER #1, HISCORE and "GAME OVER"**,
and the **attract/title screen**: "Character / Nickname" with the ghost names (Shadow/Blinky, Speedy/Pinky, Bashful/Inky, Pokey/Clyde), a Pac-Man and ghost parade, "10 Points", "50 Points", "Pacman", **"©1983 Atari Inc."** and **"Press Space bar"**.
The headless frame at 60M instructions has the same text and layout as the AppleWin screenshot (colours differ: my renderer is an approximation; AppleWin blends NTSC colours).
So the game was **not frozen** in our earlier run: it was in its attract loop waiting for the space bar, which explains no key waits through RDKEY (the game polls $C000 directly) and no speaker clicks (no sound during attract, U).
The "©1983" glyphs seen in TASK-015 spell this line. Evidence for TASK-008: the game itself prints "1983 Atari Inc." (V). It does not by itself prove which revision/dump this is.
Screenshots and renders stay in scratch/ (git-ignored): aw_mode.png, aw_game.png, aw_game2.png (AppleWin window only), ours_*.png (headless).
