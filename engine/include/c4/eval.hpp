#pragma once

#include <array>

#include "c4/position.hpp"
#include "c4/score.hpp"

namespace c4 {

// Every evaluation weight in one place, so a later phase can tune them without touching code.
// Each weight applies to both players with opposite signs (own counts add, opponent counts
// subtract). Using one weight per feature keeps evalFor antisymmetric, which negamax relies on:
// the score of a position for one player is exactly the negation of the score for the other.
struct EvalWeights {
    int threeOpen = 5;     // per window with 3 stones of one player and 1 empty cell
    int twoOpen = 2;       // per window with 2 stones of one player and 2 empty cells
    int centerStone = 3;   // per stone in the center column
};

inline constexpr int WINDOW_COUNT = 69;

namespace detail {

constexpr std::array<Bitboard, WINDOW_COUNT> makeWindows() {
    std::array<Bitboard, WINDOW_COUNT> windows{};
    int n = 0;
    // Direction s steps by (dCol[s], dRow[s]): up, right, up-right, down-right. Plain indexed
    // arrays because MSVC 19.44 rejected a range-for over a local 2D array during constant
    // evaluation (error C2131, "expression did not evaluate to a constant").
    const int dCol[4] = {0, 1, 1, 1};
    const int dRow[4] = {1, 0, 1, -1};
    for (int s = 0; s < 4; ++s) {
        for (int col = 0; col < WIDTH; ++col) {
            for (int row = 0; row < HEIGHT; ++row) {
                const int endCol = col + 3 * dCol[s];
                const int endRow = row + 3 * dRow[s];
                // Keep only windows whose four cells are all on the board.
                if (endCol >= WIDTH || endRow < 0 || endRow >= HEIGHT) {
                    continue;
                }
                Bitboard w = 0;
                for (int k = 0; k < 4; ++k) {
                    w |= Bitboard{1} << bitIndex(col + k * dCol[s], row + k * dRow[s]);
                }
                windows[n++] = w;
            }
        }
    }
    return windows;
}

}  // namespace detail

// All 69 four-cell windows: 21 vertical + 24 horizontal + 12 + 12 diagonal. Computed once, at
// compile time, so evaluation is a loop over a constant table.
inline constexpr std::array<Bitboard, WINDOW_COUNT> WINDOWS = detail::makeWindows();

// Scores the stones `own` against `opp` (mask = own | opp). Positive is good for `own`.
// A test checks that the weights keep every possible result below the mate range.
int evalFor(Bitboard own, Bitboard opp, Bitboard mask, const EvalWeights& weights = {});

// Heuristic score of a non-terminal position, from the side to move's point of view.
inline int evaluate(const Position& pos, const EvalWeights& weights = {}) {
    return evalFor(pos.current(), pos.opponent(), pos.mask(), weights);
}

}  // namespace c4
