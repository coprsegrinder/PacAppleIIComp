# The 38 "forced" bytes and what the port must do with them (TASK-021e)

User requirement (2026-11-01): **the port must not ignore these bytes**, because they may produce unexpected results in some circumstances. This file decodes each one from the disassembly (V = read from the bytes; the forced run in section 7 of UNEXECUTED_CODE.md shows they execute) and states what the port must contain and how it is tested.
**Correction to UNEXECUTED_CODE.md section 7:** `$8A3A` is a `$00` byte (padding/data after an always-taken `BEQ`), not an `RTS`, and `$53B2-$53B4` are table data (`66 06 00`), not instructions. Executing them in the forced run proved nothing about behaviour. They are **not code**; the port needs no logic for them. The other 34 bytes are real code (below).

| Bytes | Instruction(s) (V) | What it does when it runs | Port requirement |
|---|---|---|---|
| `$5754, $5770, $57AA, $57CF, $57EB, $5825` (12) | `INC $17` after `BCC +2` | carry from the low byte of the pointer `$16` into the high byte `$17` while walking a table with `ADC #$14` or `ADC #$23` | Implement the pointer as a normal 16-bit address (carry is then automatic). With the real constants (`$625D`, `$657D`) it never carries; keep a unit test that adds across a page boundary. |
| `$5D31-$5D3C` (12) | when `$1D = 0` (from `$5D0D`): `JSR $5CE7; INC $0C; BNE; INC $0D; JMP $5D20` | the maze/wall draw routine (`$5CF0`) advances its x position by **1** cell instead of **2** (`$1D <> 0`: two `JSR $5CE7`, `+2`) | Implement the `$1D = 0` stepping mode of the draw routine, including the wrap of the 16-bit position counter `$0C/$0D`. |
| `$5D66-$5D6D` (8) | when `$1D = 0` (from `$5D59`): table pointer `$5DBF` instead of `$5F69` | the wall-drawing routine `$5D53` (called at start of a level, `$5662`) reads its triples (inferred: x start, x end, y; unverified) from the **other wall table at `$5DBF`** (`$1D <> 0` uses `$5F69`; the first bytes of the two tables differ by one in places: they are two variants of the same drawing) | The data pack must contain **both** tables (`$5DBF-$6112`, 852 bytes, and `$5F69`) and the port must be able to draw with either, selected by the flag `$1D`. |
| `$56E9` (1) | `RTS` when `$19 = 0` | the "draw remaining lives" routine draws nothing when there are no lives left | Implement the guard (draw loop runs `$19` times; zero means none). |
| `$6DEE` (1) | `RTS` after an always-taken `BEQ` | end of a routine; unreachable fall-through | Nothing to implement; the routine ends. |

## Open question (needs the oracle, not guesswork)
Nothing in the game writes `$1D = 0` (writes seen: start-up `1` at `$403A`, and a save/restore of that value around `$5264-$52A8`), so the `$1D = 0` mode looks unused. The user asked that it still be reproduced, so the port keeps it as a selectable mode (default = what the game uses, `$1D = 1`).

## How the port is tested against these
1. A differential test on the oracle: force `$1D = 0` and capture the drawn screen (hi-res page after `$5D53`), then compare the C++ draw with the same flag, pixel by pixel. Same for the six carry sites by calling the pointer walk with a start pointer near a page boundary and comparing the visited addresses. (Planned in M2/M4; not done yet.)
2. The data-pack manifest check: both wall tables are present and hash-verified (checked when the manifest is extended; TASK-021e follow-up).

## Update (TASK-027d)
The `$1D = 0` mode is the **white-wall mode**: table `$5DBF`, adjacent pixels (step 1) instead of every second pixel (step 2). `tools/wall_model.py` + tests/test_wall_model.py reproduce the oracle's page for both modes byte for byte, so the 20 bytes at `$5D31-$5D3C`/`$5D66-$5D6D` are specified and have a differential test; the C++ port must pass the same comparison. The carry fix-ups and the lives guard are still to be covered when the corresponding C++ code exists.
