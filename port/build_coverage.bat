@echo off
rem Coverage build of the tests (TASK-034-COVERAGE): the same sources as build.bat, compiled without optimisation and with debug information (PDB) into out\cov, so that
rem Microsoft dotnet-coverage can measure which lines of the port's own C++ the tests run. See docs/CODE_COVERAGE_PORT.md.
setlocal
set "ROOT=%~dp0"
set "OUT=%ROOT%..\out\cov"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%i"
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%\obj_tests" mkdir "%OUT%\obj_tests"
set "FLAGS=/nologo /std:c++20 /W4 /WX /permissive- /utf-8 /EHsc /Od /Zi /FS %PACMAN_CRT%"
set "CORE=src\verify\sha256.cpp src\verify\dos33.cpp src\verify\verify.cpp src\datapack\datapack.cpp src\datapack\manifest.cpp src\hires\hires.cpp src\hires\replay.cpp src\core\cycleclock.cpp src\core\ticktrace.cpp src\core\gamestate.cpp src\core\input.cpp src\core\steptimer.cpp src\core\junction.cpp src\core\pacstep.cpp src\core\eating.cpp src\core\lives.cpp src\core\lifestart.cpp src\core\screen.cpp src\core\tickdraw.cpp src\core\cartoon.cpp src\core\sound.cpp src\core\squarewave.cpp src\core\deathtiming.cpp src\core\attract.cpp src\core\attractflow.cpp src\core\cpu6502.cpp src\core\monitor.cpp src\core\textscreen.cpp src\core\gamecore.cpp src\core\ghosttick.cpp src\core\ghostjunction.cpp src\core\ghostmode.cpp src\core\ghostdecide_gen.cpp src\core\gamedriver.cpp src\core\achievements.cpp src\video\colour.cpp src\video\monochrome.cpp src\video\screens.cpp"
pushd "%ROOT%"
cl %FLAGS% /Fd"%OUT%\pacman_tests.pdb" /Fo"%OUT%\obj_tests\\" /Fe"%OUT%\pacman_tests.exe" tests\test_main.cpp tests\test_sha256.cpp tests\test_verify.cpp tests\test_datapack.cpp tests\test_hires.cpp tests\test_hires_vectors.cpp tests\test_dots.cpp tests\test_script.cpp tests\test_sound.cpp tests\test_deathtiming.cpp tests\test_attract.cpp tests\test_flow.cpp tests\test_players.cpp tests\test_boot.cpp tests\test_monitor.cpp tests\test_hidden.cpp tests\test_textscreen.cpp tests\test_gamewindow.cpp tests\test_settings.cpp tests\test_cycleclock.cpp tests\test_ticktrace.cpp tests\test_gamestate.cpp tests\test_input.cpp tests\test_steptimer.cpp tests\test_junction.cpp tests\test_pacstep.cpp tests\test_eating.cpp tests\test_lives.cpp tests\test_lifestart.cpp tests\test_screen.cpp tests\test_tickdraw.cpp tests\test_gamecore.cpp tests\test_ghosthead.cpp tests\test_ghostjunction.cpp tests\test_ghostmove.cpp tests\test_ghostworld.cpp tests\test_ghostdecide.cpp tests\test_gamedriver.cpp src\app\gamewindow.cpp src\app\d3drenderer.cpp src\app\pausemenu.cpp src\app\crtfilter.cpp src\app\background_art.cpp src\app\settings.cpp src\app\audio.cpp tests\test_video.cpp tests\test_real_image.cpp tests\test_gamepad.cpp tests\test_achievements.cpp src\app\gamepad.cpp src\app\achievementbook.cpp %CORE% hid.lib setupapi.lib ole32.lib d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib /link /DEBUG
if errorlevel 1 (popd & exit /b 1)
popd
echo Built: %OUT%\pacman_tests.exe
exit /b 0
