# Table catalog (TASK-014)

Tools: `tools/trace_tables.py` (records, for each self-modified table reader, which base addresses it used and which offsets it read). Same scenario as DYNAMIC_COVERAGE.md
(keys `1`,`N`, then 243M cycles free run), so only tables used in that scenario appear. **Extents are what was read (lower bounds), not proven table sizes.** Role names are Inferred.
Only addresses and sizes are listed (no original bytes). Re-run: `python -I tools/trace_tables.py` (~2.5 min; output in scratch/tables.txt, tables.json). Re-run after the refactor gave identical output.

## Reader $55B1: the shape blitter (I, from the code: `LDA tbl,X / ORA (ptr),Y / STA (ptr),Y`)
Routine $55A6-$55C6 ORs `$09` bytes per row into the hi-res row addressed by pointer $12/$13, for `$18` rows, taking shape bytes from a table whose base is patched into $55B2/$55B3 (by $6A62/$6A65, A = low, Y = high).
Index X ($03) runs on from row to row. 51 different bases were used (71 reader/base pairs in total over all readers):
| Group | Bases | Per-base bytes read | Notes |
|---|---|---|---|
| Two larger shapes | $450A (28), $4526 (45) | 28, 45 | first entries of the shape area |
| **9-byte cells** | $4553-$4673, 29 bases, stride mostly 9 (25 of 30 gaps) | 9 | small sprites/glyph cells, back to back; a few gaps are missing cells not drawn in this run |
| 49-byte shapes | $55F7, $5628 | 49 | very frequently drawn ($55F7: 10,339 byte reads) |
| 30-byte sections | $7DBC, $7E16, $7E34, $7E70 | 30 (read at offsets 180-209, 90-119, 60-89, 0-29) | four windows of the same 210-byte block ending at $7E8D: the block $7E70.. is read starting at different offsets (U) |
| 28-byte shapes | $811D, $8139 | 28 | most-read shapes (80,052 byte reads each) |
| **210-byte images** | $96FA, $97CC(180 read), $989E, $9970, $9A42, $9B14(180), $9BE6, $9CB8, $9D8A, $A28E, $A3A0, $A472 | 210 (=30 rows x 7 bytes?, U) | large frames; stride $D2 = 210 between consecutive ones; $9D8A-$A28E and $A28E-$A3A0 gaps are other data or unused |
So the sprite/shape area is a set of fixed-size frames, 9 / 28 / 49 / 210 bytes. The 210-byte frames run from $96FA to $A543 (about 3.7 KB; includes the 4,888-byte gap noted in DATA_REGIONS.md).

## Other self-modified readers
| Reader | Patched operand | Bases seen (bytes read) | Inferred role |
|---|---|---|---|
| $6A4A `LDA tbl,X` | $6A4B/$6A4C (by $6A28/$6A2B) | $A472, $A544, $A616 (210 each, same frames as above); $7E34 / $96FA / $9BE6 (30 each, at offsets 60-119) | the other half of the blit pair: reads the same image frames (erase/restore or a second drawing pass, U) |
| $6950 and $6959 `LDA tbl,X` | $6951/$6952, $695A/$695B (by $61B8-$61C3) | $61CA (offsets 16-95), $7191, $7201, $73C1 (offsets 64-95 / 80-95) | maze / board data read two bytes at a time (X and X+1), 79 bytes per window (U). $61CA is the first byte of the 1,904-byte gap |
| $69B7 `LDA tbl,X` | $69B8/$69B9 (by $69FD/$6A00) | $69CD, $6A04 (27 each) | 27-byte rows, 55 bytes apart: two small tables (U) |
| $6EB7 `LDA tbl,X` | $6EB8/$6EB9 (loaded from word table $8A3A, indexed by $E0 x 2) | $8A53, $8A61, $8A6F (16 read each, base stride 14) | **word table at $8A3A-$8A43 (E0 = 1..4 gives $8A44, $8A53, $8A61, $8A6F)**; entries $E0=2,3,4 were used; four entries suggests four ghosts (U) |
| $6EFF `ORA tbl,X` | $6F00/$6F01 (by $6EE2/$6EE5) | $6B6E (36) | a 36-byte shape ORed in (U) |

## Startup copy and plain-address streams (not patched)
* $5343: copies 896 bytes from $65BA-$6939 into $623A-$65B9 (V from the read/write trace; 013e). $5380 and $6129 then read the copy.
* $5275/$527A/$527F: three streams of 672 bytes each over $4A7A-$5259 (I: one 2,016-byte table in three interleaved parts).
* $8C76: dispatch index table $8CBE-$8DBD and word table $8DBE-$8E1D (V; REVERSE_ENGINEERING.md section 10).

## Consequences
* A faithful reconstruction needs these as data blobs with named base addresses and a "set pointer, then blit n bytes x m rows" routine; the patched-operand trick maps directly to a pointer argument.
* Not yet known: the pixel layout of each frame (needs rendering to an image: a candidate next task), who calls $6A62 with which base (a table of call sites would name every sprite), and the table users outside the readers listed here.
