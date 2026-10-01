#include "c4/eval.hpp"

#include <algorithm>
#include <bit>

namespace c4 {

namespace {

constexpr int CENTER_COLUMN = WIDTH / 2;

// The score one side gets from a single window. Only "open" windows count: if the other player
// has any stone in it, this window can never become four in a row for us.
int windowScore(int mine, int empty, const EvalWeights& w) {
    if (mine == 3 && empty == 1) return w.threeOpen;
    if (mine == 2 && empty == 2) return w.twoOpen;
    return 0;
}

}  // namespace

int evalFor(Bitboard own, Bitboard opp, Bitboard mask, const EvalWeights& weights) {
    int score = 0;
    for (Bitboard window : WINDOWS) {
        const int empty = std::popcount(window & ~mask);
        score += windowScore(std::popcount(window & own), empty, weights);
        score -= windowScore(std::popcount(window & opp), empty, weights);
    }
    const Bitboard center = columnMask(CENTER_COLUMN);
    score += weights.centerStone * (std::popcount(own & center) - std::popcount(opp & center));
    // Symmetric clamp, so antisymmetry survives even for extreme custom weights.
    return std::clamp(score, -EVAL_LIMIT, EVAL_LIMIT);
}

}  // namespace c4
