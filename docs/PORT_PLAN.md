# Port plan (TASK-024): proposal for your approval. No code has been written for the port yet.

Written 2026-10-29 after the decompilation groundwork (TASK-013 to TASK-020) and housekeeping (006-012). Architecture: ARCHITECTURE.md.

## 1. Where we stand (V)
| Area | State |
|---|---|
| Code map | 38.5% of the program is code (10,456 bytes, 5,165 instructions); every byte executed in 7 coverage runs lies inside it; 87.5% of it has been executed |
| Data | 55.9% read as data, 5.0% referenced only, 0.5% unclassified; tables and frame formats catalogued (TABLES.md, SPRITES.md) |
| Behavior known | controls, menus, hidden rank/cartoon menu, intermissions, pause, sound toggle, joystick/paddle reading, playfield map, draw routines |
| Behavior NOT yet understood | **ghost movement rules (48 jump-table routines)**, speeds per level, fruit rules, scoring/extra-life rules, level progression tables, sound data (jingle and effects), intermission scripts |
| Oracle | Python 6502 core + Apple II stubs, compared with AppleWin at two checkpoints |
| Toolchain (V, checked today) | Visual Studio Build Tools 2026 with `cl`, `nmake`, MSBuild and a **bundled CMake 4.3.1** (not on PATH); Windows SDK 10.0.26100 with Direct3D 11, XAudio2/WASAPI, XInput and DXGI 1.6 (HDR). **No download is needed for building** (this removes the "CMake download" item from PROJECT_PLAN.md) |

## 2. Decisions (D1 = A and D2 = A DECIDED by the user on 2026-10-29; D3-D5 defaults stand; constraint: the final result must be based only on the supplied .dsk)
### D1. Where does the game data come from? (major, affects rule 4 and the legal position)
| Option | What it means | For | Against |
|---|---|---|---|
| **A. Local data pack built from your verified disk (recommended)** | On first start the app asks for the disk image, verifies it, extracts the tables, shapes, font and strings into a pack stored on *your* machine, and then runs without touching the disk image again. Nothing original is committed or shipped. | Most faithful; least work; matches how many fan ports/engines handle original data | The pack is original data; rule 4 is satisfied only in the sense "game does not read the disk at run time"; the disk image is still read once to build the pack. Needs your confirmation that this reading of rule 4 is acceptable. |
| B. Re-created assets | Redraw sprites, font and maze from scratch; type the tables ourselves | Cleanest legal story | Not faithful (pixel shapes and timing tables would be re-authored), large effort, hard to prove equivalence |
| C. Build-time embedding | The exe is built locally from your disk and contains the data | Single file | Release builds could not be shared; same legal status as A, less flexible |
My recommendation: **A**, with the pack format designed so the user can delete it at any time, and with the launcher showing the verification result in plain English.

### D2. How is the code produced? (major, affects effort)
| Option | What it means | For | Against |
|---|---|---|---|
| **A. Oracle-driven hand reconstruction (recommended)** | We write readable C++ routine by routine, checking each against the Python oracle with recorded vectors and scenario traces (see ARCHITECTURE.md). Start with leaf routines (blitters, tables, input) and work up. | Readable, maintainable, no machine-translated original code; matches your brief's "recovered / reconstructed / rewritten" tracking | Most work (129 routines + 48 jump-table routines); ghost AI must be understood first |
| B. Mechanical translation first | Auto-convert the 6502 code into C++ that mimics the original instruction by instruction, then gradually replace it | Playable much sooner | The result is machine-translated original code running on an emulated memory/CPU state: close to "an emulator" and to redistributing the original code in another form; unreadable; conflicts with rules 4 and 6 |
My recommendation: **A**. B would only be used privately as a measuring aid if we ever wanted it, never shipped.

### D3 to D5: I will take these defaults unless you object
* **Graphics:** Direct3D 11, integer-scaled hi-res image first (faithful NTSC-style colors per the palette-bit rules); optional smoothing/scaling/high refresh later as separate, tested options.
* **Audio:** XAudio2 playing samples generated from the cycle-timed click events; Ctrl-S mute behavior kept.
* **Timing:** faithful mode = virtual cycle clock paced to 1.0227 MHz (reproduces the slowdowns measured in ARCHITECTURE.md); optional "steady speed" mode later.
* **Build:** CMake (bundled with the Build Tools) with the Visual Studio generator, plus a plain `build.bat` using `cl`; tests are small console programs run by a script (no extra frameworks).

