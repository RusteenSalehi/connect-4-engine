# connect-4-engine

## Build

Requires CMake 3.21+, Ninja, and a C++20 compiler. On Windows, run these from a
"Developer PowerShell / Command Prompt for VS 2022" (ARM64 or x64) so `cl` is on the PATH.

```
cmake --preset release
cmake --build --preset release
```

Debug build: replace `release` with `debug` (output goes to `build/debug`).

With clang or gcc (no presets):

```
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
```

## Test

```
ctest --preset release
```

or run the test binary directly for doctest's full output:

```
build/release/c4tests
```
