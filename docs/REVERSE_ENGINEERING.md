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
* **Filename says Atari, 1983.** Atari published the Apple II port of Pac-Man in 1983, so this is likely that
  release, *not* a 1982 Namco/Midway arcade-derived title. The project brief says "1982"; this needs reconciling (TASK-008).
  Release year/revision is **not yet verified** from the image contents or external sources.
* Boot: `HELLO` is `10 PRINT CHR$(4);"BRUN PAC-MAN"` (V).
* `PAC-MAN` loads at $1000, length $6A00, so it occupies $1000-$79FF (V).
* First bytes disassemble (I, by hand) as a relocation loop: sets pointers ($14/$15 and $16/$17), copies pages
  with `LDA ($14),Y / STA ($16),Y`, then `JSR $FE89` (IN#0), `JSR $FE93` (PR#0), `JMP $4027` (the real entry point).
  The destination of the copy is not yet confirmed.
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
