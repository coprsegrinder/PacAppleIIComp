# Golden gameplay traces (TASK-034a)

Tool: `tools/golden_trace.py`. Data: `tests/golden/gameplay_traces.json` (hashes, small samples, inputs and pokes only; no pictures, no original data). Test: tests/test_golden_gameplay.py. V = verified by running.
Purpose: the C++ game core (M4-M6) is tested against these: same inputs per tick must give the same logical state.

## Definitions
* **Tick** = one pass of the game's main loop, counted at each execution of `$7861` (Pac-Man's step timer) with the demo flag `$E8 = 0` (V: about 7,100 cycles per tick on average, 5,330 ticks in 100 blocks of 150,000 instructions). The port needs no cycle timing; inputs are keyed by tick.
* **Logical state** at tick t (recorded at the start of tick t, before its keys and pokes): zero page `$52-$6F` (score BCD, level, dot counters, lives, fruit timers, Pac-Man position, direction and junctions, ghost score step), `$78 $79 $E8 $E9`, the four ghost records `$84-$D3`, and the maze dot map `$623A-$65B9`. Everything else (code, scratch variables, screen) is not part of the comparison. If a port-side check shows that one of these bytes is scratch that the port legitimately computes differently, it is dropped from the list and the traces are regenerated (documented change).
* **Chain hash** h(t) = SHA-256(h(t-1) + state(t)); stored every 500 ticks (`checkpoints`) and at the end (`final`). **Samples** every 50 ticks keep 20 readable fields (score, level, dots, lives, Pac-Man x/y/direction, mode flags, ghost x/y) so a divergence can be located. `events` count the oracle's marks (`$7AF4` dot, `$7AC7` energizer, `$8B9D` collision resolved, `$8BDD` Pac-Man caught, `$6CF4` level complete).
* **Inputs**: `[tick, key]` pairs (about every tenth tick one random key of the game's key set, seeded); pushed before tick t executes. **Pokes**: `[tick, address, value]`, applied right after the keys of that tick; they are state changes the port applies through its state interface (the port must support them in test mode): level-end (`$56` = 3 dots to go), energizers (every 7th dot of the map becomes value 131) and "Pac-Man placed on frightened ghost 1" (`$60/$61` = `$84/$85`).

## Scenarios (V; all four replay identically on the oracle)
| | Ticks | Events | Notes |
|---|---|---|---|
| A | 8,090 (game over) | 44 dots, 1 energizer, 3 caught | plain random play |
| B | 4,610 (game over) | 51 dots, 1 level complete, 3 caught | level end poked at tick 299 |
| C | 6,195 (game over) | 68 dots, 2 level completes, 3 caught | levels 1 and 2 poked to end; passes the level-2 cartoon |
| D | 14,937 (game over) | 55 dots, 9 energizers, 3 ghosts eaten + 3 catches | 47 pokes: energizers at tick 100, Pac-Man put on a frightened ghost three times |

## Checks run
* Each scenario was replayed from the stored inputs and pokes (each in its own run, 123-148 s) and reproduced every checkpoint, sample, event count, the final hash and the tick count (V). The first replay attempt failed for scenario D because my replay code kept only one poke per tick; I fixed that bug (it was in the replay code, not in the traces) and re-ran all four.
* tests/test_golden_gameplay.py has two tests: the stored file's coverage of events (runs in a moment, passes) and the full replay of all four scenarios (about 9 minutes in total; **not run as one test this turn**, only as four separate replays through the same function).

## Not covered (U)
Fruit appearing and being eaten (needs 64 items and a path through the fruit; none of the scenarios eats fruit), the bonus life at 10,000 points, levels above 3, two players, speaker clicks (the sound model has its own tests), intermission pictures (the cartoon decode is TASK-031c), and keyboard variants (joystick modes).
