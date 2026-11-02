@echo off
rem Plain build without CMake: compiles pacman_verify.exe, pacman_pack.exe and pacman_tests.exe with cl into ..\out\bat
rem Usage: build.bat            build only
rem        build.bat test       build, then run the tests
rem Needs Visual Studio Build Tools (found through vswhere). Nothing is downloaded.
setlocal
set "ROOT=%~dp0"
set "OUT=%ROOT%..\out\bat"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo vswhere.exe not found: install Visual Studio Build Tools with the C++ workload.
  exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
  echo No Visual Studio installation found.
  exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%\obj_cli" mkdir "%OUT%\obj_cli"
if not exist "%OUT%\obj_pack" mkdir "%OUT%\obj_pack"
if not exist "%OUT%\obj_tests" mkdir "%OUT%\obj_tests"
set "FLAGS=/nologo /std:c++20 /W4 /WX /permissive- /utf-8 /EHsc /O2"
set "CORE=src\verify\sha256.cpp src\verify\dos33.cpp src\verify\verify.cpp src\datapack\datapack.cpp src\datapack\manifest.cpp src\hires\hires.cpp src\hires\replay.cpp src\core\cycleclock.cpp src\core\ticktrace.cpp src\core\gamestate.cpp src\core\input.cpp src\core\steptimer.cpp src\core\junction.cpp src\core\pacstep.cpp src\core\eating.cpp src\core\lives.cpp src\core\lifestart.cpp src\core\screen.cpp src\video\colour.cpp"

pushd "%ROOT%"
if not exist "%OUT%\obj_view" mkdir "%OUT%\obj_view"
cl %FLAGS% /Fo"%OUT%\obj_view\\" /Fe"%OUT%\pacman_view.exe" src\app\view_main.cpp src\app\pagewindow.cpp %CORE% user32.lib gdi32.lib /link /ENTRY:wmainCRTStartup /SUBSYSTEM:WINDOWS
if errorlevel 1 (popd & exit /b 1)
cl %FLAGS% /Fo"%OUT%\obj_cli\\" /Fe"%OUT%\pacman_verify.exe" src\app\verify_main.cpp %CORE%
if errorlevel 1 (popd & exit /b 1)
cl %FLAGS% /Fo"%OUT%\obj_pack\\" /Fe"%OUT%\pacman_pack.exe" src\app\pack_main.cpp %CORE%
if errorlevel 1 (popd & exit /b 1)
cl %FLAGS% /Fo"%OUT%\obj_tests\\" /Fe"%OUT%\pacman_tests.exe" tests\test_main.cpp tests\test_sha256.cpp tests\test_verify.cpp tests\test_datapack.cpp tests\test_hires.cpp tests\test_hires_vectors.cpp tests\test_dots.cpp tests\test_script.cpp tests\test_gamewindow.cpp tests\test_cycleclock.cpp tests\test_ticktrace.cpp tests\test_gamestate.cpp tests\test_input.cpp tests\test_steptimer.cpp tests\test_junction.cpp tests\test_pacstep.cpp tests\test_eating.cpp tests\test_lives.cpp tests\test_lifestart.cpp tests\test_screen.cpp src\app\gamewindow.cpp tests\test_video.cpp tests\test_real_image.cpp %CORE% user32.lib gdi32.lib
if errorlevel 1 (popd & exit /b 1)
popd

if /i "%~1"=="test" (
  "%OUT%\pacman_tests.exe"
  exit /b %errorlevel%
)
echo Built: %OUT%\pacman_verify.exe, %OUT%\pacman_pack.exe and %OUT%\pacman_tests.exe
exit /b 0
