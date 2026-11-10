# Release notes - Pac-Man (Apple II) native Windows port

Release v1.0.0.

## What it is
An unofficial, non-commercial fan project: a native Windows program that plays the 1983 Atari version of Pac-Man for the Apple II. It is **not an emulator**: the game's logic, timing, drawing and sound were rebuilt in C++ and compared tick by tick with a reference model that runs the original program. The game's data comes from **your own copy** of the original disk image; the image and the original code are not part of the download.

## What you need
* Windows 10 or 11 with a graphics card that supports Direct3D 11.
* Your own disk image of the game: a 140 K DOS-order `.dsk` (or `.do`) file. Other formats (`.po`, `.woz`, `.nib`) are refused, and so is any image that is not the exact original.

## Install and start
1. Unpack `PacMan-AppleII.zip` anywhere (keep the "Apple II roms" folder next to `PacMan.exe`).
2. Put your disk image in the same folder.
3. Double-click `PacMan.exe`. The first time it reads the image once, checks it, and keeps what it needs in `%LOCALAPPDATA%\PacmanAppleIIPort`; after that the image is not needed. To start from scratch delete that folder.

## What is in it
* **The whole game as the original plays it:** four ghosts with their modes, timers and speeds per level, dots, energizers, fruit, extra lives, scoring, every level, the three intermissions, the death animation, GAME OVER and the attract loop, one to five players taking turns, the hidden menu. The original's quirks are kept on purpose.
* **Sound:** the Apple II speaker, with the original's timing.
* **Display options** (Esc opens the pause menu): borderless full screen or a resizable window, whole-number scaling, backgrounds (black, ambient glow, an Apple II monitor with its bezel, the Apple II desktop), original colour or green phosphor, and a CRT filter (scanlines, softness, glow, curvature and more, with presets).
* **Controls:** keyboard, DualShock 4 and DualSense (USB or Bluetooth) and Xbox (XInput) controllers, found automatically. **Xbox controller support is implemented only in theory** (from Microsoft's documentation): it has not been tried on a real Xbox controller.
* **Vibration** on the supported pads (Off / Low / Medium / High; on an Xbox controller in theory only, like the rest of its support).
* **Light bar** on the DualShock 4 and DualSense, in the game's own Apple II colours: white in play, blue while the ghosts are blue, a flash in the ghost's colour when one is eaten or catches you, and the DualSense's player LEDs show your lives. Off / Low / Medium / High under Pause menu > Controls.
* **Achievements:** the 22 achievements (190 points) of the RetroAchievements set for this game, checked by the program. They are kept on your computer only; nothing is sent anywhere.
* Your settings are kept between runs.

## How closely it follows the original
Compared with the reference model on 41 recordings: all 29,494 ordinary game ticks take exactly the same number of CPU cycles as the original, the game state of every tick is equal, the drawing calls are equal, and 800 of the model's screens match pixel for pixel. A few rare moments are timed from measurements instead of being counted instruction by instruction: being caught, the end of a level, the bonus life, the fruit, and the Esc pause (the pause menu replaces the original's Esc pause).

## Known limits
* Windows only. The picture rate is the original's (60.05 a second); there are no other frame-rate options.
* The colour model is the usual Apple II hi-res one; the "original colour" mode fills the dot between two lit dots of the same colour, which is a display choice, not the exact dot picture (green phosphor mode shows the exact dots).
* The level-complete flash of the light bar is the author's own design: the port does not show the original's maze flash.
* Tried on real hardware: the DualShock 4 and the DualSense (including the light bar). **Not tried: Xbox (XInput) controllers**, whose support is implemented only in theory. Other programs that write a pad's light at the same time (Steam Input, DS4Windows) will fight over it.
* Facts about the original that nobody has established (which revision the image is, whether its DOS is Atari's) are listed in `docs/KNOWN_ISSUES.md`.

## Notices
The download includes two Apple II ROM dumps (monitor and character ROM, copyright Apple Computer, Inc.) because the program needs them. Not affiliated with or endorsed by Atari, Namco, Apple or RetroAchievements; all trademarks belong to their owners. See `NOTICES.txt` in the zip.
