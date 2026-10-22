# Design

How the engine works and why it is built this way. Build and usage instructions are in the
[README](README.md).

## Layout

```
engine/include/c4/   public headers        engine/src/   implementation
  position.hpp       bitboard board          position.cpp
  zobrist.hpp        hashing, splitmix64     zobrist.cpp
  score.hpp          score scale and mate range
  tt.hpp             transposition table     tt.cpp
  eval.hpp           evaluation              eval.cpp
  search.hpp         search, rootScores      search.cpp
  difficulty.hpp     difficulty levels       difficulty.cpp
tests/               doctest unit and property tests
bench/bench.cpp      c4bench, bench/results/ holds recorded runs
cli/cli.cpp          c4cli (analyze, play)
tune/tune.cpp        c4tune (evaluation tuning by engine-vs-engine matches)
match/match.cpp      c4match (difficulty levels played against each other)
analysis/            loss analysis: c4replay, c4solve (independent solver), c4probe, scripts
wasm/api.cpp         WebAssembly API (extern "C", built by the wasm preset with c4bench for Node)
web/scripts/smoke.mjs  Node smoke test for the WebAssembly module
web/src/             browser game (Vite, TypeScript)
  game.ts            move list, board derived from it, legality, undo
  engine.ts          promise-based wrapper around the worker
  engine.worker.ts   loads the WebAssembly module, answers requests
  main.ts            page: rendering, game flow, input
```

