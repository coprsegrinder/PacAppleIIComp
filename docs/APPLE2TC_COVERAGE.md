# apple2tc results for PAC-MAN (TASK-029h3, 2026-11-02)

Inputs: the game as it is after its own relocation (`tools/a2tc_runtime_image.py`: load $4000, $6A00 bytes, entry $4027, made by running the verified program in the oracle machine) and the merged run data of 30 recorded scenarios (`tools/a2tc_collect.py`, `tools/a2tc_scenarios.py`, `tools/a2tc_merge.py`; 1,105 branch targets and 1,417 branch origins inside the game after filtering). Outputs stay in git-ignored `scratch/` (they contain original code): `a2tc_pac.asm` (36,497 lines), `a2tc_pac.c` (16,478 lines, `--simple-c`), `a2tc_cov.txt`.

Note: running the program file as it is on disk (load $1000) does not work for apple2tc: it analyses the loaded range ($1000-$79FF), but the game runs at $4000-$A9FF. The relocated image is the right input.

## Coverage numbers (apple2tc's own report, range $4000-$A9FF = 27,136 bytes)
| Class | Bytes | % |
|---|---|---|
| Code found | 10,386 | 38.3 |
| Declared data | 0 | 0 (nothing declared yet) |
| Unknown, all zero | 3,359 | 12.4 |
| Unknown, non-zero | 13,391 | 49.3 (tables, images, strings: data, not yet declared) |

## Cross-check with our own earlier analysis (`tools/a2tc_compare.py`; V)
- apple2tc code (from dynamic data only) 10,384 bytes; our static code set (recursive disassembly from $4027 + the dispatch-table targets) 10,456 bytes; 10,380 bytes in both.
- **Only ours, 76 bytes: $951F-$956A.** Code that none of the 30 recordings reaches (our static analysis follows it from a dispatch table). This is the whole difference: everything the scenarios execute, apple2tc finds, and nothing more is executed that our static set does not contain.
- **Only apple2tc, 4 bytes: $53B5-$53B7 and $8A3B.** Mis-decoded bytes at the edge of data (a `BRK`-looking run inside a table at $53B2-$53B8 and one byte at $8A3B); apple2tc itself reports invalid instructions at $53B8 and $8A3C. They come from edge targets/generation snapshots and are not real code (Inferred; check in 029h5).
- Meaning for "100 % understood": all 10,456 code bytes are known; 10,380 are executed by recordings (99.3 %); the other 76 are known code that has never run (what it does is read from the listing, not observed).

## What the C output is
A `--simple-c` emulation-style translation (switch per basic block, memory images `s_mem_4000` and the ROM `s_mem_d000`), i.e. "working C", not readable C: variables are CPU registers and memory. It needs apple2tc's runtime library (`lib/a2io`, `a2host`) to build and run. Not compiled or run yet (029h4).

## Still to do
- Declare the data tables with `--known-data` (from DATA_REGIONS.md) to reduce the 49 % "unknown non-zero" to a real list of unidentified bytes (029h5).
- Decide what to do with the 29 distinct final code states (21 self-modified operands): apple2tc output has a single image; check which state it holds and that the 29 states differ only in operand bytes (029h4).
- Compile and run the generated C against the oracle (029h4).
