@echo off
rem Starts the Pac-Man port with sound. Double-click this file.
rem Uses the game data file scratch\local.pmpk (built from your own disk image; never shipped).
cd /d "%~dp0"
if not exist out\bat\pacman_play.exe (
  echo Cannot find out\bat\pacman_play.exe - the program has not been built yet.
  pause
  exit /b 1
)
if not exist scratch\local.pmpk (
  echo Cannot find scratch\local.pmpk - the game data file is missing.
  pause
  exit /b 1
)
rem Shows which build this is, so you can confirm it is the newest one: the exe's build time and the latest commit.
for %%F in (out\bat\pacman_play.exe) do echo Build of pacman_play.exe: %%~tF
for /f "delims=" %%G in ('git log -1 --format^="%%h  %%cd  %%s" 2^>nul') do echo Latest commit: %%G
start "" out\bat\pacman_play.exe --pack scratch\local.pmpk --scale 3
