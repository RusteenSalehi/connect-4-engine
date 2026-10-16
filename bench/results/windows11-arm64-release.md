# c4bench results

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
