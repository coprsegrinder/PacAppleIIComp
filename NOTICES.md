# Notices

This is an unofficial, non-commercial fan project. It is not affiliated with or endorsed by Atari, Namco, Apple or RetroAchievements. All trademarks belong to their owners.

## The game
Pac-Man for the Apple II (Atari, 1983) belongs to its authors and owners. This repository does **not** contain its disk image, program code, graphics or sound as files; the program builds its game data from a disk image that the player already owns, and refuses any other image. The screenshots in `docs/images` show the program running.

## Apple II ROM files (`Apple II roms/`)
Two ROM dumps of the Apple II Plus are kept in the repository and in the release because the program needs them to run:
* `Apple II plus ROM Pages F8-FF - 341-0020 - Autostart Monitor.bin`
* `Apple II Character ROM - 341-0036.bin`

Their contents are copyright Apple Computer, Inc.; they are included because the program needs them to run, and they remain the property of their owner. Other Apple II ROM dumps that were used for research are not tracked (earlier commits still contain them; see `docs/KNOWN_ISSUES.md`).

## Achievements
The 22 achievements are from the RetroAchievements set for "Pac-Man" (Apple II), game 13932 (https://retroachievements.org/game/13932). Set by **Kisumebuckets**; logic by **xnaivx**. The titles and descriptions are used with small corrections of spelling and "goblins" written as "ghosts". They are checked by this program itself; nothing is sent to RetroAchievements and no account is used.

## Tools used only during development
An Apple II emulator (AppleWin) was used as a reference for comparisons; it is kept outside the repository (`third_party/`, ignored) and is not part of the release.

## This program
The source code was written for this project from the behaviour of the original game, checked against a reference model of the original program. It has no third-party libraries; it uses the Windows APIs (Direct3D 11, WASAPI, XInput loaded at run time, raw HID for the Sony controllers).