## 3. Milestones (each ends at a checkpoint with tests, ledger update and a commit; one per "go")
| M | Content | Acceptance test |
|---|---|---|
| M0 | Repository skeleton: CMake/`build.bat`, test runner, README, BUILDING, ARCHITECTURE, USER_GUIDE, CHANGELOG, KNOWN_ISSUES, **LEGAL_AND_ATTRIBUTION** (credits Atari 1983 as named in the program, notes the reported Taxman lineage as unverified); C++ port of the verifier | Builds from a clean checkout on this machine; verifier tests give the same results as tools/verify_image.py on the 4 real-image checks and the synthetic cases |
| M1 | Data pack builder + loader (shapes, font, strings, playfield master, tables); pack never committed | Pack built from the verified image; loader rejects a pack built from another hash; byte counts and hashes of each table match the oracle's memory |
| M2 | `hires` layer: row-address math, the two blitters, font drawing, pixel decoding | After scripted draw sequences, the hi-res page equals the oracle's page byte for byte (several recorded scenarios) |
| M3 | `video` + `app`: window, scaled display, color model, menu and attract screens | Rendered frames compared with oracle hi-res pages (exact) and with AppleWin screenshots (visual) |
| M4 | Game core without ghosts: playfield map, Pac-Man movement, dots/energizers/score/lives, keyboard + gamepad input, pause, hidden menu, customised keys | Scenario traces match the oracle (score, map, Pac-Man position) for scripted key sequences |
| M5 | **Ghost logic**: first an analysis task (understand the 48 routines, tables and speeds), then reconstruction | Ghost positions match the oracle step by step in recorded scenarios |
| M6 | Levels, fruit, intermissions 1-3, level-rank start | Level sequences and cartoons match the oracle; the oracle's unexecuted 1,309 code bytes are covered or proven dead (TASK-021) |
| M7 | Sound: jingle, effects, click scheduler, audio output | Click lists equal the oracle's (cycle-exact) for recorded scenarios; listening test by you |
| M8 | Faithful pacing + modern options (scaling, high refresh, HDR evaluation, gamepad polish) as separate switches; clean-machine run; packaging; release notes. Publishing only with your approval. | Each option has its own test and off-by-default status; clean-machine test |

Analysis tasks to schedule before the milestones that need them: TASK-021 (last 1,309 unexecuted code bytes), a ghost-AI analysis (before M5), sound-data analysis (before M7), scoring/level-table analysis (before M4/M6), and TASK-022 (synchronised gameplay and audio comparison with AppleWin) as the final accuracy check. TASK-023 (Taxman source) stays logged only; it needs your separate approval to download anything.

## 4. Test strategy (summary)
Unit tests for every module; differential tests against the oracle (routine vectors and scenario traces, local-only when they contain original data); screenshot/dump spot checks with AppleWin; an "isolation test" that the finished exe runs without the disk image present after the pack exists and without any emulator code (rule 4), plus a check that the repository contains no original data.

## 5. Risks (ranked)
1. **Ghost AI complexity** (48 routines through a jump table, per-level behavior): may take many analysis tasks. Mitigation: study with the oracle first, M5 gated on understanding.
2. **Faithful timing** (cycle costs of ~180 routines): costs can be measured per call from the oracle; risk is subtle slowdowns not matching. Mitigation: scenario tests compare cycle counts per pass.
3. **Legal/attribution** (Atari copyright; possible Taxman lineage; data pack). Mitigation: nothing original committed or shipped, pack built locally, attribution file written in M0; your lawyer's advice applies to D1.
4. **Colors**: Apple II NTSC artifacts depend on the monitor; the port will use a documented approximation; compare with AppleWin screenshots.
5. **Audio**: only cycle-timed 1-bit clicks; converting to samples needs a careful filter to avoid aliasing; faithful vs pleasant is a user option.
6. **Single reference image** (no public hash): a different retail revision might behave differently.

## 6. Effort feel (rough, not a promise)
M0-M3 are small and mostly mechanical (about a dozen short tasks); M4 moderate; **M5 is the big unknown**; M6-M8 moderate. I would not estimate calendar time until M5's analysis is done.

## 7. Proposed next step after your decisions
TASK-025: write M0 (skeleton, docs, C++ verifier). No analysis is blocked by your decisions: TASK-021 and the ghost analysis can proceed in parallel if you prefer to start with those.
