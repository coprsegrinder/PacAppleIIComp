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
  src/video/colour.cpp)
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
  tests/test_gamewindow.cpp
  tests/test_cycleclock.cpp
  tests/test_ticktrace.cpp
  src/app/gamewindow.cpp
  tests/test_video.cpp
  tests/test_real_image.cpp)
target_link_libraries(pacman_tests PRIVATE pacman_core_verify user32 gdi32)

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
