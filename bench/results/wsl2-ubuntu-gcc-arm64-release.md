# c4bench results

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

