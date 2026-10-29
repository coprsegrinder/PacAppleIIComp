# Reverse Engineering Notes

Status legend: **V** = verified by direct inspection of the image; **I** = inferred; **U** = unknown.

## 1. Supplied image (V)
| Item | Value |
|---|---|
| Filename | `Pacman (1983)(Atari).dsk` |
| Size | 143,360 bytes = 35 tracks x 16 sectors x 256 bytes (standard 140K) |
| SHA-256 | `F9E19375B15852B91776776A95755DD3F57E2B20A517AA99ED8603413E0FE805` |
| SHA-1 | `F80FCF39BD5C035A665DEDE00A4522961C1FCF7C` |
| MD5 | `6D323B42925299E361579B50C60BD32E` |
| Format | DOS 3.3 sector order, volume 254, VTOC at T17/S0, catalog starts T17/S15 |
| Catalog | `HELLO` (Applesoft, 2 sectors, locked); `PAC-MAN` (binary, 108 sectors, locked) |

Hash recorded on 2026-10-29 after inspection; original file untouched (hash re-checked after tooling ran).
These hashes are what we **observed**. They are *not* yet cross-checked against any public database, so they
are not yet an authoritative "expected" hash for the verification module (see TASK-009).

Reproduce: `python -I tools/inspect_dsk.py "Pacman (1983)(Atari).dsk"` (output saved in `docs/disk_inspection_output.txt`).

## 2. Game identification (I / U)
**User statement (2026-10-29, unverified):** per online documentation the target is the **1983 US (Atari) release**, not a 1982 EU original. The user does not know which exact copy/dump this is. The image itself has not yet proven this; we treat it as the working assumption and still need an external hash match (TASK-008/009).

* **Filename says Atari, 1983.** Atari published the Apple II port of Pac-Man in 1983, so this is likely that
  release, *not* a 1982 Namco/Midway arcade-derived title. The project brief says "1982"; this needs reconciling (TASK-008).
  Release year/revision is **not yet verified** from the image contents or external sources.
