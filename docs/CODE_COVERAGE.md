# Code Coverage of Recursive Disassembly

Tool: `python -I tools/recdis.py scratch/extract/PAC-MAN.1000.bin 4000 4027 scratch/rec_entry` (TASK-005a).
Listings stay in git-ignored `scratch/` (they contain original code); only these summaries are committed.

## Result from entry $4027 alone (V: reached by following JSR/JMP/branches; assumes no self-modifying code)
| Metric | Value |
|---|---|
| Instructions reached | 3,963 |
| Bytes reached | 8,212 of 27,136 (30.3%) |
| Distinct labels (branch/call/jump targets) | 566 |
| Indirect jumps (JMP (addr)) found | 0 |
| Overlapping instruction pairs | 6 (all explained below) |

## Findings
1. **BIT-skip idiom (V):** at $6FC3/$6FC6/$6FC9 the byte $2C (BIT abs) hides the following `LDA #3 / #2 / #1`; entering at +0 or +1 gives different behavior.
   This is deliberate and explains all 6 overlaps. The reconstruction must preserve it (e.g. as a routine with a parameter or three entry points).
2. **ROM calls (V):** the code calls Apple II monitor routines at $FC22, $FD0C, $FD6F and $FDF0 (names: to be confirmed against the monitor listing; probably VTAB, RDKEY, GETLN, COUT1 - **I**).
   It uses the ROM for text/keyboard input only so far.
3. **Open: `JSR $0666` at $8CA6 (U):** $0666 is in low RAM. Several instructions index a table at $0666 (`LDA $0666,X/Y`, `ORA $0666,X`).
   Hypothesis (**I**): $0666 holds data/code created at runtime (e.g. a lookup table or generated routine). Must be checked in the emulator.
4. **Coverage gap (V):** 70% of bytes are not reached from the single entry. Reasons not yet known: data tables (expected), code reached via
   RTS-dispatch or other address tables (not followed), RAM-resident code, or code only reachable from additional entry points (e.g. interrupt/other loaders).
   Unreached does **not** mean data.