Everything engine-side is plain structs and free functions, except `Position` and
`TranspositionTable`, which are classes because each guards an invariant (the board's bitboards
and hash must stay consistent; the table's index mask needs a power-of-two size).

## WebAssembly API

`wasm/api.cpp` is the only file the web build adds: nine `extern "C"` functions over
`chooseMove`. It is stateless on purpose. JavaScript passes the full move string on every call,
so undo, new game and "analyze this position" are just different strings, and the engine cannot
drift out of sync with the page. Two things persist between calls: the 2^20-entry TT (a cache,
which only saves work and never changes a result) and the last search's stats, read by the
getters right after a move. Every return value is a `double`: node counts and the 49-bit
winning-cell mask fit exactly, and JavaScript never has to handle BigInt. Invalid input (bad move
string, finished game, unknown difficulty) returns -1 from `c4_choose_move` rather than trapping.

Memory is fixed at 32 MB with no growth: the TT takes 16 MiB, and nothing else allocates more
than a few KB. Without growth, the module's memory views never get replaced, which is one less
thing for the UI to handle.

## Web app

The page keeps one piece of game state, the list of columns played; the board, whose turn it is
and which columns are open are derived from it (`game.ts`), matching the stateless WASM API.
Wins are not checked in TypeScript: the page asks the engine (`c4_winning_cells`) after every
move, so "four in a row" has one definition. The engine runs in a Web Worker (`engine.worker.ts`)
behind a promise wrapper (`engine.ts`), because Impossible searches for a second and would
otherwise freeze the page. While a request is out, input is ignored and the columns are marked
`aria-disabled` (not `disabled`, which would drop keyboard focus). The worker hands Emscripten
the `.wasm` URL Vite assigns (`?url` import), so the file is found under the `/connect-4-engine/`
base in dev and in the hashed production build. Engine replies are shown no sooner than 300 ms
after the request, for display only; the stats show the engine's own time. The mate threshold
for the score text comes from the engine at runtime (`c4_win_score`, `c4_min_win_score`, sent
with every engine move), so the page holds no copy of the engine's score scale.

## Board representation

The board is two 64-bit bitboards: `current` (stones of the side to move) and `mask` (all
stones). Each column uses 7 bits: 6 cells plus an empty sentinel bit on top, so bit
`col * 7 + row` is cell (col, row).

- **Win detection** is four shift-and-AND steps, one per direction (1 = vertical, 7 = horizontal,
  6 and 8 = diagonals). The sentinel bit is never set, so a shift that walks off the top of a
  column reads a 0 instead of wrapping into the next column.
- **Playing a move** is `mask + bottom_of_column`, masked to the column: the carry lands on the
  lowest empty cell. Switching sides is `current ^= mask` before adding the stone.
- **Copy-make.** `Position` is 32 bytes and trivially copyable, so the search copies it for each
  child instead of undoing moves. There is no undo code to get wrong.

## Zobrist hashing vs a perfect key

A Connect-4 position fits exactly in one 64-bit number: `current + mask` is unique per position
(the sum marks each column's height with one extra bit), so it could key the transposition table
with zero collisions.
The engine still uses Zobrist hashing:

- **Its bits are evenly mixed.** The perfect key is cheap to compute too, but its bits are board
  cells: the low 20 bits describe only columns 1 to 3, so indexing a 2^20 table by them would put
  every position that agrees on those columns in the same slot. Zobrist bits are uniformly
  random, so `hash & (size - 1)` spreads positions evenly. It is also incremental: `play()` XORs
  one precomputed key into the hash.
- **It is the standard technique** in game engines (chess, go, shogi) and the one this project
  sets out to demonstrate.
- **Collisions are negligible.** Two different positions share a 64-bit hash with probability
  about 2^-64 per pair. By the birthday bound, collisions become likely only after about 2^32
  (4 billion) positions are stored together, far beyond a 2^20-entry table. The TT stores the
  full hash and compares it on every probe, so slot sharing (which is constant) never returns
  another position's data.

The keys are 2 x 49 numbers from a fixed-seed splitmix64 generator, built at compile time: one
key per player per bit index. There is no side-to-move key, because in Connect-4 the side to
move follows from the number of stones, which the hash already encodes. An earlier version also
exposed the perfect key as `Position::key()` for a collision-checking debug build; nothing else
used it, so the simplification audit removed both.

## Transposition table

2^20 entries of 16 bytes (16 MiB), always-replace, indexed by the hash's low bits.

| Field | Type | Contents |
|---|---|---|
| key | uint64 | full Zobrist hash, to verify the slot holds this position |
| score | int16 | score, with mate scores stored node-relative (below) |
| depth | uint8 | remaining depth the score was searched to |
| flag | uint8 | NONE (empty), EXACT, LOWER, UPPER |
| bestMove | uint8 | column, or 255 for none |

Four entries fill one 64-byte cache line, so a probe touches one line.

- **Bound flags.** With alpha-beta, a node's score is exact only if it lands inside the
  (alpha, beta) window. A fail-high stores a LOWER bound, a fail-low an UPPER bound. A probe may
  replace the search only if the stored depth is at least the needed depth and the flag proves
  the result for the current window (`usableScore` in `tt.cpp`).
- **NONE flag.** The empty board hashes to 0, the same as a zeroed slot's key. The NONE flag is
  what keeps an empty slot from looking like data for the empty board.
- **Mate scores.** A win on ply p from the search root scores `10000 - p`. Stored as is, that
  distance would be wrong when the same position is reached at another ply, in this search or
  the next. So `store` adds the node's ply (making the score "win in k plies from this node")
  and `probe` subtracts the probing node's ply.
- **Always-replace** is the simplest policy. Its cost shows in the PV (below); depth-preferred
  replacement is the obvious next step if that matters.

## Search

`search()` runs iterative deepening over a negamax function. Each node, in order:

1. Count the node; every 4096 nodes (after the first iteration), check the time limit.
2. If the side to move has a winning move, return `10000 - (ply + 1)`. Checking this first means
   no child is ever a finished game, and a win is seen one ply past the depth limit.
3. If the board is full, return 0 (draw). At depth 0, return the evaluation.
4. Probe the TT. Below the root, return the stored score if it is usable; remember the stored
   best move either way.
5. Search the children in order: the TT move first, then center-out (4, 3, 5, 2, 6, 1, 7). Center
   columns belong to more four-in-a-row lines, so they are more often best.
6. Store the result with its bound flag.

**Alpha-beta is fail-soft**: a node returns its best child score even outside the window, which
gives tighter TT bounds. The root never takes a TT cutoff, so it always reports a searched move.

**Iterative deepening** searches depth 1, 2, 3, ... Each iteration leaves best moves in the TT
that order the next one, and a finished iteration is always available when time runs out. The
first iteration always completes, so there is always a move. A proven win or loss stops the
deepening, since more depth cannot change it.

**PV** comes from following TT best moves from the root, checking each for legality, and ends
with the winning move when the line is a proven win.

**Toggles.** `SearchOptions` has `useAlphaBeta`, `useTT`, `useMoveOrdering` and
`useIterativeDeepening`, so the benchmark can turn each technique off and the tests can prove
none of them changes the result. With alpha-beta off the window stays (-inf, +inf), which is
plain minimax. Move ordering off means columns 1 to 7 in order and no TT move.

**Stats**: nodes, TT probes and hits (differences of the TT's own counters), beta cutoffs, and
cutoffs by the first move tried. The share of first-move cutoffs measures ordering quality.

## Difficulty levels

`chooseMove(pos, difficulty, seed, tt)` picks a move at one of four levels, so the logic lives in
the engine, in one tested place, rather than in each user interface. Every number is a named
constant in `difficulty.hpp`.

**Exact root scores.** Alpha-beta proves only that a losing move is "no better than the best", so
`search()` cannot tell a tie from a blunder. `rootScores(pos, depth, tt)` searches each legal
root move with a full window (sharing the TT), which makes every move's score exact. All levels
choose from these scores. A test checks them against plain minimax, and another checks that
their maximum equals `search()`'s score.

| Level | Depth (plies) | Choice among root scores |
|---|---|---|
| Easy | 1 | Softmax: weight exp((score - best) / 4). A move 4 points worse is picked 0.37 times as often as the best. Wins and losses in one are thousands of points away, so Easy always takes a win in one and avoids a loss in one. |
| Medium | 3 to 5, uniform per move | Uniform among moves at most 3 points below the best (one center stone with the default weights). |
| Hard | 7 | Uniform among the best moves (Medium's rule with a margin of 0). |
| Impossible | 10, then deeper | Deepens one ply at a time like `search()`. Up to depth 10 nothing stops it; after that it stops when a 1000 ms budget runs out (the unfinished depth is discarded) or the result is a proven win or loss. Best move, ties to the most central column. |

**Randomness** comes only from a splitmix64 generator seeded by the caller, so the same position,
level, seed and TT contents always give the same move. Impossible also depends on how deep it
gets in its time. The CLI seeds each game from the clock and adds the move number per move; the
bench and tests use fixed seeds.

**Depth caps.** A level never searches deeper than the number of empty cells, since there is
nothing left to search. Impossible can therefore end below its 1 second budget by searching
to the end of the game; in the benchmark that happened in 2 of 100 positions (a proven draw).

**Not clearing the TT.** Like `search()`, `chooseMove` leaves the TT to the caller: the CLI keeps
one per game, tests and the bench clear it before each move.

## Evaluation

At depth 0 the score is a weighted count over the 69 four-cell windows of the board, plus a
center-column term. All weights are in one struct, `EvalWeights`:

| Weight | Default | Counts |
|---|---|---|
| threeOpen | 5 | windows with 3 of a player's stones and 1 empty cell |
| twoOpen | 2 | windows with 2 stones and 2 empty cells |
| centerStone | 3 | stones in the center column |

Each weight applies to both players with opposite signs, so the score of a position for one side
is exactly the negation of its score for the other, which negamax relies on. A test computes the
largest possible score from the weights (363 with the defaults) and checks it stays below the
mate range (9958 and up), so no heuristic score can be mistaken for a proven result. The
defaults started as a common textbook choice; the tuning below tested them and found no
improvement. The evaluation also ignores which threats can actually be completed first
(odd/even threat parity), which matters a lot in Connect-4.

## Evaluation tuning

`c4tune` tunes the weights by playing the engine against itself.

**Match.** Two weight sets play all 49 two-ply openings (`twoPlyOpenings()` in `position.hpp`,
shared with `c4match`: every first move times every reply),
each opening twice with colors swapped, at a fixed search depth (4 for tuning). Win 1, draw 0.5,
loss 0, out of 98. Each move is a fresh search with an empty TT, so the two players never read
each other's scores. The engine has no randomness, so a match between the same two weight sets
always gives the same result, on any machine. A match of the defaults against themselves must
score exactly 50%, and the tool checks this first.

**Hill climbing.** Each round changes one weight by +1 or -1 (cycling threeOpen, twoOpen,
centerStone, plus then minus), plays the candidate against the current best, and accepts it only
if it scores above 55% (54 of 98 points). The margin keeps a candidate from being accepted
because it happens to suit a few openings. 60 rounds.

**Validation.** The final weights play the original defaults at the tuning depth and again at
depth 6, which the tuner never used, to check that a gain is not tied to depth 4.

**Result: no improvement.** No candidate reached 55%, so the defaults are unchanged.

| Candidate (vs defaults, depth 4) | W / D / L | Score |
|---|---|---|
| threeOpen +1 (6) | 38 / 15 / 45 | 46.4% |
| threeOpen -1 (4) | 39 / 13 / 46 | 46.4% |
| twoOpen +1 (3) | 41 / 11 / 46 | 47.4% |
| twoOpen -1 (1) | 37 / 11 / 50 | 43.4% |
| centerStone +1 (4) | 46 / 11 / 41 | 52.6% |
| centerStone -1 (2) | 34 / 18 / 46 | 43.9% |

| Final vs original | W / D / L | Score |
|---|---|---|
| At the tuning depth (4) | 41 / 16 / 41 | 50.0% |
| At the validation depth (6) | 42 / 14 / 42 | 50.0% |

Since no change was accepted, the final weights are the defaults, and both "final vs original"
matches are the defaults playing themselves: exactly 50%, as the color swap guarantees.

What this does and does not show:

- Five of the six neighbours lose clearly to the defaults (43 to 47%), and the sixth
  (centerStone 4) wins only 52.6%. Under this criterion the defaults are a local optimum.
- Because the matches are deterministic and the candidates repeat in a fixed cycle, once one
  full pass of six is rejected, every later round replays a match already played. In practice
  this run measured six distinct candidates; rounds 7 to 60 repeat them.
- It does not show the defaults are globally best. Hill climbing with step 1 only looks at
  immediate neighbours, and 98 games from one fixed opening set is a small sample. A larger step,
  a larger opening set (for example the 343 three-ply openings), or tuning new features such as
  threat parity could still find something. I have not run those, because choosing settings
  until something passes would make the result meaningless.

## Strength match

`c4match` checks that each difficulty level is actually stronger than the one below it. Each
pairing (Medium vs Easy, Hard vs Medium, Impossible vs Hard) plays the 49 two-ply openings twice
with colors swapped, the same opening set as `c4tune`. Move m of game g uses seed
`SEED + 64 g + m`, and every move starts from an empty TT, so the two sides never share search
results and Easy, Medium and Hard games replay exactly. Impossible stops on a time budget, so its
games depend on the machine's speed.

| Pairing | W / D / L (stronger side) | Score |
|---|---|---|
| Medium vs Easy | 87 / 2 / 9 | 89.8% |
| Hard vs Medium | 86 / 6 / 6 | 90.8% |
| Impossible vs Hard | 73 / 7 / 18 | 78.1% |

Every step up wins clearly. Impossible still loses 18 of 98 games to Hard, so it is not a perfect
player: it searches about 16 plies, not to the end of the game, and its evaluation ignores threat
parity. In a replay of the pairing (15 losses), an independent solver showed that 5 losses came
from openings already lost for Impossible, and 10 from a losing move at plies 2 to 8 that only a
search of 16 to 29 plies or deeper would have avoided.

## Reproducing the loss analysis

The tools in `analysis/` produce the numbers above. They are not built by default and not part
of the wasm build.

- `c4replay` replays `c4match`'s Impossible vs Hard pairing (same seeds, same per-move TT
  clearing) and logs every move with Impossible's depth and score.
- `c4solve` is a weak solver (win, draw or loss for the side to move), written without the
  engine's code, after Pascal Pons' solver design. It proves early positions a 1 s engine
  search cannot.
- `classify.py` marks each loss as (a) lost from the start, if every position Impossible moved
  from was lost, or (b) a mistake, at the first move from a won or drawn position into a lost one.
- `c4probe` computes exact root scores at depth 1, 2, 3, ... at each mistake, picking a move by
  Impossible's rule, and `depths.py` reports the depth from which that pick is always non-losing.

From the repository root, writing into the ignored `build/analysis/`:

```
cmake --preset linux-release
cmake --build --preset linux-release --target c4replay c4solve c4probe
mkdir -p build/analysis
build/linux-release/c4replay > build/analysis/replay.txt
awk '$1=="game" && $8=="loss" {for (i = 2; i <= length($10); i++) print substr($10, 1, i)}' \
  build/analysis/replay.txt | sort -u > build/analysis/positions.txt
build/linux-release/c4solve 120 < build/analysis/positions.txt > build/analysis/solved.txt
python3 analysis/classify.py build/analysis
build/linux-release/c4solve 120 < build/analysis/alts.in > build/analysis/alts.txt
build/linux-release/c4probe 300 < build/analysis/probe.in > build/analysis/probe.txt
python3 analysis/depths.py build/analysis
```

The arguments are time budgets in seconds per position. On one core the replay takes about 10
minutes, the solve step about 20 and the probe step about an hour; the input files can be split
and run in parallel. A `?` in `solved.txt` means the solver ran out of time on that position.
Impossible stops on a clock, so a replay on another machine, or under load, plays somewhat
different games: the counts are reproducible in kind, not exactly.

## Testing

- **Minimax equivalence.** 60 seeded random positions, depths 1 to 6, all 16 combinations of the
  four toggles, TT cleared before every search. Each must return the plain-minimax score, and the
  reported move must be one whose minimax score equals it.
- **Mutation testing** checked that the equivalence test catches real bugs. Storing a fail-low as
  EXACT, accepting a LOWER bound at alpha instead of beta, ignoring the stored depth, and storing
  alpha instead of the best score all fail it. At depth 5 the first of these passed, which is why
  the test goes to depth 6.
- **Mate distance through a reused TT.** Mate distance bugs (wrong ply on store or probe) pass the
  equivalence test, because a cleared TT at depth 6 almost never reuses a mate entry at another
  ply. A second test searches a proven position, plays the best move, and searches again with the
  same TT, as a game would: the result must be exactly one ply closer. It catches all three mate
  distance mutations tried.
- **Difficulty levels**: the same seed gives the same move (Easy, Medium and Hard on random positions,
  Impossible on proven positions, where it stops at depth 10 whatever the clock); Hard always plays
  a move with the best score at depth 7 and breaks real ties differently across seeds; Impossible
  reaches depth 10 or more (also from a position already proven at depth 2) and plays the most
  central best move; Easy picks non-best moves but favors better ones, and always takes a win in
  one and blocks the opponent's; Medium stays in its depth range and margin.
- **Property tests**: bitboard win detection against a naive checker on 20,000 boards,
  incremental hash against a from-scratch hash over 200 games, evaluation antisymmetry and mirror
  symmetry on random positions.
- Plus unit tests for parsing, draws, TT flags and mate conversion, wins, blocks, iteration
  bookkeeping and the time limit.

## Benchmark method

`c4bench` searches 100 seeded random positions of 4 to 16 plies, one search per position, with
the TT cleared before each, so every row measures the same work from the same start. "Time per
move" is the time of one such search. The seed and TT size are constants, so runs are comparable
across machines. The ablation table turns off one technique at a time at one depth, and compares
pure minimax with the full engine at a shallower depth where minimax finishes quickly.

The same `bench.cpp` also builds with Emscripten (`build/wasm/c4bench.js`, run in Node), so the
native and WebAssembly builds can be compared on one machine. Node counts at fixed depths are
identical under MSVC, gcc and Emscripten, which shows the search is deterministic across
compilers. On the recorded ARM64 laptop the WebAssembly build took 13 to 22% less time than gcc's
native build at depths 6 to 16; the cause has not been investigated.

What the recorded results show (see the README for the numbers):

- **Alpha-beta and move ordering matter most**: without alpha-beta the engine searches about 110
  times more nodes at depth 10, without ordering about 13 times more.
- **The TT saves about 3.6 times** the nodes at depth 10, and is what makes the ordering work
  across iterations.
- **Iterative deepening costs nodes at a fixed depth** (about 13% more at depth 10). It pays for
  itself in time control, not in fixed-depth node counts.
- **Difficulty levels cost very different amounts**: Easy, Medium and Hard all take under 5 ms per
  move even in the worst case; Impossible averages about half a second, because about half of the
  positions end early with a proven win or loss (searched to depth 16.4 on average).
- **First-move cutoff rate is about 80%** with ordering on, so most cutoffs come from the first
  move tried. That leaves limited room for killer moves (see below).

## What was left out, and known limitations

- **Killer moves, JSON benchmark output, `--seed` and `--tt-size` flags, a collision-checking
  build**: cut in the simplification audit as unused or unmeasured. Killers can come back if the
  benchmark shows the first-move cutoff rate leaves enough room.
- **The PV can be short** on deep searches: always-replace overwrites the root line's entries
  while sibling subtrees are searched (complete at depth 14, a few moves at 17 and beyond). The
  best move and score are unaffected.
- **Scores alternate between odd and even depths**, because the evaluation favors whoever moved
  last at the horizon.
- **The time limit applies only with iterative deepening on**, since it is checked from the second
  iteration onward, and it can overshoot by up to 4096 nodes (well under a millisecond).
