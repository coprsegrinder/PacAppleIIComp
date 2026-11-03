# Sound: how the Apple II game makes noise (TASK-032a, analysis)

Tools: `tools/sound_model.py` (reads the lists, runs the real engines on the oracle, compares with a closed-form model), `tools/trace_sound.py` (real play: which sounds fire, same comparison). Tests: tests/test_sound_tools.py.
V = verified on the oracle (our own 6502/Apple II model; **not** compared with AppleWin audio, see TASK-022), I = inferred from code, U = unknown. Names are mine. Only counts, lengths and formulas are given here; the note lists themselves are original data and go to the data pack.

## 1. The hardware side (V)
* The only way to make sound is touching the speaker switch `$C030`; each touch is one click that flips the speaker cone. **There are exactly two such instructions in the whole program: `BIT $C030` at `$7BB5` (engine A) and at `$7D06` (engine B)** (byte scan). In a 60-million-instruction play run, **all 5,052 clicks came from those two instructions, 0 from anywhere else.**
* Ctrl-S flips the operand of both between `$C030` (sound) and `$C020` (cassette output, silent) (CONTROLS.md). The port needs only an on/off flag.
* Pitch is set purely by timing loops, so the port must reproduce **click times in CPU cycles** (1,022,727 per second). A tone of click spacing S cycles has frequency 1,022,727 / (2 x S).

## 2. The engines (V: model equals the original code cycle for cycle)
**Engine A (`$7BAC` tone, `$7BBC` list player):** a list of period bytes P ending with 0. Each byte plays 4 clicks.
* spacing inside a note = **9 x P + 11** cycles;
* from the last click of a note to the first of the next = **9 x P_next + 44** cycles, **plus 1 cycle if the list read (`LDA abs,Y`) crosses a page** (low byte of the list address + note index > 255). That one-cycle effect was found by the comparison failing at one note; the model now includes it.
* The list player does nothing in demo mode (`$E8 <> 0`): the attract demo is silent for these sounds (V from code `$7BC2`, I in play).

**Engine B (`$7CF7` tone, `$7D0D` list player):** a list of pairs (P, n) ending with P = 0; n clicks per note (the count is patched into the operand at `$7CF8`, then restored to 4).
* spacing inside a note = **31 x P + 11**; from the last click of a note to the first of the next = **31 x P_next + 30,624**, where 30,624 is the key poll (`$6F5E`, no key pressed) plus a fixed pause (`$7D7B`, 30 passes of the `$6CE8` delay).
* So the jingle's timing depends on the **key poll taking the same time**; a key pressed during the jingle changes the poll's path (not run: U).

**Engine C (`$7C51`):** 80 plain tones of period `$20` through `$7BAC`, with 37 extra cycles between tones.

## 3. The sounds (V for lists and timing, I for which game event)
| Sound | Entry | Engine | Notes | Clicks | Length | Triggered by |
|---|---|---|---|---|---|---|
| Dot eaten | `$7BDC` | A | 2 | 8 | 2,414 cycles (2.4 ms) | dot eating (V in play: 91 of 91 match the model) |
| Energizer | `$7C45` | A | 4 | 16 | 8,328 (8 ms) | energizer eating (V in play: 9 of 9) |
| **Ghost eaten** | `$7BE6` | A | 87 | 348 | 178,263 (0.174 s) | the ghost-eaten freeze (`$7DB8-$7E1F`), called from `$7DFC` (V in play: 4 ghosts eaten = 4 sounds, 3 catches = none; **corrected in TASK-021b: I had labelled it the death sound**) |
| **Pac-Man death** animation steps | table at `$7C8F` indexed by `$0A` = 1..6, played by `$7C82` (called from `$7EF2`) | A | 11, 12, 12, 12, 12, 18 | 44-72 | 4,763 to 30,112 | `$0A` counted 6,5,4,3,2,1 once per death (V in play: 3 deaths x 6 calls; the clicks of these were checked standalone only) |
| Sound at `$7C61` | `$7C61` | A | 25 | 100 | 88,713 (0.087 s) | fruit eaten (V: the last call of the fruit-eaten routine `$6CB2`, see LEVEL_TABLES.md) |
| Bonus life | `$7C51` | C | 80 | 320 | 97,435 (0.095 s) | 10,000-point bonus (I; the call site `$7132` is the bonus-life code, V in PACMAN_RULES.md) |
| Intermission/cartoon sound | `$7CF0` (list at `$7C9D`, the same list as death step 6) | A | 18 | 72 | 16,966 (0.017 s) | four call sites in the cartoon code (`$866F`, `$8690`, `$86B1`, `$86D2`) (V: never executed in cartoons 1, 2, 3, see section 5; the port is silent in cartoons) |
| Intro jingle | `$7D38` (called from `$568A`) | B | 29 | 2,212 | 5,120,766 (5.0 s) | start of every game that is not a demo (code reading, section 5; V standalone for the clicks) |

