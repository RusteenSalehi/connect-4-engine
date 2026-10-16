# c4bench results

100 random positions of 4 to 16 plies (fixed seed), one search per position, TT of 2^20 entries cleared before every search. Times are per search, i.e. per move.

## Time per move by depth (all techniques on)

| Depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 2 | 26 | 0.004 | 0.004 | 0.009 | n/a | 12.4% | 87.0% |
| 4 | 213 | 0.026 | 0.029 | 0.097 | 8.17 | 36.1% | 82.6% |
| 6 | 1153 | 0.128 | 0.123 | 0.507 | 9.03 | 45.6% | 81.6% |
| 8 | 4897 | 0.598 | 0.534 | 2.196 | 8.19 | 50.9% | 81.6% |
| 10 | 17813 | 1.979 | 1.523 | 15.396 | 9.00 | 52.9% | 80.3% |
| 12 | 56312 | 6.054 | 3.883 | 57.149 | 9.30 | 54.7% | 79.1% |
| 14 | 167655 | 17.608 | 9.102 | 190.562 | 9.52 | 54.3% | 77.0% |
| 16 | 490270 | 55.082 | 18.514 | 796.549 | 8.90 | 52.2% | 73.9% |

## Ablation (one technique off at a time)

| Configuration | Depth | Mean nodes | Nodes vs all on | Mean ms | M nodes/s | TT hit rate | First-move cutoff rate |
|---|---:|---:|---:|---:|---:|---:|---:|
| All on | 10 | 17813 | 1.0x | 1.915 | 9.30 | 52.9% | 80.3% |
| No alpha-beta | 10 | 1967171 | 110.4x | 231.127 | 8.51 | 61.7% | n/a |
| No transposition table | 10 | 64754 | 3.6x | 5.697 | 11.37 | n/a | 70.5% |
| No move ordering | 10 | 235149 | 13.2x | 27.580 | 8.53 | 57.5% | 32.5% |
| No iterative deepening | 10 | 15768 | 0.9x | 1.526 | 10.33 | 26.0% | 65.0% |
| All on | 7 | 2483 | 1.0x | 0.264 | 9.39 | 49.6% | 82.6% |
| All off (pure minimax) | 7 | 276450 | 111.4x | 34.833 | 7.94 | n/a | n/a |
