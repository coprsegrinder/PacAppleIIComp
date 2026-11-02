# Resume Prompt

How to use: open Claude Code in `F:\Ports\Pac-Man - Apple II`, then paste everything inside the box below as your first message.
**Keep this file current:** whenever a task finishes, update the "Current state" and "Next task" sections (ask Claude to do it).
Last updated: 2026-10-29 (after TASK-013a; added checkpoint-stop rule and objective limits).

````text
You are resuming a long-running project: a documented decompilation of the Apple II Pac-Man disk image in this folder
and a native Windows port. I am not a programmer. Work autonomously on safe, local, reversible steps in small,
verifiable tasks (10-20 minutes each), explain things in plain English, and give me exact instructions when I must act.

FIRST, before doing anything: read docs/TASK_LEDGER.md, docs/REVERSE_ENGINEERING.md, docs/PROJECT_PLAN.md,
docs/EXTERNAL_RESOURCES.md and `git log --oneline`. Then report in a few lines where we are and what the next task is,
and wait for my "go" before starting it (rule 11). Do not re-derive facts already recorded in those files.

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
9. Don't ask me unnecessary questions (but see rule 11: stopping at checkpoints is required, not optional). Ask only for downloads, destructive actions, major design choices, or when the
   reference behavior can't be established. Give a recommendation with any question.
10. When screenshotting the emulator, capture ONLY its window (PrintWindow on the AppleWin window), never the whole
    desktop, because my screen contains personal content. Delete any accidental capture.
11. STOP AT EVERY CHECKPOINT. Do exactly ONE ledger task (or one clearly bounded sub-step) per turn. When it is done:
    run its tests, update the ledger, notes and this resume file, commit, write a short plain-English report
    (what changed, tests run and results, known-good state or not, next recommended task), then STOP and wait for me to
    say "go". Never chain into the next task on your own, even if it looks easy.
    TIME RULE (corrected 2026-11-01 after a 1 h 4 min turn): the 20-minute limit is for the WHOLE turn, measured with `python -I tools/turn_clock.py start` at the beginning and `check` before every long command; plan the turn first with `turn_clock.py plan <minutes...>` (sum must be <= 15), and split anything longer into a new ledger row.
    Break heavy work (large disassemblies, big reconstructions) into several checkpointed pieces; prefer many small
    verified steps over one big one. Objective limits (replace judgment with these):
    - Commands: every command gets an explicit timeout (default 60 s, max 5 min). Every emulator launch gets a kill step;
      never leave an emulator or background process running at a checkpoint unless I need to look at it.
    - Retries: the same failure twice => stop, save partial progress to the ledger, report the blocker, ask me.
      Never try a third variation of the same fix.
    - Output: disassemble/inspect in chunks of at most ~100 lines per step and write bulk output to files
      (scratch/ or docs/) instead of printing it.
    - GUI/emulator: at most 3 capture or keystroke attempts per goal; then ask me to look at the window.
    - Research: at most 5 searches or pages per question, then summarize what was found.
    - Scope: one routine group or one test scenario per task; if a task needs more, split it in the ledger first.

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
- (Updated after TASK-013b) Headless Apple II machine exists: tools/apple2.py; game runs to the mode screen and accepts '1'.
- Image: DOS 3.3 140K, files HELLO (Applesoft: BRUN PAC-MAN) and PAC-MAN (binary, load $1000, len $6A00).
- Release identity: I believe it's the 1983 US Atari release (unverified; from online docs, no hash source yet).
- Startup stub relocates the program up by $3000, so it runs at $4000-$A9FF, entry $4027 (confirmed by AppleWin memory dump, TASK-005b).
- Emulator boots the image to an input-selection screen (1 = keyboard, 2 = Apple joystick). Nothing beyond that observed yet.
- Tools done: tools/inspect_dsk.py, dsk.py, extract_binary.py, dis6502.py, region_map.py (+ unit tests). Done tasks:
  TASK-001..004, 005a-c, 013a (see ledger); tools/cpu6502.py core verified incl. real startup stub; runtime origin $4000 CONFIRMED by emulator dump; recdis.py reaches 30% of the program from the entry point. Region map is heuristic only.