* Boot: `HELLO` is `10 PRINT CHR$(4);"BRUN PAC-MAN"` (V).
* `PAC-MAN` loads at $1000, length $6A00, so it occupies $1000-$79FF (V).
* First bytes disassemble (I, by hand) as a relocation loop: sets pointers ($14/$15 and $16/$17), copies pages
  with `LDA ($14),Y / STA ($16),Y`, then `JSR $FE89` (IN#0), `JSR $FE93` (PR#0), `JMP $4027` (the real entry point).
  Disassembly confirmed by `tools/dis6502.py` (TASK-003): copy loops are `BNE $1010` (inner) / `BPL $1010` (outer). The destination of the copy is not yet confirmed.
* About 13% of the program body is zero bytes; nothing else is known about layout yet.

## 3. Copy protection / special format (U) - see also docs/BOOT_TRACKS.md (TASK-006)
Tracks 3-16 and 25-34 are completely empty and the disk is a plain DOS 3.3 layout, with no nibble-level data
available in a `.dsk`. No custom RWTS has been looked for yet. If protection is found, we stop and ask (brief section 8).

## 4. Open questions
1. Exact release/revision, and which public sources describe this same image.
2. Where the relocation loop copies code, and what is in the 35 sectors of boot tracks (standard DOS 3.3 vs. modified).
3. Memory map: code, tables, maze data, sprite/shape data, sound routines.
4. Graphics mode (probably hi-res), timing method, sound method (speaker clicks), input (keyboard/paddle/joystick).
5. Is the game deterministic (RNG source)?

## 5. First observed behavior in AppleWin 1.32.1.0 (V, 2026-10-29)
Booting a hash-verified copy (`scratch/work.dsk`, emulated Apple //e enhanced, default settings) reaches a text screen:
"PACMAN CAN BE PLAYED WITH / 1. KEYBOARD / 2. APPLE JOYSTICK / CHOOSE A MODE (1 - 2)".
So the original offers keyboard or joystick input, chosen at startup. No protection prevented booting in the emulator.
Note: the first launch showed only the emulator's splash until its license dialog was accepted by the user.
Not yet observed: title screen, attract mode, gameplay, sound.

## 6. Extraction and memory layout (TASK-004)
* `PAC-MAN` body extracted with `tools/extract_binary.py` to `scratch/extract/PAC-MAN.1000.bin` (git-ignored).
  27,136 bytes (matches the declared $6A00); body SHA-256 `EFF5B2635EDF0E3B6A227BEE0A477EB697AAEBC29EBC57089167DF5F10EE182C`.
  The 252 bytes after it in the final sector are all zero. (V)
* Independent check: each of the 107 sector-sized chunks of the body was found exactly once in the raw image (T/S list not used). The file is stored in descending sector order (the first chunk is at T18/S12). (V)
* **Relocation (I, supported by arithmetic and by a sanity check):** the startup stub copies page-by-page from the top downward,
  src $7F00..$0F00 -> dst $AF00..$3F00, i.e. the whole program moves up by **$3000**. The game therefore runs at **$4000-$A9FF**,
  entry `JMP $4027` = file offset $27. Sanity check: the call target `$54D4` made from the entry code is a clean routine start
  (variable initialisation ending in a loop), as expected if the origin is $4000. Still to confirm in the emulator (memory dump after startup).
* Heuristic code/data map: see `docs/REGION_MAP.md`. Code and data are interleaved at fine granularity.
* Side effect to remember: the move overwrites $3F00-$AFFF, which includes DOS 3.3 in RAM; the game then cannot use DOS services (U: whether it loads anything further from disk - check disk-access behavior in the emulator).

## 7. Emulator memory dump confirms the layout (TASK-005b, V, 2026-10-29)
Method: AppleWin 1.32.1.0 booted a copy of the image to the input-selection screen; the debugger was opened (F7) and
`BSAVE "main.bin",0,C000` dumped main memory $0000-$BFFF (to git-ignored `scratch/main.bin`).
* **The program runs at $4000-$A9FF (file address + $3000): confirmed.** 26,240 of 27,136 bytes are identical to the extracted file.
  All 8,212 statically reached code bytes (docs/CODE_COVERAGE.md) are identical (0 differences), so no self-modification was seen in the reached code.
* The only differing bytes are one run, **$623A-$65B9 (896 bytes)**, changed by the time the mode screen appears: runtime-written data (variables/tables), not reached code. (To investigate in 005c.)
* The original load area still holds an unmodified copy at $1000 (the relocation copies, it doesn't clear the source).
* **$0666 resolved (I):** $0400-$07FF is Apple II text page 1; at the mode screen it holds spaces ($A0). Hi-res graphics don't use that RAM, so the game
  most likely builds its $0666 table/routine there after leaving text mode. Needs a dump taken during gameplay to confirm.
* Technique notes: the `DebuggerAutoRun.txt` breakpoint (BPX 4027) did not stop execution, so it was not used. AppleWin refuses keyboard focus from background processes;
  `tools/awin_ctl.ps1` instead posts key messages directly to the window (works; F7, Esc and typed commands all verified) and captures only that window.

## 8. First headless run (TASK-013b, V, 2026-10-29)
* The game's startup uses only these ROM entries before its first key wait: $FE89, $FE93, $FC22 (VTAB: takes the row from CV $25, not A), $FDF0 (via the game's own char-output routine $4A6A, which handles CR itself with `INC $25` + VTAB then sets the text page to page 2 by ORing $08 into BASH).
  So the menus are drawn on **text page 2 ($0800)**, not page 1.
* The $623A-$65B9 run that differed from the file in the AppleWin dump is written by game code during startup: our headless run reproduces it exactly (U: which routine; find it with write-coverage in 013c).
* Menu flow: mode screen (RDKEY at $48C2: '1' keyboard / '2' joystick, stores $F5 = 0/1) -> "NORMAL OR CUSTOMIZED KEYBOARD (N/C)?" (RDKEY at $494B; 'C' leads to key-customizing; anything else jumps to $4A00).
* $4A6A is the game's own character output (CR handling, else JMP $FDF0).

## 9. Dynamic coverage findings (TASK-013c-1, V, 2026-10-29)
See docs/DYNAMIC_COVERAGE.md. Key points: the `$0666` text-page-table hypothesis (section 7) is **withdrawn for the call at $8CA6**: it is a patched `JSR` whose real target is `$901A`;
code at `$8E1E-$96E7+` is reachable only through such patched calls; `$623A-$65B9` is written by code at `$5345` and `$7B2A`. Other `$0666,X/Y` table reads are not yet examined (U).

## 10. Dispatch table (TASK-013d, V/I, 2026-10-29)
Code at $8C6C-$8C84 computes `idx = byte[$8CBE + 4*$73 + $E0 - 1]` and copies the word at `$8DBE + 2*idx` into the operand of the `JSR` at $8CA6 ($8CA7/$8CA8). The 256-byte index table is at $8CBE-$8DBD;
the 48-word routine table is at $8DBE-$8E1D (V that targets are executed code for 40 of 48; table length I, from the first routine starting at $8E1E). Looks like per-level/per-ghost behavior or movement routines (U: purpose not yet analysed; $73 and $E0 meanings unknown).

## 11. Data tables and their readers (TASK-013e, 2026-10-29)
See docs/DATA_REGIONS.md. V: 15,166 non-code bytes were read at runtime (first reported as 15,969; corrected, see DATA_REGIONS.md); the `LDA $0666,X` at $55B1 reads 2,798 distinct bytes across $450A-$A543 (its base is patched at runtime).
V: code at $5343 copies a 896-byte table from $65BA-$6939 into $623A-$65B9 at startup (this is the "runtime-written block"; the earlier note that its writer was unknown is closed).
I: three 672-byte interleaved streams at $4A7A-$5259 (read by $5275/$527A/$527F).

## 12. Shape blitter and tables (TASK-014, 2026-10-29)
$55A6-$55C6 is a sprite blitter (ORs shape rows into hi-res memory; I). Its table base ($55B2/$55B3) is patched per shape; 51 bases seen, frame sizes 9, 28, 49 and 210 bytes (docs/TABLES.md).
Word table at $8A3A (indexed by $E0 x 2) feeds the reader at $6EB7 (4 entries, possibly one per ghost, U).

## 13. Drawing model (TASK-015, 2026-10-29)
Frames are row-major byte blocks drawn by $55A6 (OR) or $6A3F (OR then AND $7F); row addresses come from $5B67/$5C27 (V). Frame types: 1x9 font glyphs, 3x10 characters (7-frame sets of 210 bytes), 4x7 dots, 7x7 labels (docs/SPRITES.md; names I).

## 14. Screens and release evidence (TASK-016, V, 2026-10-29)
Attract sequence after the menus: maze draw -> "PLAYER #1 / HISCORE / GAME OVER" -> "Character / Nickname" screen (ghost names Shadow/Blinky, Speedy/Pinky, Bashful/Inky, Pokey/Clyde; "10 Points", "50 Points", "Pacman")
-> "(c)1983 Atari Inc." and "Press Space bar". Seen in AppleWin 1.32.1.0 (window captures) and reproduced by the headless machine. Supports the 1983 Atari identity for TASK-008 (does not identify the revision).
Call sites of the draw routines: docs/SPRITES.md.

## 15. Start-of-play flow and sound (TASK-017, V headless, 2026-10-29)
space at "Press Space bar" -> "HOW MANY PLAYERS? (1 - 5)" -> `1` -> "PRESS <RETURN> TO PLAY OR <SPACE> TO GO BACK TO DEMO." -> Enter -> play (maze, score, lives icon, cherry). Speaker is silent in attract and clicks (thousands per second of play) after play starts.
The 7,451 bytes executed so far all fall inside the statically derived code (docs/CODE_COVERAGE.md); details in docs/DYNAMIC_COVERAGE.md.

## 16. Input handling (TASK-018, V, 2026-10-29)
Keyboard poll at $6F77 (direct $C000/$C010): A/Z/left/right set $63 = 1/0/3/2; Ctrl-S toggles sound by flipping the operand of the speaker BIT ($30 <-> $20) at $7BB6/$7D07; space during demo jumps to the player prompt; $E8 = demo flag, $F5 = device. Details and caveats: docs/CONTROLS.md.
Note: the earlier statement 'no opcode bytes are changed' still holds; the sound toggle changes an operand byte only.

## 17. Hidden menu and customised keys (TASK-020, V, 2026-10-29)
Players prompt: Ctrl-V, Ctrl-C -> 'RANK OR CARTOON?' (start level 1-21 into $54E1, or intermission 1-3 via $8155). Customised keyboard patches the compare constants at $6FBA/$6FB6/$6FB2/$6FBE (left/right/up/down = codes 3/2/1/0). Esc pause at $6F9A. Details: docs/CONTROLS.md.

## 18. Emulator cross-check at the players prompt (TASK-019, V, 2026-10-29)
AppleWin dump vs our machine: game area identical except the 8 bytes of table-pointer operands; see docs/EMULATOR_COMPARISON.md. Intro jingle click spacings (cycles): 2,026 / 2,274 / 2,708 / 1,716 (our machine).

## 19. Boot tracks (TASK-006, V/I/U, 2026-10-29)
The image's DOS is a modified build (37 of 48 boot-track sectors differ from the System Master; delay loops NOPed; counter moved to $4E/$4F). No hidden data. Protection: Unknown. See docs/BOOT_TRACKS.md.

## 20. Release and lineage research (TASK-007, 2026-10-29)
Public pages (second-hand, see docs/RESEARCH_007.md): Atari/Atarisoft DX5501, (c)1983; reported to be a modified Taxman (H.A.L. Labs 1981, B. Fitzgerald). The game's own screen says (c)1983 Atari Inc. (V). No public hash or disassembly found.

## 21. Release identity (TASK-008, V, 2026-10-29)
The program contains a 215-byte legal notice at $4064 beginning '(C) 1983 ATARI, INC.' (never read at runtime) and prints '(c)1983 Atari Inc.' on the attract screen; no 1982 text anywhere. An unused prompt 'BLACK & WHITE OR COLOR? (B / C)' sits at $4886. Details: docs/RELEASE_IDENTITY.md.

## 22. Verifier reference values (TASK-009, 2026-10-29)
No public hash found. Provisional rule: exact image SHA-256 or PAC-MAN body SHA-256 (docs/VERIFIER_SPEC.md, tools/verify_image.py).

## 23. Playfield table (TASK-012, V/I, 2026-10-29)
$623A-$65B9 is rebuilt by $5345 at each game start and a byte is changed by $7B10 per dot eaten (stride $20): a 32x28 playfield map, values 0/1/2 (I). $0660-$066F holds $A0 during play in AppleWin and in our machine: no table at $0666.

## 24. Pacing and scale facts for the port (TASK-024, V, 2026-10-29)
129 routines called by JSR plus 48 jump-table routines; main-loop pass ($530F) costs 2,987 cycles in 59% of passes and up to 27,950 otherwise (mean 4,823): game speed depends on cycles, there is no vblank pacing. See docs/ARCHITECTURE.md.
