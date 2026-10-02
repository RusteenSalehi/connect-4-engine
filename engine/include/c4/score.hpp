#pragma once

#include "c4/position.hpp"

namespace c4 {

// All scores are from the side to move's point of view (negamax convention).
//
// A win reached at ply p from the search root scores WIN_SCORE - p, so a faster win scores
// higher and a slower loss scores higher (less negative). The engine therefore prefers to win
// quickly and to lose slowly.
inline constexpr int WIN_SCORE = 10000;

// A game lasts at most MAX_MOVES plies, so every win or loss score lies within MAX_MOVES of
// +/- WIN_SCORE. Anything at least this large in magnitude is a proven result, not a heuristic.
inline constexpr int MIN_WIN_SCORE = WIN_SCORE - MAX_MOVES;

// Larger than any real score; used as the initial alpha-beta window.
inline constexpr int INF_SCORE = WIN_SCORE + 1;

constexpr bool isWinScore(int score) { return score >= MIN_WIN_SCORE; }
constexpr bool isLossScore(int score) { return score <= -MIN_WIN_SCORE; }
constexpr bool isMateScore(int score) { return isWinScore(score) || isLossScore(score); }

}  // namespace c4
