# Data regions (TASK-013e)

Tools: `tools/classify_gaps.py` (gap classification), `tools/trace_readers.py` (which instruction reads which non-code bytes). Both need git-ignored `scratch/` data (cov.json from `tools/trace_coverage.py`).
No original bytes are reproduced here; only addresses and counts. Scenario: keys `1`,`N`, then 243M cycles free run (see DYNAMIC_COVERAGE.md). Labels are **Inferred** unless marked V.

## Overall split of the 27,136-byte program ($4000-$A9FF)
| Class | Bytes | % | Evidence |
|---|---|---|---|
| Code (recursive disassembly: entry $4027 + 48 dispatch-table targets) | 10,456 | 38.5% | V (all 3,340 executed instruction starts agree) |
| Data, read by running code, never executed | 15,166 | 55.9% | V that they were read and not executed; "data" is the natural reading |
| Data, pointed to by code but not read in this run | 1,368 | 5.0% | Pointer evidence only (absolute operands, `#lo/#hi` pairs) |
| Unclassified (no read, no pointer, not all zero) | 146 | 0.5% | e.g. embedded text that nothing reads (see below) |
Gaps: 60 contiguous non-code regions (list: `scratch/gaps.txt`). Largest: $96E8-$A9FF (4,888), $4A7A-$525F (2,022), $61CA-$6939 (1,904), $717D-$75F0 (1,140), $5DBF-$6112 (852).

**Caveat:** "read and never executed" proves the bytes are used as data, not that every byte in a gap is data: a gap can also hold unreached code next to a data table (e.g. the 8 unexecuted dispatch targets are only Inferred).
Code that only runs in game states this scenario never reached (play, death, sound, other levels) could still hide inside the 1,368 "referenced-only" bytes and inside gaps. Zero-filled spans inside gaps (~13% of the program is zero) are padding or empty table entries (U).

## Who reads the data (top readers; V for the counts)
| Reader | Distinct non-code bytes read | Range | Inferred role |
|---|---|---|---|
| `LDA $0666,X` at $55B1 (operand patched by 10 stores) | 2,798 | $450A-$A543 | shape/sprite table fetch; the base address is chosen at runtime, so one routine serves many tables |
| $5343 | 896 | $65BA-$6939 | copies a 896-byte source table into $623A-$65B9 (the runtime-written block from TASK-005b); $5380/$6129 then read it |
| $6A4A | 720 | $7E70-$A6E7 | another patched-operand table reader (operand $6A4B/$6A4C written by $6A28/$6A2B) |
| $5275 / $527A / $527F | 672 each | $4A7A-$5259 | three parallel 672-byte streams: one 2,016-byte table in three interleaved parts (U: what it holds; pairs with the 2,022-byte gap) |
| $7E27, $8964, $7A64, $7ABD, $880F | 300 / 267 / 161 / 127 / 111 | around $7F13, $6248-$65C8, $760C-$7807 | level/maze or position tables (U) |
| $8C76 | 82 | $8CC0-$8DBD | the dispatch index table (V, section 10 of REVERSE_ENGINEERING.md) |
| $48B7, $42E0, $53F5 | 86 / 107 / 84 | $48D9-$492E, $4351-$43D1, $5454-$54A7 | text strings for menus (V for $48D9: the mode-screen text) |
129 reading instructions in total touch the non-code bytes.

## Consequences for the reconstruction
* Most of the program is data driven through a few table-reader routines whose table base is a runtime variable. These should be reconstructed as `(table pointer, index)` accessors, with each table named by its use once understood.
* The data tables are derived from the original and fall under the legal/redistribution note in PROJECT_PLAN.md (risk 7); the reconstruction will need to decide whether to regenerate them at build time from the user's disk image (the verified-image approach) instead of committing them.

## Correction (2026-10-29, found during TASK-008)
The first version of this document (TASK-013e) used a coverage file that still contained the startup stub's own reads of its source copy ($1000-$79FF); in the game area $4000-$803E those reads were counted as the game reading data. tools/trace_coverage.py now discards reads made before the program starts, and the numbers above, in DYNAMIC_COVERAGE.md and in the ledger were recomputed. Earlier values were 15,969 bytes / 58.8% read, 711 / 2.6% referenced-only, 0 unclassified. The reader list (trace_readers / trace_tables / render_frames) was not affected: those tools start tracking after the relocation.
Example of what changed: the 224-byte copyright notice at $4064-$4143 was shown as 'read'; it is **never read** at runtime (it is referenced-only/unread text).
