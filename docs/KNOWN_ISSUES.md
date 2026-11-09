# Known issues and open questions

Last updated 2026-11-05. The game is complete and playable; this note lists what is open, unverified or limited.

## What was not (or could not be) verified
* **Controllers (and their vibration) were not tried on hardware.** The Xbox (XInput), DualShock 4 and DualSense paths are tested with made-up reports built from the devices' documented layouts (`port/tests/test_gamepad.cpp`) and the menus with injected states; no controller was plugged in while they were written. A DualSense over Bluetooth and a DualShock 4 of the second revision are the least certain.
* **Achievements:** the rules are tested with made-up facts and the real driver's fruit and pellet events. Not exercised end to end: an intermission watched to its end, 3 minutes without eating, and a whole game. The set's wording is read by the port (the readings are listed in `docs/MODERNIZATION.md`); the set's own logic was not available, so an edge case may differ from the original set.
* **Level 23's fruit:** the original's fruit table has an image of its own for level 23 (a quirk of the data); the achievement for the key counts level 13 and up.
* The near-ghost delay (the original's extra tick for Pac-Man when a ghost is close, from level 5 on) was rebuilt from the code in TASK-029d1b and has been seen firing and checked against the oracle since 2026-11-09 (TASK-034-NEARGHOST, docs/GHOST_RULES.md).
* The drawing gaps of the ledger (`TASK-029g10`) are closed (TASK-034-DRAW, 2026-11-09, docs/M4_PLAN.md): the door's low bit with flag 1 is reached by scenario K and proven by changing the code; the level icon is checked for every level from 2 to 26; the player switch is checked with 2, 3 and 5 players across a whole rotation. What the drawing comparison still does not cover: the pauses and sounds inside the death and level-end sequences (timing models, docs/ROUTINE_AUDIT.md) and the intermissions' own drawing (checked separately in the cartoon tests).
* The original's analogue paddle / joystick routines (`$701E`, `$7041`, `$7053`, `$706B`) are ported since 2026-11-08 (TASK-034-PADDLE) and used whenever the first menu chose the controller; only the paddle timer hardware is a model (docs/CONTROLS.md). With a controller the keyboard does not steer (as in the original), and Pac-Man's steps take about 6 % longer (the paddle timing, as in the original).
* The reference model of the original (the oracle) is checked against an Apple II emulator at a few checkpoints only; there is no frame-synchronised or audio comparison with real hardware.
* **Facts about the original that are not established:** which revision of the game the supplied image is, whether its modified DOS is Atari's, and whether it has copy protection (a `.dsk` cannot show that); a reported lineage from an earlier game (Taxman) is unverified. There is no public checksum for this image, so "verified" means "equal to the reference image the project was built from" (`docs/VERIFIER_SPEC.md`).

## Limits of the port
* Windows 10/11 only (Direct3D 11, WASAPI). Disk images: 140K DOS-order `.dsk`/`.do`; `.po`, `.woz`, `.nib` are refused.
* Games for two to five players are started from the keyboard (a controller starts a one-player game, or any number through the D-pad choice on the players prompt).
* **Original Colour** fills the unlit dot between two lit dots of the same colour (the way a colour display shows the game's every-second-dot patterns). That is a display choice, not the exact dot picture; the exact dots are what Native Black & White shows.
* The colour model is the usual Apple II hi-res one (violet/green, blue/orange, white); AppleWin's extra NTSC blur and the half-dot shift of palette-1 dots in colour are not reproduced.
* The FPS options (30 to Unlimited with smooth motion) were built, found to need approximations of the original's picture, and **removed**; the picture rate is the original's (60.05 a second). The code of the smooth motion and its pacing remains in the source, unreachable from the menu (`app/motion.h`, `app/pacing.h`), with its tests.
* The data pack's integrity uses unkeyed hashes: it detects accidents, not deliberate edits.

## Repository and release
* The Apple II ROM dumps used during research (Integer BASIC, the original monitor, Programmer's Aid) were tracked in early commits and are removed from the tree; **they remain in the history**. The two ROMs the program needs are kept on purpose (`NOTICES.md`).
* The task ledger (`docs/TASK_LEDGER.md`) and the plans were kept during development and are not rewritten: some rows still read "Planned" for work that later tasks completed. The newest facts are in the README, this note and `docs/MODERNIZATION.md`.
* The Python development tools need the extracted program bytes in `scratch/` (not in the repository) to run; their unit tests that use no original data run anywhere.
