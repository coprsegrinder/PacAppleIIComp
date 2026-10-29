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

## 3. Copy protection / special format (U)
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
