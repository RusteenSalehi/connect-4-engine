#include "c4/difficulty.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <vector>

#include "c4/score.hpp"
#include "c4/zobrist.hpp"

namespace c4 {

namespace {

using Clock = std::chrono::steady_clock;

double elapsedMs(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void addStats(SearchStats& total, const SearchStats& s) {
    total.nodes += s.nodes;
    total.ttProbes += s.ttProbes;
    total.ttHits += s.ttHits;
    total.betaCutoffs += s.betaCutoffs;
    total.firstMoveCutoffs += s.firstMoveCutoffs;
}

// Full columns score -INF_SCORE, so they never win the maximum.
int bestScore(const RootScores& r) { return *std::max_element(r.scores.begin(), r.scores.end()); }

// A uniform double in [0, 1): the top 53 bits of one draw, as many as a double holds.
double uniform01(SplitMix64& rng) { return static_cast<double>(rng.next() >> 11) * 0x1.0p-53; }

// Picks each legal move with probability proportional to exp((score - best) / temperature).
int pickSoftmax(const Position& pos, const RootScores& r, SplitMix64& rng) {
    const int best = bestScore(r);
    std::array<double, WIDTH> weight{};
    double total = 0;
    for (int col = 0; col < WIDTH; ++col) {
        if (pos.canPlay(col)) {
            weight[col] = std::exp((r.scores[col] - best) / EASY_TEMPERATURE);
            total += weight[col];
        }
    }
    double x = uniform01(rng) * total;
    int last = -1;
    for (int col = 0; col < WIDTH; ++col) {
        if (!pos.canPlay(col)) continue;
        if (x < weight[col]) return col;
        x -= weight[col];
        last = col;
    }
    return last;  // rounding can leave x a hair above the last weight
}

// Picks uniformly among the legal moves scoring at least best - margin.
int pickWithinMargin(const Position& pos, const RootScores& r, int margin, SplitMix64& rng) {
    const int best = bestScore(r);
    std::vector<int> candidates;
    for (int col = 0; col < WIDTH; ++col) {
        if (pos.canPlay(col) && r.scores[col] >= best - margin) candidates.push_back(col);
    }
    return candidates[rng.below(static_cast<int>(candidates.size()))];
}

int pickBestTowardCenter(const Position& pos, const RootScores& r) {
    const int best = bestScore(r);
    for (int col : CENTER_OUT) {
        if (pos.canPlay(col) && r.scores[col] == best) return col;
    }
    return -1;  // unreachable: some legal move has the best score
}

}  // namespace

MoveChoice chooseMove(const Position& pos, Difficulty difficulty, std::uint64_t seed,
                      TranspositionTable& tt) {
    const auto start = Clock::now();
    SplitMix64 rng(seed);
    const int emptyCells = MAX_MOVES - pos.moves();
    MoveChoice choice;
    RootScores r;

    switch (difficulty) {
        case Difficulty::Easy:
            choice.depth = std::min(EASY_DEPTH, emptyCells);
            r = rootScores(pos, choice.depth, tt);
            choice.move = pickSoftmax(pos, r, rng);
            break;
        case Difficulty::Medium:
            choice.depth = std::min(
                MEDIUM_MIN_DEPTH + rng.below(MEDIUM_MAX_DEPTH - MEDIUM_MIN_DEPTH + 1), emptyCells);
            r = rootScores(pos, choice.depth, tt);
            choice.move = pickWithinMargin(pos, r, MEDIUM_MARGIN, rng);
            break;
        case Difficulty::Hard:
            choice.depth = std::min(HARD_DEPTH, emptyCells);
            r = rootScores(pos, choice.depth, tt);
            choice.move = pickWithinMargin(pos, r, 0, rng);  // a random one of the best moves
            break;
        case Difficulty::Impossible:
            // Deepen one ply at a time, like search(), so each iteration's TT entries order the
            // next. Up to the minimum depth nothing may stop it; after that, stop when the budget
            // runs out (the unfinished iteration is discarded) or the result is proven.
            for (int depth = 1; depth <= emptyCells; ++depth) {
                const bool required = depth <= IMPOSSIBLE_MIN_DEPTH;
                const int remainingMs = IMPOSSIBLE_BUDGET_MS - static_cast<int>(elapsedMs(start));
                if (!required && remainingMs <= 0) break;
                const RootScores next = rootScores(pos, depth, tt, required ? 0 : remainingMs);
                addStats(choice.stats, next.stats);
                if (!next.complete) break;
                r = next;
                choice.depth = depth;
                if (depth >= IMPOSSIBLE_MIN_DEPTH && isMateScore(bestScore(r))) break;
            }
            choice.move = pickBestTowardCenter(pos, r);
            break;
    }

    // Impossible has already summed the stats of all its iterations.
    if (difficulty != Difficulty::Impossible) choice.stats = r.stats;
    choice.score = r.scores[choice.move];
    choice.timeMs = elapsedMs(start);
    return choice;
}

}  // namespace c4
