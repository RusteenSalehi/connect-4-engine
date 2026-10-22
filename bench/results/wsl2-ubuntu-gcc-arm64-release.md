# c4bench: WSL2 Ubuntu 26.04, gcc 15.2, Release, Snapdragon X Elite (X1E80100), AC power

Native `build/linux-release/c4bench` with default flags, recorded 2026-10-21 on a Dell XPS 13 9345
plugged in (AC). Third of three back-to-back runs; it is the median run at depth 12 (mean ms
4.688, 4.410 and 4.568 across the three runs, node counts identical). Run 1's difficulty table was
an outlier (Hard 1.608 ms mean vs 0.421 and 0.426 in runs 2 and 3) and is not used.

100 random positions of 4 to 16 plies (fixed seed), one search per position, TT of 2^20 entries cleared before every search. Times are per search, i.e. per move.

## Time per move by depth (all techniques on)

| Depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 2 | 26 | 0.005 | 0.005 | 0.013 | n/a | 12.4% | 87.0% |
| 4 | 213 | 0.024 | 0.025 | 0.079 | 9.01 | 36.1% | 82.6% |
| 6 | 1153 | 0.109 | 0.107 | 0.378 | 10.62 | 45.6% | 81.6% |
| 8 | 4897 | 0.421 | 0.363 | 1.778 | 11.64 | 50.9% | 81.6% |
| 10 | 17813 | 1.460 | 1.194 | 10.181 | 12.20 | 52.9% | 80.3% |
| 12 | 56312 | 4.568 | 2.939 | 41.966 | 12.33 | 54.7% | 79.1% |
| 14 | 167655 | 13.046 | 7.033 | 146.296 | 12.85 | 54.3% | 77.0% |
| 16 | 490270 | 38.539 | 13.067 | 577.647 | 12.72 | 52.2% | 73.9% |

## Ablation (one technique off at a time)

| Configuration | Depth | Mean nodes | Nodes vs all on | Mean ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---|---:|---:|---:|---:|---:|---:|---:|
| All on | 10 | 17813 | 1.0x | 1.430 | 12.45 | 52.9% | 80.3% |
| No alpha-beta | 10 | 1967171 | 110.4x | 185.905 | 10.58 | 61.7% | n/a |
| No transposition table | 10 | 64754 | 3.6x | 4.654 | 13.91 | n/a | 70.5% |
| No move ordering | 10 | 235149 | 13.2x | 22.291 | 10.55 | 57.5% | 32.5% |
| No iterative deepening | 10 | 15768 | 0.9x | 1.221 | 12.92 | 26.0% | 65.0% |
| All on | 7 | 2483 | 1.0x | 0.226 | 10.98 | 49.6% | 82.6% |
| All off (pure minimax) | 7 | 276450 | 111.4x | 29.957 | 9.23 | n/a | n/a |

## Difficulty levels (chooseMove)

| Level | Mean depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s |
|---|---:|---:|---:|---:|---:|---:|
| Easy | 1.0 | 7 | 0.002 | 0.002 | 0.022 | n/a |
| Medium | 3.9 | 421 | 0.042 | 0.021 | 0.209 | 10.10 |
| Hard | 7.0 | 4936 | 0.426 | 0.360 | 2.275 | 11.58 |
| Impossible | 16.4 | 7482804 | 497.684 | 378.176 | 1001.114 | 15.04 |

