# M8 plan: a complete, faithful game, then options and packaging (proposed 2026-11-03, waiting for the user's OK)

Status of the live game (pacman_play) today: the maze, ghosts, dots, fruit, levels, intermissions and sound work. Missing for a complete game: the READY pause at the start of each life, the death animation (the game just continues), GAME OVER and a new game, the title/menu screens, two players. Timing has not been checked against a stopwatch (the user asked about cartoon speed on 2026-11-03).

Each step is one ledger task, one turn, one checkpoint.

| Task | What | Test / done when |
|---|---|---|
| 033-1 | Pacing audit: measure the live tick rate and the cartoon length with a stopwatch-free test (clock model) and fix anything off; check the first-start jingle freeze | test against oracle cycle counts; the user times a cartoon |
| 033-2 | READY pause at the start of every life | frames and cycle count equal the oracle's |
| 033-3 | Death animation (drawing plus its six sounds in place) | frames and sound positions equal the oracle's |
| 033-4 | GAME OVER and starting a new game (jingle plays again, score reset, high score kept) | state after GAME OVER equals the oracle's |
| 033-5 | Title / mode screen / players prompt (029i), one player first | screens equal the oracle's |
| 033-6 | Two players (core already has the save/swap) | oracle comparison |
| 033-7 | Modern options, one per turn, each off by default with its own test: scaling filter, high refresh, gamepad, HDR evaluation | per option |
| 033-8 | Packaging: release folder (exe + README, no disk image, no original data inside; the player builds the pack from their own disk image once) | build from a clean checkout |
| 033-9 | Clean-machine test (needs a second PC or Windows Sandbox; the user's choice) | game starts, sound works |
| 033-10 | Release notes; publishing only with the user's explicit approval | n/a |

Suggested order: 033-1, 033-2, 033-3, 033-4 first (they make the game playable start to finish), the rest after.

## 033-1 result (2026-11-03): pacing audit, findings only (no code changed)
Measured from `scratch/tick_cycles_A.bin` (cycle cost of each of 3,000 ticks of scenario A on the oracle):
* Ordinary ticks (2,990) cost **4,843 cycles on average, but between 2,987 and 19,518** (standard deviation 2,779). About half cost 2,500-3,000, the rest 6,000, 9,500, 13,000 and more depending on what the tick draws. The port charges the same flat 4,870 cycles for every tick (`kTypicalTickCycles`). **Over a long stretch of play of the scenario-A kind the speed is right; in busier scenes (more moving ghosts, other levels) the original is slower than the port, and the original's short slowdowns/stutters are missing.** (V for the numbers, I for the effect on the feel; scenario A only, level 1.)
* Long pauses: the death sequence costs 6,043,263 cycles (5.91 s, V). The port uses that same figure for the **level end** and for **GAME OVER** as placeholders; neither was measured (the level-end note in gamedriver.cpp says so). A level end without a cartoon probably pauses a different length. (U)
* Cartoons: lengths are exact (5,597,305 / 4,347,216 / 8,211,963 cycles, V on the oracle); the live player uses them directly.
* Not checked: the first-start jingle freeze (5,153,446 cycles, V from the sound task), the real PC clock accuracy (`CycleClock` is exact integer arithmetic and tested with a fake clock).

New sub-tasks (replace the single 033-1):
| Task | What | Done when |
|---|---|---|
| 033-1b | Measure the real length of the level end and of GAME OVER on the oracle and use them instead of the death-sequence figure | live pause = oracle cycles |
| 033-1c | Per-tick cost model: record the cycles of every tick for more scenarios (B, C, D, levels 2+, frightened), fit a cost from what the tick does, test the model against the recordings | model within a stated error of the oracle per tick and exact in total over each recording |
