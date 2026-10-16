# connect-4-engine

A Connect-4 engine in C++20: bitboard board, negamax search with alpha-beta pruning, iterative
deepening, a transposition table keyed by Zobrist hashing, and a window-counting evaluation with
all weights in one place. It comes with a benchmark that reports nodes, nodes per second and time
per move by depth plus an ablation table, a terminal CLI, and tests that prove every combination
of search techniques returns the same score as plain minimax.

How it works and why: [DESIGN.md](DESIGN.md).

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

## CLI

```
c4cli analyze [--moves 4453] [--depth N] [--time MS]
c4cli play [--engine-first] [--time MS]
```

Moves are digits 1 to 7, one per ply, X first. Without `--depth` or `--time` the engine thinks
for 1 second. `play` starts a game in the terminal; you are X unless `--engine-first` is given.

Example (`build/release/c4cli analyze --moves 4453 --depth 16`, Windows 11 ARM64 laptop, Release
build):

```
. . . . . . .
. . . . . . .
. . . . . . .
. . . . . . .
. . . O . . .
. . O X X . .
1 2 3 4 5 6 7
X to move

depth  score   best  nodes        ms
    1       4     5           8       0.0
    2      -2     6          40       0.0
    3       7     4         127       0.0
    4      -2     4         379       0.0
    5       7     4         924       0.1
    6      -2     4        2033       0.2
    7       7     4        4533       0.5
    8      -3     4        9437       0.9
    9       6     4       17190       1.7
   10      -3     4       32012       3.2
   11       7     4       55651       5.7
   12      -3     4       98493       9.7
   13       7     4      169813      17.0
   14      -2     4      275282      26.3
   15       5     4      452766      43.6
   16      -1     4      674861      62.2

Best move: 4
Score: -1 (heuristic)
Principal variation: 4 4 4 3 3
Nodes: 674861 in 62.2 ms (10.84 M nodes/s), TT hit rate 65.7%
```

Scores are from the side to move's point of view, in evaluation units; proven results print as
"win in N plies" or "loss in N plies". Scores alternate between odd and even depths because the
evaluation favors whoever moved last.

## Benchmark

```
c4bench [--depths 2,4,6,8,10,12,14,16] [--positions 100] [--ablation-depth 10]
```

The defaults are shown. The seed and TT size are fixed constants, so runs are comparable.
Recorded runs live in [bench/results](bench/results).

### Results: Windows 11 ARM64 laptop, Release build (MSVC 19.44)

Verbatim output of `build/release/c4bench` with default flags, from
[bench/results/windows11-arm64-release.md](bench/results/windows11-arm64-release.md). Only its
title line is replaced by the heading above.

100 random positions of 4 to 16 plies (fixed seed), one search per position, TT of 2^20 entries cleared before every search. Times are per search, i.e. per move.

## Time per move by depth (all techniques on)

| Depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 2 | 26 | 0.004 | 0.005 | 0.016 | n/a | 12.4% | 87.0% |
| 4 | 213 | 0.026 | 0.027 | 0.094 | 8.16 | 36.1% | 82.6% |
| 6 | 1153 | 0.130 | 0.126 | 0.424 | 8.86 | 45.6% | 81.6% |
| 8 | 4897 | 0.583 | 0.512 | 2.476 | 8.41 | 50.9% | 81.6% |
| 10 | 17813 | 1.770 | 1.454 | 12.225 | 10.06 | 52.9% | 80.3% |
| 12 | 56312 | 5.546 | 3.910 | 46.987 | 10.15 | 54.7% | 79.1% |
| 14 | 167655 | 16.131 | 8.768 | 172.640 | 10.39 | 54.3% | 77.0% |
| 16 | 490270 | 47.572 | 16.770 | 734.889 | 10.31 | 52.2% | 73.9% |

## Ablation (one technique off at a time)

| Configuration | Depth | Mean nodes | Nodes vs all on | Mean ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---|---:|---:|---:|---:|---:|---:|---:|
| All on | 10 | 17813 | 1.0x | 1.722 | 10.34 | 52.9% | 80.3% |
| No alpha-beta | 10 | 1967171 | 110.4x | 237.732 | 8.27 | 61.7% | n/a |
| No transposition table | 10 | 64754 | 3.6x | 6.319 | 10.25 | n/a | 70.5% |
| No move ordering | 10 | 235149 | 13.2x | 30.979 | 7.59 | 57.5% | 32.5% |
| No iterative deepening | 10 | 15768 | 0.9x | 1.671 | 9.43 | 26.0% | 65.0% |
| All on | 7 | 2483 | 1.0x | 0.300 | 8.27 | 49.6% | 82.6% |
| All off (pure minimax) | 7 | 276450 | 111.4x | 37.357 | 7.40 | n/a | n/a |
