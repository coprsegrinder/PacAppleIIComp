# Disk-image verifier: rule and reference hashes (TASK-009)

Status: **PROVISIONAL**, pending the user's confirmation. No public source confirms the hashes (searches below). Reference implementation: `tools/verify_image.py` (tests/test_verify_image.py, 6 tests).

## Search for a public hash (V: what was searched; browsing only)
Three web searches, 2026-10-29: the image's SHA-1 string; its MD5 string with "Pac-Man Apple II"; its file name `Pacman (1983)(Atari).dsk`. **No page contained either hash or the file name.** Earlier (TASK-007) catalogue pages listed no checksums either.
(One search summary guessed the file might be an Atari 8-bit disk. That is wrong: the image is an Apple II DOS 3.3 disk, verified by its catalog, boot code and by running it in AppleWin.)
Not tried: dedicated preservation databases that need an account or a download, and asking the user for the original source of the file. SHA-256 was not searched (public lists seldom use it).

## Reference values (all derived from the one image the user supplied; V for the computation, unverified publicly)
| Name | Value |
|---|---|
| Whole image SHA-256 (143,360 bytes) | F9E19375B15852B91776776A95755DD3F57E2B20A517AA99ED8603413E0FE805 |
| Whole image SHA-1 / MD5 | F80FCF39BD5C035A665DEDE00A4522961C1FCF7C / 6D323B42925299E361579B50C60BD32E |
| Game file `PAC-MAN` (DOS 3.3 binary, load $1000, length $6A00 = 27,136 bytes) body SHA-256 | EFF5B2635EDF0E3B6A227BEE0A477EB697AAEBC29EBC57089167DF5F10EE182C |

## Rule recommended
1. Accept a 140K (143,360-byte) image in **DOS sector order** (.dsk/.do). Other containers (.po, .woz, .nib, .2mg) are **not supported yet**; the verifier says so instead of guessing.
2. **Exact match**: whole-image SHA-256 equals the value above, or
3. **Program match**: the image parses as DOS 3.3 (VTOC at T17/S0, catalog chain), has a binary file named `PAC-MAN` with load $1000 and length $6A00, and the SHA-256 of its body equals the value above.
4. Anything else is refused with a plain-English reason. The verifier never modifies the image and never raises on malformed input (checked on synthetic broken catalogs).

Why accept the program match: the same game data can sit on a disk with different DOS tracks, volume number or free space (we know this disk's DOS is non-stock; BOOT_TRACKS.md). The program bytes are what the port depends on. Checked: altering a boot-track byte or the volume number still gives "game-program" match; flipping one program byte gives "not-verified"; the unrelated System Master gives "not-verified" (V, real runs).

Risks and open points
* The reference is a single image; if it is a modified dump of the retail disk, genuine retail disks may fail to verify (U). The user can supply another dump later to add a second accepted body hash.
* Hashes are not publicly corroborated (this document says so; the app's launch text must not claim "official release" status, only "matches the reference image the project was built from").
* Rule 4 of the project brief (the game must not read the disk image at runtime) means this verification happens once at launch; how the game data reaches the port is a design question for the port plan (e.g. extract tables at first run into the user's own cache after verification, versus build-time embedding), not decided here.
