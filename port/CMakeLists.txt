cmake_minimum_required(VERSION 3.20)
project(PacmanApplePort LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /WX /permissive- /utf-8 /EHsc)
endif()

# --- disk-image verification (no original data inside; only the expected hashes) ---
add_library(pacman_core_verify STATIC
  src/verify/sha256.cpp
  src/verify/dos33.cpp
  src/verify/verify.cpp
  src/datapack/datapack.cpp
  src/datapack/manifest.cpp
  src/hires/hires.cpp
  src/hires/replay.cpp
  src/core/cycleclock.cpp
  src/core/ticktrace.cpp
  src/core/gamestate.cpp
  src/core/input.cpp
  src/core/steptimer.cpp
  src/core/junction.cpp
  src/core/pacstep.cpp
  src/core/eating.cpp
  src/core/lives.cpp
  src/core/lifestart.cpp
  src/core/screen.cpp
  src/core/tickdraw.cpp
  src/core/cartoon.cpp
  src/core/sound.cpp
  src/core/squarewave.cpp
  src/core/deathtiming.cpp
  src/core/attractflow.cpp
  src/core/cpu6502.cpp
  src/core/monitor.cpp
  src/core/textscreen.cpp
  src/core/attract.cpp
  src/core/gamecore.cpp
  src/core/ghosttick.cpp
  src/core/ghostjunction.cpp
  src/core/ghostmode.cpp
  src/core/ghostdecide_gen.cpp
  src/core/gamedriver.cpp
  src/video/colour.cpp
  src/video/monochrome.cpp
  src/video/screens.cpp)
target_include_directories(pacman_core_verify PUBLIC src)

add_executable(pacman_verify_cli src/app/verify_main.cpp)
target_link_libraries(pacman_verify_cli PRIVATE pacman_core_verify)
set_target_properties(pacman_verify_cli PROPERTIES OUTPUT_NAME pacman_verify)

add_executable(pacman_pack_cli src/app/pack_main.cpp)
target_link_libraries(pacman_pack_cli PRIVATE pacman_core_verify)
set_target_properties(pacman_pack_cli PROPERTIES OUTPUT_NAME pacman_pack)

if(WIN32)
  add_executable(pacman_view src/app/view_main.cpp src/app/pagewindow.cpp)
  target_link_libraries(pacman_view PRIVATE pacman_core_verify user32 gdi32)
  target_link_options(pacman_view PRIVATE /ENTRY:wmainCRTStartup /SUBSYSTEM:WINDOWS)

  add_executable(pacman_play src/app/play_main.cpp src/app/gamewindow.cpp src/app/d3drenderer.cpp src/app/pausemenu.cpp src/app/background_art.cpp src/app/settings.cpp src/app/audio.cpp)
  target_link_libraries(pacman_play PRIVATE pacman_core_verify user32 gdi32 winmm ole32 d3d11 dxgi d3dcompiler)
  target_link_options(pacman_play PRIVATE /ENTRY:wmainCRTStartup /SUBSYSTEM:WINDOWS)
endif()

# --- tests ---
add_executable(pacman_tests
  tests/test_main.cpp
  tests/test_sha256.cpp
  tests/test_verify.cpp
  tests/test_datapack.cpp
  tests/test_hires.cpp
  tests/test_hires_vectors.cpp
  tests/test_dots.cpp
  tests/test_script.cpp
  tests/test_sound.cpp
  tests/test_deathtiming.cpp
  tests/test_attract.cpp
  tests/test_flow.cpp
  tests/test_players.cpp
  tests/test_boot.cpp
  tests/test_monitor.cpp
  tests/test_hidden.cpp
  tests/test_textscreen.cpp
  tests/test_gamewindow.cpp
  tests/test_settings.cpp
  tests/test_cycleclock.cpp
  tests/test_ticktrace.cpp
  tests/test_gamestate.cpp
  tests/test_input.cpp
  tests/test_steptimer.cpp
  tests/test_junction.cpp
  tests/test_pacstep.cpp
  tests/test_eating.cpp
  tests/test_lives.cpp
  tests/test_lifestart.cpp
  tests/test_screen.cpp
  tests/test_tickdraw.cpp
  tests/test_gamecore.cpp
  tests/test_ghosthead.cpp
  tests/test_ghostjunction.cpp
  tests/test_ghostmove.cpp
  tests/test_ghostworld.cpp
  tests/test_ghostdecide.cpp
  tests/test_gamedriver.cpp
  src/app/gamewindow.cpp src/app/d3drenderer.cpp src/app/pausemenu.cpp src/app/background_art.cpp src/app/settings.cpp src/app/audio.cpp
  tests/test_video.cpp
  tests/test_real_image.cpp)
target_link_libraries(pacman_tests PRIVATE pacman_core_verify user32 gdi32 ole32 d3d11 dxgi d3dcompiler)

enable_testing()
add_test(NAME unit COMMAND pacman_tests)

# Optional: real-image checks. Configure with -DPACMAN_DSK=<game.dsk> [-DPACMAN_REFERENCE_DSK=<other DOS 3.3 disk>].
# The images stay outside the repository.
if(PACMAN_DSK)
  set(real_args --image "${PACMAN_DSK}")
  if(PACMAN_REFERENCE_DSK)
    list(APPEND real_args --reference "${PACMAN_REFERENCE_DSK}")
  endif()
  add_test(NAME real_image COMMAND pacman_tests ${real_args})
endif()
