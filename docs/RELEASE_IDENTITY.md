# Release identity (TASK-008)

Date: 2026-10-29. Inputs: the image itself (V = verified by inspection or by running it), docs/RESEARCH_007.md (public web pages, second-hand), docs/BOOT_TRACKS.md.

## Evidence found in the image (V)
| Item | Where | Notes |
|---|---|---|
| "(C) 1983 ATARI, INC. ALL RIGHTS RESERVED. UNAUTHORIZED REPRODUCTION, ADAPTATION, DISTRIBUTION, PERFORMANCE, OR DISPLAY OF THIS DOCUMENT, THE ASSOCIATED COMPUTER PROGRAM, OR THE AUDIOVISUAL WORK IS STRICTLY PROHIBITED." | game program at $4064-$413F (file offset $0064) | embedded 215-byte notice; **never read at runtime** (not referenced by any executed code in 6 scenarios; the earlier "read" mark was a tracking error, corrected in DATA_REGIONS.md) |
| "(c)1983 Atari Inc." drawn on the attract screen, then "Press Space bar" | attract screen (docs/SPRITES.md) | seen in AppleWin and in the headless run |
| Ghost names Shadow/Speedy/Bashful/Pokey with nicknames Blinky/Pinky/Inky/Clyde | "Character / Nickname" screen | the licensed arcade names |
| Disk: DOS 3.3 (modified boot tracks), volume 254, files HELLO (Applesoft, runs BRUN PAC-MAN) and PAC-MAN (binary, load $1000, length $6A00), both locked | docs/BOOT_TRACKS.md, REVERSE_ENGINEERING.md | |
| Text scan of the whole image (any-case ASCII and high-bit ASCII) for "1982", "Taxman", "H.A.L.", "Fitzgerald", "Namco", "Midway", "Bally": **no hits**; the only copyright/date text is the Atari 1983 notice above | | covers text stored as plain or high-bit ASCII; other encodings and drawn pictures were not scanned |
| A prompt "BLACK & WHITE OR COLOR? (B / C)" at $4886 that is **never shown** in any run and is not referenced by static code | game program | likely left over from an earlier code base (I: weak hint toward a derived program; see below) |

## Evidence from outside (second-hand; docs/RESEARCH_007.md)
* A catalogue lists the Atari/Atarisoft Apple II Pac-Man as manufacturer no. DX5501, copyright 1983, 48K, DOS 3.3, with no other Apple II Pac-Man release on that page.
* Wikipedia says Atari published a modified version of Taxman (H.A.L. Labs, 1981, Brian Fitzgerald) as the official Apple II Pac-Man under the Atarisoft label.

## Reconciliation
| Question | Answer | Status |
|---|---|---|
| Is this the 1983 Atari release? | **Yes**: the program itself says (C) 1983 ATARI, INC., on a legal notice and on screen; a catalogue entry for the retail release (DX5501) agrees. | **Verified** (identity of the work and year) |
| Was the brief's "1982" right? | Nothing in the image or in the pages read supports a 1982 *Atari* Apple II release. The 1982 may refer to the arcade/other platforms, to Taxman 2 (1982) or to a different region's version. | **Unresolved discrepancy in the brief**; the project uses 1983 |
| Which revision/dump is this exact image? | No revision text in the image, no public hash found, only one image available. | **Unknown** (TASK-009 for the hash) |
| Is it derived from Taxman? | Reported by public pages; the image has no Taxman/H.A.L./Fitzgerald text; the unused "BLACK & WHITE OR COLOR?" prompt and the modified DOS are consistent with a derived or re-hosted program but do not prove it. | **Inferred (unverified)** |
| Is the DOS on the disk Atari's? | The boot tracks are a modified DOS 3.3 (docs/BOOT_TRACKS.md); who modified it is unknown. | **Unknown** |

## Decision (recorded in docs/DECISIONS.md)
Refer to the work in all project documents as **"Pac-Man, Atari/Atarisoft, 1983, Apple II (DOS 3.3 disk), the supplied image with SHA-256 F9E19375B15852B91776776A95755DD3F57E2B20A517AA99ED8603413E0FE805"**. Do not claim a revision number. The attribution document must credit Atari (1983) as the copyright holder named in the program and note the reported Taxman lineage as unverified.
