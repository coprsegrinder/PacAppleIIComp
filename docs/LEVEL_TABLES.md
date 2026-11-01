# Level-indexed tables and the fruit (TASK-031 analysis)

Tools: `tools/level_tables.py` (catalogue, writes scratch/level_tables.json), `tools/trace_fruit.py` (fruit set-up, pick-up, lifetime on the oracle). Tests: tests/test_level_tables.py.
V = verified by running the original code on the oracle, I = inferred from code, U = unknown. Names are mine. The full tables are original data and go to the data pack; this file gives their shape and the level-1 and last values only.

## 1. How levels index the tables (V)
* The level number `$55` starts at **1** and is incremented when a level is completed (`$6D10`). Helper `$6D1E` returns `min(level, 15)`; `$6D27` returns `min(level, 21)`; the fruit code clamps at 26. **The tables are indexed by the level itself, so entry 0 is never used in play** (that is why earlier notes list a leading 0 or a duplicate value; and the dot-penalty tables reach 0 from level 11, not 12).
* So from level 15 on (21 for the frightened time, 26 for the fruit) everything is constant.

## 2. The tables found (V: all exist and are read through the helpers; role V or I as marked)
| Table | At | Entries (levels) | Level 1 | Last | Shape | Role | Conf. |
|---|---|---|---|---|---|---|---|
| Pac-Man step delay | `$7851` | 15 | 9 | 4 | falls, 6 values | base of `$59` (PACMAN_RULES.md) | V |
| Dot penalty now / next | `$7A8C` / `$7A9C` | 15 | 2 / 2 | 0 / 0 | falls, 3 values | delay after eating | I |
| House countdown, ghosts 2/3/4 | `$54A4/B4/C4` | 15 | 112 / 48 / 144 | 8 / 4 / 9 | falls with small bumps (not monotonic) | release timing (MOVEMENT.md) | V |
| Ghost step delay: normal / after a turn / frightened / eyes | `$8723 / $8733 / $8743 / $8753` | 15 | 10 / 19 / 19 / 4 | 4 / 5 / 7 / 1 | all fall | speed (GHOST_MODES.md); **a turn costs almost twice a straight step at level 1** | I |
| Timer extension on a ghost eaten | `$6E19` | 15 words | 48 | 16 | falls, 4 values | added to the frightened timer | I |
| Energizer blink period | `$5733` | 15 | 80 | 80 | constant | screen blink | I |
| 1UP blink period | `$5903` | 15 | 8 | 8 | constant | screen blink | I |
| **Fruit score (hundreds, BCD)** | `$6CD4` | 15 | 1 | 80 | rises, 8 values | fruit eaten (section 3) | V |
| Frightened time | `$6DEF` | 21 words | 352 | 64 | falls with a bump | GHOST_MODES.md | V |
| **Fruit set-up** | `$6AFE` | 26 x 4 bytes | see 3 | | | sprite pointer (2 bytes), lifetime (2 bytes) | V |

The ghost delay tables count main-loop passes per ghost step (smaller = faster). At level 1 Pac-Man acts every 9 passes and a ghost every 10, so Pac-Man is slightly faster than a ghost, and a frightened ghost (19 passes) is much slower (V from the table values; real speed also depends on the cost of each pass, ARCHITECTURE.md).

## 3. Fruit (V standalone; one forced pick-up in play)
* **Appearance:** the counter `$57` starts at 64 and drops by one for every dot or energizer eaten (V). When it is 0 the main loop (`$6A7C`) loads the fruit for the current level, sets `$E7 = 1` (fruit shown) and reloads `$57 = 96` (V on the oracle). So **the first fruit appears after 64 items eaten and the second after 96 more (160 in total)**; a third would need 256 of the 248 items, so it cannot happen. In my play test I shortened the first count by presetting `$57`; the set-up and the reload to 96 are the original code (V), and 64 was observed as the starting value.
* **Where:** Pac-Man picks it up when his Y equals `$6B` and his X is `$8A` or `$8C` (`$79CD`), the point just below the ghost house (I from the code; one forced pick-up V: placing him there with the fruit shown gave +100 at level 1, `$E7` 1 -> 2, display timer 48).
* **Lifetime:** counted in Pac-Man ticks (one per `$59 == 1`), from the table: **256 at levels 1-4, 288 (5-6), 304 (7-8), 320 (9-10), 352 (11-22), 512 (23 and up)** (V: exactly 256, 288 and 512 ticks counted for levels 1, 5 and 26). After expiry the fruit is erased (`$E7 = 0`). After pick-up the score stays on screen for **48 ticks**, then it is erased.
* **Kinds (V pointers; names are I, from my own rendering of the sprites):** 8 fruit sprites of 12 rows x 3 bytes stored one after another: cherries, strawberry, peach/orange-like, apple, melon-like, a Galaxian-like ship, a bell, a key. By level: **1 / 2 / 3-4 / 5-6 / 7-8 / 9-10 / 11-12 / 13 and up**. **Level 23's table entry points one sprite further, at a 12 x 3 block that looks like the letters "H A L" (the original authors' credit) instead of a fruit; levels 24 and up go back to the key.** Whether this is an intended Easter egg or a table slip, and how it looks in the running game, is U (to check in M6). The level-fruit indicator at the bottom of the screen (`$6F13`) uses the same table.
* **Score by level:** 100, 300, 500, 500, 700, 700, 1000, 1000, 2000, 2000, 3000, 3000, then 5000 from level 13 (table values in hundreds, BCD). V standalone: the original routine `$6CB2` added exactly the table value for levels 1-30 and played the 100-click `$7C61` list each time, so **`$7C61` is the fruit-eaten sound (V on the call path)**. Nothing is added in demo mode.

## 4. Other level logic seen, not analysed (for M6)
`$8155` branches on levels 2, 5 and 9 (and above) into the intermission cartoons; `$8B4A` changes the collision rule from level 5 on (PACMAN_RULES.md); the maze is reloaded at each level start (`$531E`, `$5659`).

## 5. Still unknown
Whether level 23's HAL sprite really shows in play; the pass-cost model behind the delays (real per-level speeds need the cycle model, M8); the intermission schedule beyond levels 2, 5 and 9.
