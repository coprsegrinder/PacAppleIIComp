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
