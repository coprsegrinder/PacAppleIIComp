# The last unexecuted code (TASK-021)

Tools: `tools/cov_close.py`, `tools/cov_close2.py`, `tools/cov_close3.py` (targeted scenarios with coverage on, writing scratch/cov_close*.json), `tools/cov_union.py` (merge and compare with the static code map). Tests: tests/test_unexecuted.py.
V = verified by running, I = inferred from reading the code, U = unknown. Addresses are the relocated ones ($4000-$A9FF).

## 1. Result
* Before this task: 9,147 of 10,456 static-code bytes had been executed (87.5%), 1,309 bytes in 125 runs were not.
* **Now: 10,386 of 10,456 (99.3%) executed; 70 bytes in 16 runs are left; 0 executed bytes outside the static code map (V, union of 10 coverage files).** (The static code map is the 38.5% of the program found by recursive disassembly plus dispatch tables; the other 61.5% is data, DATA_REGIONS.md.)
* What closed the 1,239 bytes (all on the oracle): the 48 ghost routines over their complete class tables (GHOST_RULES.md, 853 of the 1,309 bytes), every sound list and engine, fruit set-up/pick-up/score for levels 1-30 and in play, ghost eating and the bonus life, deaths, a forced level completion and the level clamps for levels 22-40, the cartoon scheduler for levels 10-30, Ctrl-S, Space at the "PRESS RETURN" prompt, a 2-player game, joystick buttons, and the forced mode timers.
* Not one executed byte contradicted the static map (the map is sound as far as it was tested).

## 2. New facts found while closing it
* **The keyboard mode also reads a 4-switch digital joystick (V).** Routine `$6FD0` (used when `$F5 = 0`, i.e. mode 1) reads the game-port button input `$C061` four times, each time after switching the annunciators (`$C058-$C05D`), and assembles a 4-bit mask (bits 4, 8, 2, 1 for the four reads). Fed with all 16 masks on the oracle: **exactly the four single-bit masks (1, 2, 4, 8) give a direction, every other mask (0, 3, 5-7, 9-15) gives "no input"** (V). The direction-code table is at `$700E` (16 entries). A real Apple II with the keyboard mode never needs it, but a port with a gamepad can use the same four-way logic; this is the only joystick-like path of mode 1.
* **Mode 2 (Apple joystick) uses the paddle reader at `$7041`** instead (`$F5 = 1`; V: with the buttons held, mode 2 reached `$7041` and never `$6FD0`).
* Ctrl-S toggling the speaker (`$6F83`) now executed: the speaker operands at `$7BB6/$7D07` flip between `$30` and `$20` (V).
* Space at the "PRESS <RETURN> TO PLAY OR <SPACE> TO GO BACK TO DEMO" prompt goes back to the demo (`$E8 = 1`, V).
* The chase-to-corner mode switch (`$80A1`, `$E9` 0 -> 1, countdown reloaded with 140) executed once in a forced test (V).

## 3. The 70 bytes left (16 runs), classified
| Run(s) | Bytes | What it is | Reachable? |
|---|---|---|---|
| `$806F-$807C` | 14 | during the death animation (`$805D`), a ghost that is eyes gets two extra steps | I: reachable only if a ghost is eyes when Pac-Man is caught. **Two tries to force it failed (the death code seems to clear the eyes flags first), so I stopped.** |
| `$89AC-$89B0` | 5 | ghost-house exit script: extra queue entry when the ghost's direction is "up" in the non-corner phase | I: depends on exit direction and phase; not seen |
| `$5754, $5770, $57AA, $57CF, $57EB, $5825` | 2 each (12) | `INC $17`: carry into the high byte when a computed screen address crosses a page | I: data-dependent, never happened in any run; probably never |
| `$5D31-$5D3C`, `$5D66-$5D6D` | 12 + 8 | alternative loops of a draw routine when its flag `$1D` is 0 | I: only the `$1D <> 0` variant occurs in our scenarios |
| `$41D2-$41DB` | 10 | after the ROM line input (`JSR $FD6F`) when the typed line has length 1 | U: depends on our ROM stub for line input, which does not behave like the real ROM here; the port replaces text entry anyway |
| `$52F9-$52FB` | 3 | Space pressed during a delay loop at `$52E9` jumps to the key poll | I: reachable by pressing Space there; low value |
| `$53B2-$53B4` | 3 | after an unconditional branch; the bytes look like table data | I: static map artefact (data), not code |
| `$56E9`, `$6DEE`, `$8A3A` | 1 each | a lone `RTS` / `RTS` / a `$00` byte between routines | I: fall-through returns or padding, never reached |
None of these changes any rule found so far; they are rare corners or artefacts. They stay open as TASK-021b (low priority), not blocking the C++ port.

## 4. How to repeat
`python -I tools/cov_close.py`, `cov_close2.py`, `cov_close3.py` (each 5-10 minutes), then `python -I tools/cov_union.py scratch/cov.json scratch/cov_auto1.json scratch/cov_auto2.json scratch/cov_cartoons.json scratch/cov_custom.json scratch/cov_play.json scratch/cov_rank.json scratch/cov_close.json scratch/cov_close2.json scratch/cov_close3.json`. (cov_close2's scenario 1, which tried Space at the prompt, never started its game because of a wrong key order; cov_close3 does it correctly. Both stay in the repository as run.)

## 5. TASK-021b (second look at the last 70 bytes; stopped at the 20-minute limit)
Tool: `tools/cov_close4.py` (its coverage file was not written because the run was stopped; the hit counts below come from its log). Test: tests/test_dead_code.py.
* **`$806F-$807C` runs (V, 6 hits):** it is not part of Pac-Man's death. Routine `$805D` is the "other ghosts keep moving" part of the **ghost-eaten freeze** (`$7DB8-$7E1F`): an eyes ghost gets two extra steps while the eaten ghost's score is shown. Forcing "the other ghosts are eyes" (the state after eating two ghosts in a row) ran it 6 times.
* **`$89AC-$89B0` (V, 2 hits):** reached when ghost 2 or 4 leaves the house while moving up (countdown forced to end in that state; the state itself is natural, because those ghosts bounce up and down in the house). It queues one extra "down" move first.
* **Proven dead by the code itself (V, static, tests/test_dead_code.py):** `$5754` and `$57CF` (`LDA #$3A; ADC #$23` can never carry), `$6DEE` and `$8A3A` (a `BEQ` right after `LDA #$00` is always taken), `$53B2-$53B4` (a `BNE` right after `LDA #$62 + 3` is always taken; the bytes behind it are data).
* **Not dead, not yet reached:** `$5770, $57AA, $57EB, $5825` (carry when `$16 >= $EC` after `JSR $7819`; depends on the address computed there); `$52F9` (my Space-in-the-wait-loop attempt never reached the loop at `$52E9`); `$41D2` (the hidden rank prompt answered with one digit: scenario started but stopped by the time limit).
* **Inferred, not proven:** `$5D31-$5D3C` and `$5D66-$5D6D` need the flag `$1D = 0`, but the only writes to `$1D` are the start-up `1` (`$403A`) and a save/restore of that value around one routine (`$5264-$52A8`): it looks always 1, so these loops look dead (I). `$56E9` (`RTS` when `$19 = 0` lives to show): the code zeroes the lives byte and ends the game before that can be drawn (I).
* Remaining open: moved to TASK-021c.
