#pragma once

#include <cstdint>
#include <vector>

#include "c4/eval.hpp"
#include "c4/position.hpp"
#include "c4/tt.hpp"

namespace c4 {

// The four toggles exist so the benchmark can measure each technique's effect (ablation) and so
// the equivalence test can prove that none of them changes the result.
struct SearchOptions {
    int maxDepth = MAX_MOVES;
    int timeLimitMs = 0;  // 0 means no limit
    bool useAlphaBeta = true;
    bool useTT = true;
    bool useMoveOrdering = true;  // TT move first, then center-out; off means columns 1 to 7
    bool useIterativeDeepening = true;
    // Evaluation weights for the leaves. c4tune plays engines with different weights against
    // each other; everything else uses the defaults.
    EvalWeights weights;
};

struct SearchStats {
    std::uint64_t nodes = 0;
    std::uint64_t ttProbes = 0;
    std::uint64_t ttHits = 0;
    std::uint64_t betaCutoffs = 0;
    // Cutoffs caused by the first move tried. Its share of betaCutoffs says how good the move
    // ordering is, which is the data needed before adding more ordering (such as killer moves).
    std::uint64_t firstMoveCutoffs = 0;
};

// One completed iteration of iterative deepening. nodes and timeMs are cumulative.
struct Iteration {
    int depth;
    int score;
    int bestMove;
    std::uint64_t nodes;
    double timeMs;
};

struct SearchResult {
    int bestMove = -1;   // column 0 to 6
    int score = 0;       // from the side to move's point of view
    int depthReached = 0;
    std::vector<int> pv;  // principal variation, starting with bestMove
    SearchStats stats;
    std::vector<Iteration> iterations;
    double timeMs = 0;
};

// Searches pos to opt.maxDepth plies (fewer if time runs out or a win or loss is proven) and
// returns the best move from the last completed iteration. The TT is not cleared, so knowledge
// carries over between searches; clear it first for a reproducible search.
// Precondition: the game is not over.
SearchResult search(const Position& pos, TranspositionTable& tt, const SearchOptions& opt = {});

}  // namespace c4
