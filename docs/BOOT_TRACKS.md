# Boot tracks 0-2 (TASK-006)

Tool: `tools/boot_compare.py <image> <reference>` (read-only; unit-tested on synthetic disks in tests/test_boot_compare.py).
Reference: `DOS 3.3 System Master - 680-0210-A.dsk` from the AppleWin zip (already downloaded and approved on 2026-10-29; used read-only for comparison, nothing copied or committed).

## Results (V: byte comparison of two .dsk files in the same DOS sector order; the boot sector starts identically in both)
| Item | Pac-Man image | System Master |
|---|---|---|
| VTOC DOS release byte | 3 | 3 |
| Volume number | 254 | 1 |
| Tracks 0-2 sectors identical to the master | 11 of 48 | |
| Tracks 0-2 sectors that differ | 37 of 48 (3,412 differing bytes) | |
| ...of the differing bytes, those that differ only in bit 7 | 1,151 | |
| T0/S10 and T0/S11 | all zero | code |
| T0/S5 and T0/S6 | table-like non-zero data | zero |
| Deleted catalog entries | none | |
| Non-zero sectors owned by no file or system area | 0 (of 560 sectors, 174 are owned) | |

Two concrete patches in the image's DOS code (V: bytes; I: meaning):
* T0/S4 offset 0 (and again T0/S7 offset $9C): the master has a delay loop (`LDX #$11 / DEX / BNE` and `LDY #$12 / DEY / BNE`); the image has **NOPs** in its place (`EA EA EA EA EA`).
* In both places the 16-bit counter increment `INC $46 / BNE / INC $47` of the master reads `INC $4E ...` in the image. $4E/$4F are the monitor's random-number seed bytes (RNDL/RNDH), also bumped by the monitor's key wait.
  So this DOS spins its wait counter in the seed location. **No instruction in the statically known game code reads or writes $4E/$4F** (checked on the 10,456-byte code map), so the game does not appear to use this seed (U for indirect access).

## Interpretation
* **The boot tracks are not a stock DOS 3.3 copy**: they are a modified DOS (delay loops removed, different counter, extra table data, T0/S10-11 empty, hundreds of bit-7 differences). Whether this was done for speed, size, a different DOS build, or protection is **Unknown**.
* The master is one DOS 3.3 version; a different stock DOS 3.3 build (or an INIT-created disk) could also differ in places, so "differs from this master" is not by itself proof of tampering. The size of the difference (37 of 48 sectors) is nevertheless large.
* No hidden data: every non-zero sector belongs to DOS, the catalog, or a file. No deleted catalog entries.
* A .dsk cannot show nibble-level protection (custom address marks, half tracks, timing). The image **boots and runs in AppleWin from this .dsk** (TASK-010) and in our headless run the DOS is irrelevant: the game's startup copy overwrites DOS in RAM, and the game never calls DOS afterwards (all file I/O is the single BRUN at boot; V from the coverage runs: no DOS entry points executed).
* Consequence for the port: the boot tracks do not need to be reproduced. They matter only for the verifier (the expected hash covers the whole image, TASK-009) and for provenance (TASK-008).

## Not done / limits
No disassembly of the whole modified DOS (not needed for the port); no comparison with other DOS 3.3 builds (needs another downloaded reference: ask first); the RNG seed question is only a static check.
