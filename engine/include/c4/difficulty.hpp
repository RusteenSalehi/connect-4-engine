#pragma once

#include <cstdint>

#include "c4/position.hpp"
#include "c4/search.hpp"
#include "c4/tt.hpp"

namespace c4 {

enum class Difficulty { Easy, Medium, Hard, Impossible };

// Every number that defines a level, in one place. Depths are in plies. Scores are evaluation
// units (see EvalWeights).
inline constexpr int EASY_DEPTH = 1;
// Softmax temperature: a move scoring T below the best is picked e^-1 (about 0.37) times as often
// as the best. Proven wins and losses are thousands of units apart, so Easy still takes a win in
// one and avoids losing in one.
inline constexpr double EASY_TEMPERATURE = 4.0;
inline constexpr int MEDIUM_MIN_DEPTH = 3;
inline constexpr int MEDIUM_MAX_DEPTH = 5;
// Medium picks uniformly among moves at most this far below the best (one center stone's worth
// with the default weights).
inline constexpr int MEDIUM_MARGIN = 3;
inline constexpr int HARD_DEPTH = 7;
inline constexpr int IMPOSSIBLE_MIN_DEPTH = 10;
inline constexpr int IMPOSSIBLE_BUDGET_MS = 1000;

struct MoveChoice {
    int move = -1;      // column 0 to 6
    int score = 0;      // the chosen move's exact score at `depth`
    int depth = 0;      // depth of the scores the move was chosen from
    SearchStats stats;  // summed over every search the choice needed
    double timeMs = 0;
};

// Picks a move for the side to move at the given level. All randomness comes from a splitmix64
// generator seeded with `seed`, so the same position, level, seed and TT contents give the same
// move (Impossible also depends on how deep it gets within its time budget). Like search(), it
// does not clear the TT. Fewer empty cells than a level's depth cap the depth.
// Precondition: the game is not over.
MoveChoice chooseMove(const Position& pos, Difficulty difficulty, std::uint64_t seed,
                      TranspositionTable& tt);

}  // namespace c4
