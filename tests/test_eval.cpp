#include <doctest/doctest.h>

#include "c4/eval.hpp"
#include "c4/position.hpp"
#include "c4/score.hpp"
#include "c4/zobrist.hpp"

#include <bit>
#include <set>
#include <vector>

using namespace c4;

namespace {

// Non-terminal positions from seeded random playouts, used by the property tests.
std::vector<Position> randomPositions(int count, std::uint64_t seed) {
    std::vector<Position> out;
    SplitMix64 rng(seed);
    while (static_cast<int>(out.size()) < count) {
        Position p;
        const int plies = rng.below(MAX_MOVES - 2);
        bool ok = true;
        for (int i = 0; i < plies && ok; ++i) {
            int col = rng.below(WIDTH);
            // Skip full columns and winning moves, so every prefix stays non-terminal.
            int tries = 0;
            while ((!p.canPlay(col) || p.isWinningMove(col)) && tries < WIDTH) {
                col = (col + 1) % WIDTH;
                ++tries;
            }
            if (tries == WIDTH) {
                ok = false;  // no safe move: discard this playout
            } else {
                p.play(col);
            }
        }
        if (ok) {
            out.push_back(p);
        }
    }
    return out;
}

Bitboard cell(int col, int row) { return Bitboard{1} << bitIndex(col, row); }

}  // namespace

TEST_CASE("exactly 69 distinct four-cell windows, each a real line on the board") {
    CHECK(WINDOWS.size() == 69);
    std::set<Bitboard> distinct(WINDOWS.begin(), WINDOWS.end());
    CHECK(distinct.size() == 69);

    Bitboard playable = 0;
    for (int col = 0; col < WIDTH; ++col) {
        playable |= columnMask(col);
    }
    for (Bitboard w : WINDOWS) {
        CHECK(std::popcount(w) == 4);
        CHECK((w & ~playable) == 0);   // no sentinel or out-of-board bits
        CHECK(hasFourInARow(w));       // the four cells form a line
    }
}

TEST_CASE("evalFor is antisymmetric on random positions") {
    for (const Position& p : randomPositions(500, 11)) {
        const Bitboard a = p.current();
        const Bitboard b = p.opponent();
        CHECK(evalFor(a, b, p.mask()) == -evalFor(b, a, p.mask()));
    }
}

TEST_CASE("evaluation is mirror symmetric") {
    for (const Position& p : randomPositions(500, 12)) {
        CHECK(evaluate(p) == evaluate(p.mirror()));
    }
}

TEST_CASE("evaluation stays far below the mate range") {
    for (const Position& p : randomPositions(500, 13)) {
        CHECK_FALSE(isMateScore(evaluate(p)));
        CHECK(evaluate(p) <= EVAL_LIMIT);
        CHECK(evaluate(p) >= -EVAL_LIMIT);
    }
    // Even absurd weights are clamped.
    EvalWeights huge{1000000, 1000000, 1000000};
    Bitboard own = cell(0, 0) | cell(1, 0) | cell(2, 0) | cell(3, 1);
    CHECK(evalFor(own, 0, own, huge) == EVAL_LIMIT);
    CHECK(evalFor(0, own, own, huge) == -EVAL_LIMIT);
}

TEST_CASE("adding an open three increases its owner's score") {
    // Own stones in columns 1 and 2 on the bottom row; the opponent is stacked far away in
    // column 7 so it does not touch any window we change.
    const Bitboard opp = cell(6, 0) | cell(6, 1);
    const Bitboard before = cell(0, 0) | cell(1, 0);
    const Bitboard after = before | cell(2, 0);  // now 3 own + 1 empty in columns 1 to 4
    const int scoreBefore = evalFor(before, opp, before | opp);
    const int scoreAfter = evalFor(after, opp, after | opp);
    CHECK(scoreAfter > scoreBefore);
    // From the opponent's side the same change must look worse by the same amount.
    CHECK(evalFor(opp, after, after | opp) < evalFor(opp, before, before | opp));

    // The threeOpen weight alone drives the difference when the other weights are zero.
    EvalWeights onlyThrees{1, 0, 0};
    CHECK(evalFor(before, opp, before | opp, onlyThrees) == 0);
    CHECK(evalFor(after, opp, after | opp, onlyThrees) == 1);
}

TEST_CASE("center column stones are rewarded") {
    EvalWeights onlyCenter{0, 0, 1};
    const Bitboard own = cell(3, 0) | cell(3, 1);
    const Bitboard opp = cell(3, 2);
    CHECK(evalFor(own, opp, own | opp, onlyCenter) == 1);
    CHECK(evaluate(Position{}) == 0);  // the empty board is balanced
}
