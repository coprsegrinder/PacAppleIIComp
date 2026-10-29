# Resume Prompt

How to use: open Claude Code in `F:\Ports\Pac-Man - Apple II`, then paste everything inside the box below as your first message.
**Keep this file current:** whenever a task finishes, update the "Current state" and "Next task" sections (ask Claude to do it).
Last updated: 2026-10-29 (after TASK-004).

````text
You are resuming a long-running project: a documented decompilation of the Apple II Pac-Man disk image in this folder
and a native Windows port. I am not a programmer. Work autonomously on safe, local, reversible steps in small,
verifiable tasks (10-20 minutes each), explain things in plain English, and give me exact instructions when I must act.

FIRST, before doing anything: read docs/TASK_LEDGER.md, docs/REVERSE_ENGINEERING.md, docs/PROJECT_PLAN.md,
docs/EXTERNAL_RESOURCES.md and `git log --oneline`. Then report in a few lines where we are and what the next task is,
and continue with it. Do not re-derive facts already recorded in those files.

## Standing rules (from my original brief)
1. NEVER modify, rename, overwrite or delete the original disk image `Pacman (1983)(Atari).dsk`.
   Re-check its SHA-256 (expect F9E19375B15852B91776776A95755DD3F57E2B20A517AA99ED8603413E0FE805) when in doubt.
   The emulator only ever gets a copy (`scratch/work.dsk`).
2. Ask my explicit permission BEFORE downloading or installing anything, cloning repos, or publishing/pushing anything
   (GitHub repo, release). Log every external resource in docs/EXTERNAL_RESOURCES.md. Browsing the web is allowed.
3. Truthfulness: never claim a test passed unless it was run; mark things Verified / Inferred / Unknown; never weaken a
   failing test to make it pass; never invent hashes or facts.
4. Final product: a native Windows 11 executable that does NOT run an emulator and does NOT read the disk image at
   runtime. The disk image is used only for a separate hash-verification step at launch. Never ship or commit the disk
   image, extracted original binaries or original assets (scratch/ and third_party/ are git-ignored on purpose).
5. Faithful original behavior is the default (the Apple II game itself, not the arcade or other ports); modern
   enhancements (scaling, high refresh, HDR evaluation, gamepad, etc.) are optional, separate and tested one by one.
6. Decompilation completeness is tracked separately from "it plays". Distinguish recovered / reconstructed / rewritten.
7. Keep the task ledger (docs/TASK_LEDGER.md) honest; commit tested checkpoints to git with descriptive messages.
   Documentation is written alongside the work, not at the end (README, PROJECT_PLAN, REVERSE_ENGINEERING,
   ARCHITECTURE, TESTING, BUILDING, USER_GUIDE, CHANGELOG, DECISIONS, KNOWN_ISSUES, LEGAL_AND_ATTRIBUTION).
8. Legal: my situation is a lawyer-consulted gray area, non-commercial. Don't nag about it; document concrete issues only.
9. Don't ask me unnecessary questions. Ask only for downloads, destructive actions, major design choices, or when the
   reference behavior can't be established. Give a recommendation with any question.
10. When screenshotting the emulator, capture ONLY its window (PrintWindow on the AppleWin window), never the whole
    desktop, because my screen contains personal content. Delete any accidental capture.

## Environment facts
- Windows 11, PowerShell 5.1 + Git Bash, Git, Python 3.14 (use `python -I`), Visual Studio 18 Build Tools (MSVC 14.51).
  No CMake/Ninja/gcc/pytest installed (tests use unittest: `python -I tests/test_*.py`).
- Reference emulator: AppleWin 1.32.1.0 at third_party/AppleWin/ (approved + downloaded + hash-verified; GPL-2.0;
  git-ignored). Launch: `AppleWin-x64.exe -d1 "<path to scratch\work.dsk>"`. First run needed me to accept its license
  dialog (already done on this machine).
- If scratch/ or third_party/ are missing (fresh clone), recreate: `python -I tools/extract_binary.py "<dsk>" PAC-MAN
  scratch/extract` (expect body SHA-256 EFF5B2635EDF0E3B6A227BEE0A477EB697AAEBC29EBC57089167DF5F10EE182C); for AppleWin ask me again.
- The original brief is long; its rules are condensed above. If you need the full text, ask me to paste it again.

## Current state (2026-10-29)
- Image: DOS 3.3 140K, files HELLO (Applesoft: BRUN PAC-MAN) and PAC-MAN (binary, load $1000, len $6A00).
- Release identity: I believe it's the 1983 US Atari release (unverified; from online docs, no hash source yet).
- Startup stub relocates the program up by $3000, so it runs at $4000-$A9FF, entry $4027 (arithmetic + sanity check;
  NOT yet confirmed with an emulator memory dump).
- Emulator boots the image to an input-selection screen (1 = keyboard, 2 = Apple joystick). Nothing beyond that observed yet.
- Tools done: tools/inspect_dsk.py, dsk.py, extract_binary.py, dis6502.py, region_map.py (+ unit tests). Done tasks:
  TASK-001..004 (see ledger). Region map is heuristic only.

## Next task
TASK-005: recursive-descent disassembly from entry $4027 (follow JSR/JMP/branches) to separate code from data, begin
naming routines (label inferred names as inferred), and confirm the $4000 origin with an AppleWin memory dump after
startup. Then TASK-006 (inspect boot tracks for protection), TASK-007 (web research, no downloads), TASK-008/009
(release identity and authoritative expected hash). Then emulator scripting for baseline observations.
````
