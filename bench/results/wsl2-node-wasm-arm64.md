# c4bench: WebAssembly in Node v24.22.0, Emscripten 6.0.12 -O3, Snapdragon X Elite (X1E80100), AC power

`node build/wasm/c4bench.js` with default flags (the same `bench/bench.cpp`, built by the `wasm`
preset), recorded 2026-10-21 in WSL2 Ubuntu 26.04 on a Dell XPS 13 9345 plugged in (AC), right
after the native runs. Second of three back-to-back runs; it is the median run at depth 12 (mean
ms 3.512, 3.887 and 4.050). Node counts are identical to the native run at every fixed depth.

100 random positions of 4 to 16 plies (fixed seed), one search per position, TT of 2^20 entries cleared before every search. Times are per search, i.e. per move.

## Time per move by depth (all techniques on)

| Depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 2 | 26 | 0.008 | 0.006 | 0.094 | n/a | 12.4% | 87.0% |
| 4 | 213 | 0.024 | 0.023 | 0.130 | 8.91 | 36.1% | 82.6% |
| 6 | 1153 | 0.095 | 0.089 | 0.549 | 12.14 | 45.6% | 81.6% |
| 8 | 4897 | 0.345 | 0.293 | 1.473 | 14.21 | 50.9% | 81.6% |
| 10 | 17813 | 1.179 | 0.919 | 8.354 | 15.11 | 52.9% | 80.3% |
| 12 | 56312 | 3.887 | 2.249 | 32.431 | 14.49 | 54.7% | 79.1% |
| 14 | 167655 | 10.591 | 5.296 | 114.756 | 15.83 | 54.3% | 77.0% |
| 16 | 490270 | 30.055 | 10.554 | 479.161 | 16.31 | 52.2% | 73.9% |

## Ablation (one technique off at a time)

| Configuration | Depth | Mean nodes | Nodes vs all on | Mean ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---|---:|---:|---:|---:|---:|---:|---:|
| All on | 10 | 17813 | 1.0x | 1.155 | 15.42 | 52.9% | 80.3% |
| No alpha-beta | 10 | 1967171 | 110.4x | 152.241 | 12.92 | 61.7% | n/a |
| No transposition table | 10 | 64754 | 3.6x | 3.782 | 17.12 | n/a | 70.5% |
| No move ordering | 10 | 235149 | 13.2x | 17.782 | 13.22 | 57.5% | 32.5% |
| No iterative deepening | 10 | 15768 | 0.9x | 0.951 | 16.57 | 26.0% | 65.0% |
| All on | 7 | 2483 | 1.0x | 0.183 | 13.56 | 49.6% | 82.6% |
| All off (pure minimax) | 7 | 276450 | 111.4x | 24.513 | 11.28 | n/a | n/a |

## Difficulty levels (chooseMove)

| Level | Mean depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s |
|---|---:|---:|---:|---:|---:|---:|
| Easy | 1.0 | 7 | 0.005 | 0.002 | 0.228 | n/a |
| Medium | 3.9 | 421 | 0.033 | 0.017 | 0.145 | 12.83 |
| Hard | 7.0 | 4936 | 0.342 | 0.277 | 1.937 | 14.44 |
| Impossible | 16.5 | 8577721 | 496.727 | 376.359 | 1001.111 | 17.27 |

