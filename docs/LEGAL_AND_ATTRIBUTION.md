# Legal notes and attribution

This file records **concrete** facts and decisions only. It is not legal advice. The owner has consulted a lawyer and treats the project as a non-commercial hobby in a legally grey area; nothing here argues that position.

## Ownership and credits
* The game is **(C) 1983 Atari, Inc.** (a notice beginning "(C) 1983 ATARI, INC. ALL RIGHTS RESERVED" is inside the program, and "(c)1983 Atari Inc." is shown on its title screen; `docs/RELEASE_IDENTITY.md`). Published under the Atarisoft label (catalogue: DX5501).
* "Pac-Man" is a Namco/Bandai trademark and arcade game; no affiliation or endorsement is claimed.
* **Reported lineage (unverified):** public pages say Atari's Apple II Pac-Man is a modified version of **Taxman** (H.A.L. Labs, 1981, by Brian Fitzgerald). The image contains no such credit, and the Taxman source has no known public host (`docs/RESEARCH_007.md`). If this is ever confirmed, credit Fitzgerald/H.A.L. Labs here.
* Original hardware and ROM behavior belongs to Apple Computer; no ROM is used or copied (stand-ins are written from documented behavior).

## What the repository contains and does not contain
* **Contains:** our own source code, tests, notes, hashes of the reference image, addresses and sizes of the program's parts, and a few short strings from the game quoted as evidence (menu texts, the copyright line).
* **Does not contain (enforced by `.gitignore`, and by habit):** the disk image, extracted program or data bytes, original graphics/sound data, screenshots or renders of the game, memory dumps, emulator binaries, ROMs. Test vectors made from original data stay local, and those tests skip when the data is absent.
* The planned **data pack** (decision D1-A) is built on the player's machine from the player's own verified image and is never distributed.

## Third-party tools used (development only, not distributed, not linked)
| Tool | Use | License | Notes |
|---|---|---|---|
| AppleWin 1.32.1.0 | reference emulator for comparisons | GPL-2.0 | downloaded with the user's approval; `third_party/`, git-ignored |
| DOS 3.3 System Master (bundled in the AppleWin zip) | read-only comparison of the boot tracks | Apple software; status of this use not assessed | not copied or committed |
| Python 3.14, Visual Studio Build Tools, CMake, Windows SDK | build and analysis | their own licenses | already installed |

## Decisions that affect this file
* The final result must be based only on the supplied `.dsk` (user, 2026-10-29). Outside references may only help understand routines; no code, data or assets from them enter the port.
* No outside download, repository publication or release without the user's explicit approval (project rule 2).

## Before any publication (checklist, not yet done)
1. Re-scan the repository for original bytes (a scan tool is not written yet).
2. Review the short quoted game strings in the notes.
3. Decide whether the analysis notes (addresses, structures, timing data) should be published; they describe the original program in detail.
4. Re-check this file with the owner's lawyer's advice.
