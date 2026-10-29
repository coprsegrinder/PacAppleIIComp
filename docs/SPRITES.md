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
* The font cells include glyphs that read like "©1983" in the contact sheet. If the game really prints a 1983 copyright line, that is evidence for TASK-008 (release identity), but a drawn glyph set is not proof of the printed string: check the actual screen (TASK-013c-2).
* Still unknown: which caller draws which frame (call sites of `$6A20`/`$55A6`), the exact pixel/colour layout versus real hardware output, and the frames not used in this scenario (death animation, fruit, other ghosts' eyes).
