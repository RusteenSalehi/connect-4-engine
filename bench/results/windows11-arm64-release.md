# c4bench results

100 random positions of 4 to 16 plies (fixed seed), one search per position, TT of 2^20 entries cleared before every search. Times are per search, i.e. per move.

## Time per move by depth (all techniques on)

| Depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 2 | 26 | 0.004 | 0.005 | 0.011 | n/a | 12.4% | 87.0% |
| 4 | 213 | 0.025 | 0.027 | 0.093 | 8.43 | 36.1% | 82.6% |
| 6 | 1153 | 0.124 | 0.124 | 0.535 | 9.28 | 45.6% | 81.6% |
| 8 | 4897 | 0.503 | 0.430 | 2.022 | 9.74 | 50.9% | 81.6% |
| 10 | 17813 | 1.805 | 1.489 | 12.336 | 9.87 | 52.9% | 80.3% |
| 12 | 56312 | 5.581 | 3.464 | 48.625 | 10.09 | 54.7% | 79.1% |
| 14 | 167655 | 18.034 | 9.527 | 180.393 | 9.30 | 54.3% | 77.0% |
| 16 | 490270 | 48.892 | 18.098 | 731.775 | 10.03 | 52.2% | 73.9% |

## Ablation (one technique off at a time)

| Configuration | Depth | Mean nodes | Nodes vs all on | Mean ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---|---:|---:|---:|---:|---:|---:|---:|
| All on | 10 | 17813 | 1.0x | 1.808 | 9.85 | 52.9% | 80.3% |
| No alpha-beta | 10 | 1967171 | 110.4x | 240.995 | 8.16 | 61.7% | n/a |
| No transposition table | 10 | 64754 | 3.6x | 6.299 | 10.28 | n/a | 70.5% |
| No move ordering | 10 | 235149 | 13.2x | 30.889 | 7.61 | 57.5% | 32.5% |
| No iterative deepening | 10 | 15768 | 0.9x | 2.010 | 7.84 | 26.0% | 65.0% |
| All on | 7 | 2483 | 1.0x | 0.330 | 7.52 | 49.6% | 82.6% |
| All off (pure minimax) | 7 | 276450 | 111.4x | 36.522 | 7.57 | n/a | n/a |

## Difficulty levels (chooseMove)

| Level | Mean depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s |
|---|---:|---:|---:|---:|---:|---:|
| Easy | 1.0 | 7 | 0.002 | 0.001 | 0.010 | n/a |
| Medium | 3.9 | 421 | 0.051 | 0.026 | 0.242 | 8.21 |
| Hard | 7.0 | 4936 | 0.539 | 0.464 | 2.705 | 9.16 |
| Impossible | 16.1 | 5578751 | 514.217 | 561.806 | 1001.331 | 10.85 |
