cmake_minimum_required(VERSION 3.21)
project(connect4_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Plain C++20 instead of gnu++20, so the code stays portable across MSVC, clang and gcc.
set(CMAKE_CXX_EXTENSIONS OFF)

# Single-config generators (Ninja, Makefiles) leave the build type empty unless told otherwise,
# which silently produces an unoptimized build. An engine benchmarked without optimization is
# misleading, so Release is the default.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# One place to set warning flags, applied to every target we own (not to doctest).
function(c4_set_warnings target)
  if(MSVC)
    # /permissive- turns off MSVC's non-standard extensions, so code that builds here also
    # builds with clang and gcc.
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
endfunction()

# ---------------------------------------------------------------------------------------------
# Engine library
# ---------------------------------------------------------------------------------------------
add_library(c4engine STATIC
  engine/src/position.cpp
  engine/src/zobrist.cpp
  engine/src/tt.cpp
  engine/src/eval.cpp
)
target_include_directories(c4engine PUBLIC engine/include)
c4_set_warnings(c4engine)

# ---------------------------------------------------------------------------------------------
# Tests (doctest, pinned to a release tag so every checkout builds against identical code)
# ---------------------------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.11
  GIT_SHALLOW    TRUE
  # doctest is header-only. Pointing SOURCE_SUBDIR at a folder that does not exist makes
  # FetchContent download the sources without running doctest's own CMakeLists.txt, which
  # targets an old CMake version and would print deprecation warnings on every configure.
  SOURCE_SUBDIR  do-not-add-subdirectory
)
FetchContent_MakeAvailable(doctest)

add_executable(c4tests
  tests/test_main.cpp
  tests/test_position.cpp
  tests/test_zobrist.cpp
  tests/test_tt.cpp
  tests/test_eval.cpp
)
# SYSTEM marks doctest's headers as third-party, so our strict warning level does not
# report warnings from code we do not own.
target_include_directories(c4tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})
target_link_libraries(c4tests PRIVATE c4engine)
c4_set_warnings(c4tests)

enable_testing()
add_test(NAME c4tests COMMAND c4tests)

# ---------------------------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------------------------
add_executable(c4bench bench/bench.cpp)
target_link_libraries(c4bench PRIVATE c4engine)
c4_set_warnings(c4bench)

add_executable(c4cli cli/cli.cpp)
target_link_libraries(c4cli PRIVATE c4engine)
c4_set_warnings(c4cli)