## Next task
Analysis done so far: 030a structure, 030c modes, 030d movement/house/eyes (docs/GHOST_ANALYSIS.md, GHOST_MODES.md, MOVEMENT.md). Eyes table $A360 verified complete; release tables $54A4/$54B4/$54C4; Pac-Man uses the same junction graph; attract demo = replayed move stream (I).
TASK-031a done (docs/PACMAN_RULES.md: scoring, lives, bonus life, speed rule verified; collision distances, turning, dot penalty, fruit counter only inferred). Tip: stop traces when $E8=1 (attract demo pollutes them).
TASK-030b done (docs/GHOST_RULES.md: complete tables; 16 routines depend on Pac-Man's direction $62).
TASK-032a sound analysis done (docs/SOUND.md: engines A/B/C, 12 lists, click-time model exact).
Level/fruit tables done (docs/LEVEL_TABLES.md; $7C61 = fruit sound; level 23 table entry = odd HAL-like sprite, U).
TASK-021 done (99.3% of static code executed, docs/UNEXECUTED_CODE.md); TASK-021d done: 100% of the static code map executed (38 bytes only under forced state, section 7). NOTE: 0x7BE6 sound is GHOST EATEN (not death); death = table sounds at $7C82.
TASK-027a-d done (docs/GRAPHICS.md: writers, addressing, golden hashes, wall and text models verified, forced $1D=0 mode specified); TASK-027e done (colour model verified vs AppleWin: 100% of isolated pixels after re-check); graphics spec complete; TASK-034a golden gameplay traces done (docs/GOLDEN_TRACES.md); TASK-031c done (docs/CARTOONS.md: schedule, cartoon 1 model, golden cartoon data); TASK-031d/e done (all three cartoons modelled, docs/CARTOONS.md), then the C++ hires layer (TASK-027 proper). User also wants the 34 forced code bytes ported (docs/FORCED_CODE.md). Remaining analysis: intermission/cartoon scripts (levels 2, 5, 9...; the scheduler at $8155 ran for levels 10-30 but the cartoons are not decoded).
TASK-028b done (2026-11-02; GDI window port/src/app, docs/APP_WINDOW.md, tests/test_window_shot.py). TASK-028c done (real maze in the window, pixel-exact vs the Python model). TASK-028d done (drawDots, 3 oracle runs exact; window shows maze + dots). TASK-028e done (attract text in the window, pixel-exact vs the Python text model). TASK-028f done (cartoon 1 call replay: final page hash equal; local script scratch/cartoon1_script.bin, test flag --script). TASK-028g1 done (window shows a replayed screen; replay library port/src/hires/replay.*). TASK-028h done (cartoons 1-3 replay exactly; --calls N). TASK-028i done (CMake route verified: cmake at C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin). TASK-028j done: design decided by the user (docs/WINDOW_DESIGN.md). TASK-028k done (GameWindow class + 6 tests). TASK-028l done (CycleClock + 9 tests). M3 pieces (window, colour, clock) exist; TASK-029 split into 029a-029i (docs/M4_PLAN.md). TASK-029a done (scratch/ticks_A..D.bin, test flag --ticks). TASK-029b done (GameState + newGameState; start-of-life and ghost bytes are oracle literals for now). TASK-029c done (InputPoller; poll runs every tick in keyboard mode). TASK-029d1 done (StepTimer; 029d split into d1-d4). TASK-029d1b done (levels 4-21 covered by poke recordings scratch/ticks_L*.bin; carry quirk at level >= 15; the near-ghost +1 rule at $8B34 still unseen on the oracle). TASK-029d2 done (junctionStep + JunctionGraph; 9 earlier $63 writes were restarts). TASK-029d2b done (stepPacman; tunnel + demo recordings scratch/ticks_T00.bin, ticks_DEMO.bin + demo_stream.bin; test flag --demo). TASK-029d2c + 029d4 done (DemoStream, natural tunnel recording N00 without wraps, eatAt). TASK-029d2d done (N00 natural tunnel with 4 wraps; T00 double replay). TASK-029f1 done (contact, eaten, caught, extra life; bonus life lives in $7091). TASK-029f2 done (startOfLife; new game from pack tables). TASK-029g1 done (first screen except score/text/icons; test flag --page0 scratch/page_tick0.bin from tools/record_page0.py). TASK-029g2/g3 done (the whole first page is exact). TASK-029g4 part 1 done (sprite calls exact for 700 ticks; local files scratch/tick_calls_A.txt and scratch/pages_A.bin, flags --tickcalls --pages). TASK-029g5 done (700 ticks of scenario A exact: calls and pages). TASK-029g6 partly done (blinks, door palette; A exact incl. blinks; D to tick 465; side files scratch/extra_X.bin, tick_calls_X.txt, pages_X.bin by tools/record_tick_calls.py <ticks> <letter>). TASK-029g7 mostly done (scenarios A-D, F exact in calls and pages; side files per letter). TASK-029g8 mostly done (flash frame, fruit, rebuilt screens after death and level end all exact). TASK-029g9 mostly done (fruit, death and level-end sequences, icon to level 7; scenarios A-D, F-H, J exact). Suggested next: 029g10 (door low-bit, two-player, levels above 7) or 029h (integration); open items listed under 029d4 (energizer ghost effects M5, fruit spot needs $E7 side data M6, score drawing 029g); watch for the one-off ctest unit failure noted in M4_PLAN.md; local test files scratch/cartoon{1,2,3}_script.bin, cartoon1_final_page.bin, cartoon1_page_at_29200.bin, cartoon1_snapshot.txt; local test files: scratch/cartoon1_script.bin, cartoon1_final_page.bin; tests: pacman_tests --image <dsk> --vectors scratch/hires_vectors.bin --dots scratch/dots_vectors.bin; colour function done in port/src/video; hires parts 1 and 2 are in port/src/hires (vectors are local: --vectors scratch/hires_vectors.bin); the pre-C++ analysis (graphics spec, golden traces, cartoons, forced bytes) is done. Rule from the user (2026-11-01): any run over 20 minutes is stopped, progress logged, remainder split into a new step. After that the analysis is ready for M2+ C++ (hires layer first).

Emulator control (verified): launch `AppleWin-x64.exe -d1 <scratch\work.dsk>`; drive it with
`powershell -File tools/awin_ctl.ps1 -Key F7` (debugger), `-Text 'BSAVE "main.bin",0,C000'` (dump main RAM to scratch/),
`-Shot <png>` (window-only capture). It posts messages straight to the window; the window may be minimized (script restores it).

Latest: 029g10 partly done; next: the player-switch sequence (P tick 1976), then remaining open drawing items.

Latest: 029g11 done (two-player switch). Next: remaining open drawing items; then a full-suite checkpoint.

Latest: door low-bit OR closed. Next open: player 2 death/level end + GAME OVER recording; pauses/sounds; high-score carry; then full-suite checkpoint.

Latest: 029g13 partly done (Q equal). Next: make R usable (1 state short), GAME OVER screens.

Latest: 029g13 done (GAME OVER of the last life). Next: player 2 level end, high-score carry, then full-suite checkpoint.

Latest: 029g14 done. Next: high-score carry, player 2 level end, then the full-suite checkpoint.

Latest: apple2tc added and built (029h0). The user wants 100% of the game understood before more C++ conversion: next 029h1 (game image .b33), 029h2 (run data), 029h3 (coverage report).

Latest: 029h1 done (make_b33.py, a2run runs the game). Next 029h2: run data from all scenarios (key files).

Latest: 029h2 partly done (A-D). Next: 029h2b other scenarios, then 029h3 apple2tc listing + coverage.

Latest: 029h2/2b done. Next 029h3: apple2tc on scratch/a2tc_PAC-MAN.b33 with scratch/a2tc_run_everything.json (asm listing + coverage report).

Latest: 029h3 done (docs/APPLE2TC_COVERAGE.md). Next 029h4: compile/run the generated C vs the oracle; 029h5: declare data, close holes ($951F-$956A never executed).

Latest: apple2tc method SCRAPPED. Continue M4: high-score carry, player-2 level end, full-suite checkpoint, then 029h integration.

Latest: FULL-SUITE CHECKPOINT done (C++ 102/0, Python 172 OK) with `bash tools/full_suite.sh python` and --drawticks. Next: high-score carry, player-2 level end, then 029h integration.

Latest: 029g15 done (high score carry). Next: player-2 level end, then 029h integration.

Latest: 029g16 done (player 2 level end). Drawing of gameplay is complete except pauses/sounds and post-GAME OVER. Next: 029h integration.

Latest: 029h DONE. pacman_play.exe runs (no ghosts). Next: M5 ghosts (030e) - replace the fed ghost fields; or 029i/other ledger items.

Latest: M5 planned; 030e1 done (A-D). Next: 030e2 GhostWorld state model, 030e3 countdown/frightened timer. User tried pacman_play.exe: movement feels great, fruit and power pills score; ghosts do not move (expected).

Latest: 030e3 done (ghosttick.cpp head). Next 030e4: junction detection, decision $8C50, move queues.

Latest: 030e4 done (D6: decision routines transliterated mechanically). Next 030e5: special junctions + house ($81) + $7A; then move 030e6.

Latest: 030e5/e6 done incl. house move. Next 030e7: mode timer $808C + reversal $80C1 + energizer ghost effects + eaten effects; then 030e9 integration (queue init at life start).

Latest: 030e7 done. Next 030e9: GhostSource inside GameCore (all ghost logic), no fed fields; free-running A-D must equal the oracle incl. eaten ghosts, catches, house.

Latest: M5 DONE (ghosts). Next: M6 (level change, level tables, intermission cartoons), M7 sound, M8 pacing/packaging; small open items: READY pause, death animation, title/attract, two-player core.
