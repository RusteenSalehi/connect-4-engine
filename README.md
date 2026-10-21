# connect-4-engine

[![CI](https://github.com/RusteenSalehi/connect-4-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/RusteenSalehi/connect-4-engine/actions/workflows/ci.yml)
[![Pages](https://github.com/RusteenSalehi/connect-4-engine/actions/workflows/deploy.yml/badge.svg)](https://github.com/RusteenSalehi/connect-4-engine/actions/workflows/deploy.yml)

**[Live demo](https://rusteensalehi.github.io/connect-4-engine/)**: play against the engine in your
browser (compiled to WebAssembly).

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

On Linux (gcc), use the `linux-release` and `linux-debug` presets (output in `build/linux-release`
and `build/linux-debug`):

```
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
```

## WebAssembly

Built with **Emscripten 6.0.12** (pinned; install it with
`./emsdk install 6.0.12 && ./emsdk activate 6.0.12` and load `emsdk_env.sh`, which sets
`EMSDK` for the preset). The smoke test was run with Node v24.22.0 (LTS).

```
cmake --preset wasm
cmake --build --preset wasm
node web/scripts/smoke.mjs
```

Output: `build/wasm/c4.js` (ES6 module factory for web, worker and Node) and
`build/wasm/c4.wasm`. Exported functions, all stateless (pass the whole move string each call):

```
int    c4_choose_move(const char* moves, int difficulty, double seed);  // column 0 to 6, or -1
double c4_last_depth();     // stats of the last c4_choose_move
double c4_last_score();
double c4_last_nodes();
double c4_last_time_ms();
double c4_last_nps();
double c4_win_score();      // score scale: a win at ply p scores c4_win_score() - p
double c4_min_win_score();  // any score at least this large in size is a proven result
double c4_winning_cells(const char* moves);  // bit col * 7 + row of the winning cells, or 0
```

`difficulty` is 0 (Easy) to 3 (Impossible). Call them with `ccall` or `cwrap`, as
`web/scripts/smoke.mjs` does.

## Web app

A browser game in `web/` (Vite 8.3.3, TypeScript 7.0.2, no framework). The engine runs in a Web
Worker, so Impossible's 1 s search never freezes the page. Needs the Emscripten setup above;
`npm run dev` and `npm run build` first run `npm run wasm`, which builds the wasm preset and
copies `c4.js` and `c4.wasm` into `web/src/wasm/`.

```
cd web
npm install
npm run dev       # http://localhost:5173/connect-4-engine/
npm run build     # type-checks, then writes web/dist/
npm run preview   # serves web/dist/ at http://localhost:4173/connect-4-engine/
```

The site is built for the base path `/connect-4-engine/` (GitHub Pages). After rebuilding the
WebAssembly module, restart `npm run dev`: in WSL on a `/mnt/c` checkout, Vite does not notice the
copied files change and keeps serving the old `c4.js`.

## Continuous integration and deployment

- `.github/workflows/ci.yml` (every push and pull request): native Release build with gcc and
  the doctest suite on `ubuntu-latest`, plus the WebAssembly build and Node smoke test.
- `.github/workflows/deploy.yml` (push to `main`): installs Emscripten 6.0.12, builds the module
  and the site, and publishes `web/dist` to GitHub Pages. Build output is never committed.

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
c4cli play [--engine-first] [--difficulty easy|medium|hard|impossible]
```

Moves are digits 1 to 7, one per ply, X first. `analyze` runs one full search; without `--depth`
or `--time` it thinks for 1 second. `play` starts a game in the terminal; you are X unless
`--engine-first` is given, and the engine plays at `--difficulty` (default `impossible`):

| Level | How it picks a move |
|---|---|
| Easy | Searches 1 ply, then picks at random, weighted toward better moves (softmax). Takes a win in one and blocks a loss in one, otherwise makes mistakes. |
| Medium | Searches 3, 4 or 5 plies (random each move), then picks at random among moves within 3 points of the best. |
| Hard | Searches 7 plies and plays a best move (random among ties). |
| Impossible | Searches at least 10 plies, then deeper until 1 second is used or the result is proven; plays the most central best move. |

Details: [DESIGN.md](DESIGN.md#difficulty-levels).

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
    4      -2     4         379       0.1
    5       7     4         924       0.1
    6      -2     4        2033       0.3
    7       7     4        4533       0.6
    8      -3     4        9437       1.2
    9       6     4       17190       2.1
   10      -3     4       32012       3.5
   11       7     4       55651       6.1
   12      -3     4       98493      10.7
   13       7     4      169813      18.5
   14      -2     4      275282      28.1
   15       5     4      452766      47.5
   16      -1     4      674861      68.3

Best move: 4
Score: -1 (heuristic)
Principal variation: 4 4 4 3 3
Nodes: 674861 in 68.3 ms (9.88 M nodes/s), TT hit rate 65.7%
```

Scores are from the side to move's point of view, in evaluation units; proven results print as
"win in N plies" or "loss in N plies". Scores alternate between odd and even depths because the
evaluation favors whoever moved last.

## Evaluation tuning

```
c4tune [--depth 4]
```

Hill climbing on engine-vs-engine matches: each round changes one evaluation weight by +1 or -1
and keeps the change only if it scores above 55% against the current weights over all 49
two-ply openings, each played twice with colors swapped (98 games). 60 rounds at depth 4, then
the final weights play the original ones at depth 4 and at depth 6, a depth tuning never used.
Method and discussion: [DESIGN.md](DESIGN.md#evaluation-tuning).

**Result: tuning found no improvement, so the default weights are unchanged.** No candidate
reached 55%; the best, centerStone 3 to 4, scored 52.6%. Excerpt of the real output (the engine
is deterministic, so this is the same on any machine; it was produced on a Windows 11 ARM64
laptop, Release build). Rounds 7 to 60 repeat rounds 1 to 6 exactly and are left out:

```
Tuning depth 4, 60 rounds, step 1, accept above 55%
Start: {threeOpen 5, twoOpen 2, centerStone 3}
Sanity check, defaults vs defaults: +41 =16 -41, 49.0 / 98 (50.0%)

Round  1: threeOpen +1 vs best: +38 =15 -45, 45.5 / 98 (46.4%)
Round  2: threeOpen -1 vs best: +39 =13 -46, 45.5 / 98 (46.4%)
Round  3: twoOpen +1 vs best: +41 =11 -46, 46.5 / 98 (47.4%)
Round  4: twoOpen -1 vs best: +37 =11 -50, 42.5 / 98 (43.4%)
Round  5: centerStone +1 vs best: +46 =11 -41, 51.5 / 98 (52.6%)
Round  6: centerStone -1 vs best: +34 =18 -46, 43.0 / 98 (43.9%)

0 of 60 changes accepted. Final: {threeOpen 5, twoOpen 2, centerStone 3}
Final vs original at the tuning depth: +41 =16 -41, 49.0 / 98 (50.0%)
Final vs original at the validation depth (6): +42 =14 -42, 49.0 / 98 (50.0%)
```

Both final matches are the defaults playing themselves, so 50% is expected.

## Benchmark

```
c4bench [--depths 2,4,6,8,10,12,14,16] [--positions 100] [--ablation-depth 10]
```

The defaults are shown. The seed and TT size are fixed constants, so runs are comparable. Besides
the search tables, the bench runs every difficulty level on the same positions (about two minutes,
mostly Impossible's 1 second budget).
Recorded runs live in [bench/results](bench/results).

### Results: WSL2 Ubuntu, gcc, Snapdragon ARM64, Release

Verbatim output of `build/linux-release/c4bench` with default flags (gcc 15.2, WSL2 Ubuntu 26.04
on a Snapdragon X laptop), from
[bench/results/wsl2-ubuntu-gcc-arm64-release.md](bench/results/wsl2-ubuntu-gcc-arm64-release.md).
Only its title line is replaced by the heading above. Recorded on battery power: timings varied by
about 40% between repeated runs, while node counts are exact and identical to the MSVC run in
[bench/results/windows11-arm64-release.md](bench/results/windows11-arm64-release.md).

100 random positions of 4 to 16 plies (fixed seed), one search per position, TT of 2^20 entries cleared before every search. Times are per search, i.e. per move.

## Time per move by depth (all techniques on)

| Depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 2 | 26 | 0.013 | 0.013 | 0.038 | 2.00 | 12.4% | 87.0% |
| 4 | 213 | 0.050 | 0.051 | 0.310 | 4.28 | 36.1% | 82.6% |
| 6 | 1153 | 0.185 | 0.178 | 0.714 | 6.22 | 45.6% | 81.6% |
| 8 | 4897 | 0.804 | 0.674 | 4.025 | 6.09 | 50.9% | 81.6% |
| 10 | 17813 | 2.597 | 2.039 | 18.073 | 6.86 | 52.9% | 80.3% |
| 12 | 56312 | 10.149 | 6.000 | 83.674 | 5.55 | 54.7% | 79.1% |
| 14 | 167655 | 35.978 | 19.557 | 297.971 | 4.66 | 54.3% | 77.0% |
| 16 | 490270 | 90.407 | 24.576 | 1565.364 | 5.42 | 52.2% | 73.9% |

## Ablation (one technique off at a time)

| Configuration | Depth | Mean nodes | Nodes vs all on | Mean ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---|---:|---:|---:|---:|---:|---:|---:|
| All on | 10 | 17813 | 1.0x | 2.682 | 6.64 | 52.9% | 80.3% |
| No alpha-beta | 10 | 1967171 | 110.4x | 474.401 | 4.15 | 61.7% | n/a |
| No transposition table | 10 | 64754 | 3.6x | 9.443 | 6.86 | n/a | 70.5% |
| No move ordering | 10 | 235149 | 13.2x | 57.397 | 4.10 | 57.5% | 32.5% |
| No iterative deepening | 10 | 15768 | 0.9x | 3.356 | 4.70 | 26.0% | 65.0% |
| All on | 7 | 2483 | 1.0x | 0.413 | 6.01 | 49.6% | 82.6% |
| All off (pure minimax) | 7 | 276450 | 111.4x | 76.377 | 3.62 | n/a | n/a |

## Difficulty levels (chooseMove)

| Level | Mean depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s |
|---|---:|---:|---:|---:|---:|---:|
| Easy | 1.0 | 7 | 0.006 | 0.004 | 0.086 | n/a |
| Medium | 3.9 | 421 | 0.077 | 0.038 | 0.356 | 5.46 |
| Hard | 7.0 | 4936 | 0.827 | 0.663 | 4.566 | 5.97 |
| Impossible | 15.4 | 2810993 | 550.156 | 1000.092 | 1002.772 | 5.11 |

