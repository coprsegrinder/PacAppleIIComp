@echo off
rem Runs the C++ tests of the port and keeps the window open so you can read the result.
rem Needs the original disk image and the DOS 3.3 reference disk (the same files the project already uses).
cd /d "%~dp0"
set "ARGS=--image "Pacman (1983)(Atari).dsk" --reference "third_party\AppleWin\DOS 3.3 System Master - 680-0210-A.dsk""
if exist scratch\sound_clicks.bin set "ARGS=%ARGS% --clicks scratch\sound_clicks.bin"
if exist scratch\cartoon1_script.bin set "ARGS=%ARGS% --script scratch\cartoon1_script.bin"
out\bat\pacman_tests.exe %ARGS%
echo.
echo Look for the last line: "N tests, 0 failed" means everything passed.
pause
