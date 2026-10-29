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
  src/verify/verify.cpp)
target_include_directories(pacman_core_verify PUBLIC src)

add_executable(pacman_verify_cli src/app/verify_main.cpp)
target_link_libraries(pacman_verify_cli PRIVATE pacman_core_verify)
set_target_properties(pacman_verify_cli PROPERTIES OUTPUT_NAME pacman_verify)

# --- tests ---
add_executable(pacman_tests
  tests/test_main.cpp
  tests/test_sha256.cpp
  tests/test_verify.cpp
  tests/test_real_image.cpp)
target_link_libraries(pacman_tests PRIVATE pacman_core_verify)

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
