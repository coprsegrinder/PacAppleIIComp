# Documentation index

The notes were written while the project was made, in the order of the work; the later notes build on the earlier ones. Task numbers (TASK-nnn) refer to [`TASK_LEDGER.md`](TASK_LEDGER.md). Statuses and claims are marked Verified / Inferred / Unknown.

## Start here
| Note | What it is |
|---|---|
| [`../README.md`](../README.md) | what the project is, what it does, how to build and use it |
| [`OVERVIEW.md`](OVERVIEW.md) | the whole story: how it was made, how close it is to 1:1 (the numbers), the modernization features |
| [`RELEASE_NOTES.md`](RELEASE_NOTES.md) | what the release is, what it needs, its limits |
| [`REGENERATING.md`](REGENERATING.md) | how to rebuild everything (program, tests, recordings) from the disk image alone |
| [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) | what is open, unverified or limited |
| [`MODERNIZATION.md`](MODERNIZATION.md) | every option added beyond the original (display, backgrounds, colour, CRT filter, controls, achievements) with how each was checked |
| [`CONTROLS.md`](CONTROLS.md) | the original's input routine, and the keys and controllers of the port |
| [`USER_GUIDE.md`](USER_GUIDE.md), [`BUILDING.md`](BUILDING.md), [`TESTING.md`](TESTING.md) | using, building and testing |

## Plans, decisions, history
| Note | What it is |
|---|---|
| [`PROJECT_PLAN.md`](PROJECT_PLAN.md), [`PORT_PLAN.md`](PORT_PLAN.md), [`ARCHITECTURE.md`](ARCHITECTURE.md) | the first plans and the design of the port |
| [`M4_PLAN.md`](M4_PLAN.md) game core, [`M5_PLAN.md`](M5_PLAN.md) ghosts, [`M6_PLAN.md`](M6_PLAN.md) levels / fruit / intermissions, [`M7_PLAN.md`](M7_PLAN.md) sound, [`M8_PLAN.md`](M8_PLAN.md) the complete game | the milestones |
| [`DECISIONS.md`](DECISIONS.md) | the decisions taken, with the reasons |
| [`TASK_LEDGER.md`](TASK_LEDGER.md), [`CHANGELOG.md`](CHANGELOG.md), [`RESUME_PROMPT.md`](RESUME_PROMPT.md) | every task and its result; the log of changes; notes to resume the work (these were kept during development and are not rewritten: the newest facts are in README, KNOWN_ISSUES and MODERNIZATION) |

## The original program
| Note | What it is |
|---|---|
| [`REVERSE_ENGINEERING.md`](REVERSE_ENGINEERING.md) | findings on how the program works |
| [`CODE_COVERAGE.md`](CODE_COVERAGE.md), [`DYNAMIC_COVERAGE.md`](DYNAMIC_COVERAGE.md), [`UNEXECUTED_CODE.md`](UNEXECUTED_CODE.md), [`REGION_MAP.md`](REGION_MAP.md) (superseded), [`ROUTINE_AUDIT.md`](ROUTINE_AUDIT.md) | which bytes are code, which ran, which routine does what |
| [`DATA_REGIONS.md`](DATA_REGIONS.md), [`TABLES.md`](TABLES.md), [`LEVEL_TABLES.md`](LEVEL_TABLES.md) | the data and the tables |
| [`FORCED_CODE.md`](FORCED_CODE.md) | the "forced" bytes of the code that the port reproduces on purpose |
| [`BOOT_TRACKS.md`](BOOT_TRACKS.md), [`RELEASE_IDENTITY.md`](RELEASE_IDENTITY.md), [`RESEARCH_007.md`](RESEARCH_007.md), [`VERIFIER_SPEC.md`](VERIFIER_SPEC.md) | the disk, which release it is, how a disk image is accepted |

## The game's rules and parts
| Note | What it is |
|---|---|
| [`PACMAN_RULES.md`](PACMAN_RULES.md) | dots, energizers, scoring, collisions, lives, speed, turning |
| [`GHOST_ANALYSIS.md`](GHOST_ANALYSIS.md), [`GHOST_RULES.md`](GHOST_RULES.md), [`GHOST_MODES.md`](GHOST_MODES.md), [`MOVEMENT.md`](MOVEMENT.md) | the ghosts: structure, decision rules, modes and timers, movement and the house |
| [`CARTOONS.md`](CARTOONS.md), [`SOUND.md`](SOUND.md), [`SPRITES.md`](SPRITES.md), [`GRAPHICS.md`](GRAPHICS.md), [`ATTRACT_FLOW.md`](ATTRACT_FLOW.md) | the intermissions, the sound, the shapes, the drawing and colour, the menus and the attract loop |

## Verification of the port
| Note | What it is |
|---|---|
| [`DATAPACK_SPEC.md`](DATAPACK_SPEC.md) | the data pack built from the player's disk image |
| [`GOLDEN_TRACES.md`](GOLDEN_TRACES.md), [`EMULATOR_COMPARISON.md`](EMULATOR_COMPARISON.md) | the recorded runs the port is compared with; the comparison with an Apple II emulator |
| [`APP_WINDOW.md`](APP_WINDOW.md), [`WINDOW_DESIGN.md`](WINDOW_DESIGN.md) | the window, the pacing and the input design |

## Research for the modern options
[`ASSET_RESEARCH.md`](ASSET_RESEARCH.md) (backgrounds), [`CRT_RESEARCH.md`](CRT_RESEARCH.md) (CRT filters), [`MONOCHROME_RESEARCH.md`](MONOCHROME_RESEARCH.md) (how a monochrome display showed the picture).

## Legal
[`LEGAL_AND_ATTRIBUTION.md`](LEGAL_AND_ATTRIBUTION.md), [`EXTERNAL_RESOURCES.md`](EXTERNAL_RESOURCES.md), and [`../NOTICES.md`](../NOTICES.md).

* [CODE_COVERAGE_PORT.md](CODE_COVERAGE_PORT.md): which lines of the port's own C++ the whole test suite runs (tools/coverage_cpp.sh, tools/linecov), with the lines that never ran.