* **No other sound sites exist:** no siren and no sound when the fruit appears. Eating a ghost does make a sound (the 0.174 s list above); the call is inside the freeze routine that `$8B9D` calls, not in `$8B9D` itself (my first reading stopped too early).
* The model equals the oracle's clicks **exactly (every click, all 12 lists)**, standalone; in play the dot, energizer and ghost-eaten sounds match in every instance seen (103 of 103).

## 4. For the port (M7)
* Data pack: add the 11 note lists (they are in `$7BE3-$7CF0` and `$7D3F-$7D7A`) and the 6-word pointer table at `$7C8F`; the manifest tool must be extended.
* Audio output: generate the click list with the formulas above (engine constants 9/11/44/+1, 31/11/30,624, 37) at cycle resolution and render clicks as speaker toggles at the audio rate; the fidelity test is "click lists equal the oracle's", already automatable with these tools.
* A faithful game must **delay execution while a sound plays** (the engines busy-wait: the game is frozen during a sound, e.g. 0.17 s each time a ghost is eaten, 5 s for the jingle), so sound length is also game timing.

## 5. Open questions, answered in TASK-032-6 (2026-11-03)
* **When the jingle plays (code reading, V):** `$568A` is the only call. It runs when the demo flag `$E8` is 0 and the flag `$F4` is 0, then sets `$F4` to 1. `$F4` is cleared at `$5301`, in the new-game start `$52FC`. So the jingle plays once at the start of every real game, never in demo/attract mode (`$E8` <> 0; the sound routine `$7BBC` also returns at once when `$E8` <> 0). The port plays it once at program start only; a second game needs the game-over/new-game flow (TASK-029i).
* **Cartoon sounds (V, oracle):** the four call sites of `$7CF0` (`$866F`, `$8690`, `$86B1`, `$86D2`) were hooked together with `$7BBC` and `$7CF0` while cartoons 1, 2 and 3 ran to their end: 0 hits, 0 clicks. Those runs started from the hidden menu (`$E8`=1); a run of cartoon 1 with `$E8` forced to 0 also had 0 clicks and the same 5,597,305 cycles. The sites are in code the intermissions never reach, so the port correctly plays nothing in cartoons. (Not tested: a cartoon reached by a natural level end on the oracle; the cartoon code is the same, so I expect the same.)
* **Key during the jingle (code reading, I):** after each of the 29 notes the game polls the keyboard (`$6F5E`). A pressed key is read and consumed: arrow keys set the direction `$63`, Ctrl-S flips the sound patch (`$7BB6`, `$7D07`), Esc waits for another key. With a key the poll takes longer than without (not measured), so the jingle would be a little longer. The port keeps keys queued and delivers them at the first tick (same direction result; jingle length stays exactly 5.04 s).
* **Still unknown:** real-hardware audio shaping (the speaker is a plain click toggle, a square wave is the reference); AppleWin audio comparison (TASK-022, only if wanted); where inside the death animation the six death steps fall (needs the death animation, M8).
