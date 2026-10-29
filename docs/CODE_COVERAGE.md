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
3. **RESOLVED by TASK-013c-1 (see DYNAMIC_COVERAGE.md): `JSR $0666` at $8CA6 is a placeholder operand patched at runtime to `JSR $901A`.** Old note follows. **(U, superseded):** $0666 is in low RAM. Several instructions index a table at $0666 (`LDA $0666,X/Y`, `ORA $0666,X`).
   Resolved in part (TASK-005b): $0400-$07FF is text page 1, and the game most likely reuses it as RAM during hi-res play (**I**). See REVERSE_ENGINEERING.md section 7; confirm with a dump taken during gameplay.
4. **Coverage gap (V):** 70% of bytes are not reached from the single entry. Reasons not yet known: data tables (expected), code reached via
   RTS-dispatch or other address tables (not followed), RAM-resident code, or code only reachable from additional entry points (e.g. interrupt/other loaders).
   Unreached does **not** mean data.

5. **Emulator cross-check (V, TASK-005b):** all 8,212 reached code bytes match runtime memory at $4000 exactly; one 896-byte run at $623A-$65B9 differs at runtime (data).

## TASK-005c: what static analysis can and cannot tell us (`tools/xrefs.py`, V unless marked)
Command: `python -I tools/xrefs.py scratch/extract/PAC-MAN.1000.bin 4000 4027` (analysis script; not unit-tested, results checked by hand).
1. **Self-modifying code exists.** 21 instructions in reached code have operand bytes rewritten by other code (31 bytes in total; no opcode bytes are changed).
   Typical use: the base address of an indexed table read is patched to choose a table. Example (value names only, no code): the `LDA $0666,X` at $55B1 has its address
   rewritten by 10 different stores. So **$0666 is a placeholder base address, not a real table location** - this supersedes the earlier guess about text page 1.
   Consequence: a faithful reconstruction must model these as variables (e.g. a table pointer), and the memory dump agreed with the file only because no patching had happened yet at the mode screen.
2. **No PHA/PHA/RTS dispatch and no JMP (indirect)** were found in reached code. Code reached via other pointer tricks cannot be ruled out.
3. **Data is mostly accessed through pointers and patched addresses.** Only 125 distinct absolute addresses into the program are referenced (clusters cover ~950 bytes of the 18,930 unreached bytes);
   the rest is likely reached through zero-page pointers `(zp),Y` or patched operands - i.e. tables/shape/maze data (**I**).
4. **Tried and discarded:** heuristics that guessed "code-pointer tables" and "pointer setups" from raw bytes produced mostly false positives (e.g. repeated $8080 values) and were removed.
5. **Conclusion:** the remaining 70% cannot be classified reliably by static analysis alone. The reliable way is **dynamic**: execute the game and record which addresses are executed,
   read, and written. That requires a small 6502 core with Apple II soft-switch stubs (planned as TASK-013), cross-checked against AppleWin.
