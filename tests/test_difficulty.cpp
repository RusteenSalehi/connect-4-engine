#include <doctest/doctest.h>

#include "c4/difficulty.hpp"
#include "c4/position.hpp"
#include "c4/score.hpp"
#include "c4/search.hpp"
#include "c4/tt.hpp"

#include <algorithm>
#include <set>
#include <vector>

#include "test_util.hpp"

using namespace c4;
using c4::test::parse;

namespace {

int best(const RootScores& r) { return *std::max_element(r.scores.begin(), r.scores.end()); }

int worstLegal(const Position& pos, const RootScores& r) {
    int worst = INF_SCORE;
    for (int col = 0; col < WIDTH; ++col) {
        if (pos.canPlay(col)) worst = std::min(worst, r.scores[col]);
    }
    return worst;
}

}  // namespace

TEST_CASE("the same seed gives the same move") {
    // Easy, Medium and Hard on random positions; Impossible below, where the clock cannot matter.
    TranspositionTable tt(16);
    for (Difficulty d : {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard}) {
        for (const Position& pos : test::randomPositions(30, 31)) {
            for (std::uint64_t seed = 0; seed < 5; ++seed) {
                tt.clear();
                const MoveChoice a = chooseMove(pos, d, seed, tt);
                tt.clear();
                const MoveChoice b = chooseMove(pos, d, seed, tt);
                CHECK(a.move == b.move);
                CHECK(a.depth == b.depth);
                CHECK(pos.canPlay(a.move));
            }
        }
    }

    // Impossible depends on the clock only past depth 10. When the result is already proven,
    // it stops at exactly depth 10 however fast the machine is, so its move is reproducible.
    for (const Position& pos : {parse("121212"), parse("2737")}) {
        tt.clear();
        const MoveChoice a = chooseMove(pos, Difficulty::Impossible, 7, tt);
        tt.clear();
        const MoveChoice b = chooseMove(pos, Difficulty::Impossible, 7, tt);
        CHECK(a.depth == IMPOSSIBLE_MIN_DEPTH);
        CHECK(b.depth == IMPOSSIBLE_MIN_DEPTH);
        CHECK(a.move == b.move);
    }
}

TEST_CASE("Hard always returns a best move at depth 7, breaking ties randomly") {
    TranspositionTable tt(16);
    int positionsWithTies = 0;
    int tiesBrokenDifferently = 0;
    for (const Position& pos : test::randomPositions(30, 32)) {
        tt.clear();
        const MoveChoice choice = chooseMove(pos, Difficulty::Hard, 0, tt);
        CHECK(choice.depth == std::min(HARD_DEPTH, MAX_MOVES - pos.moves()));
        tt.clear();
        const RootScores r = rootScores(pos, choice.depth, tt);
        CHECK(choice.score == best(r));
        CHECK(r.scores[choice.move] == best(r));

        if (std::count(r.scores.begin(), r.scores.end(), best(r)) > 1) {
            ++positionsWithTies;
            std::set<int> picks;
            for (std::uint64_t seed = 0; seed < 10; ++seed) {
                tt.clear();
                picks.insert(chooseMove(pos, Difficulty::Hard, seed, tt).move);
            }
            tiesBrokenDifferently += picks.size() > 1 ? 1 : 0;
        }
    }
    REQUIRE(positionsWithTies > 0);  // guard: the sample must contain ties to test
    CHECK(tiesBrokenDifferently > 0);
}

TEST_CASE("Impossible reaches depth 10 or more and returns the most central best move") {
    TranspositionTable tt(20);
    std::vector<Position> positions = randomPositions(2, 4, 16, 33);
    // A win in 3 is proven at depth 2; Impossible must still search on to depth 10.
    positions.push_back(parse("2737"));
    for (const Position& pos : positions) {
        tt.clear();
        const MoveChoice choice = chooseMove(pos, Difficulty::Impossible, 0, tt);
        CHECK(choice.depth >= IMPOSSIBLE_MIN_DEPTH);
        // Recompute the scores at the depth it reached, from an empty TT.
        tt.clear();
        const RootScores r = rootScores(pos, choice.depth, tt);
        CHECK(choice.score == best(r));
        const int mostCentralBest =
            *std::find_if(CENTER_OUT.begin(), CENTER_OUT.end(), [&](int col) {
                return pos.canPlay(col) && r.scores[col] == best(r);
            });
        CHECK(choice.move == mostCentralBest);
    }
}

TEST_CASE("Easy plays plausibly but sometimes picks a non-best move") {
    TranspositionTable tt(16);
    int bestPicks = 0;
    int worstPicks = 0;
    int nonBestPicks = 0;
    for (const Position& pos : test::randomPositions(20, 34)) {
        tt.clear();
        const RootScores r = rootScores(pos, EASY_DEPTH, tt);
        for (std::uint64_t seed = 0; seed < 50; ++seed) {
            tt.clear();
            const MoveChoice choice = chooseMove(pos, Difficulty::Easy, seed, tt);
            CHECK(choice.depth == EASY_DEPTH);
            const int score = r.scores[choice.move];
            bestPicks += score == best(r) ? 1 : 0;
            nonBestPicks += score < best(r) ? 1 : 0;
            worstPicks += score == worstLegal(pos, r) && score < best(r) ? 1 : 0;
        }
    }
    CHECK(nonBestPicks > 0);
    CHECK(bestPicks > worstPicks);  // weighted toward better moves

    // Plausible: wins and losses in one are thousands of units away from any heuristic score, so
    // Easy always takes a win in one and always blocks the opponent's.
    const Position winInOne = parse("121212");
    const Position mustBlock = parse("11223");  // X threatens column 4 on the bottom row
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        tt.clear();
        CHECK(chooseMove(winInOne, Difficulty::Easy, seed, tt).move == 0);
        tt.clear();
        CHECK(chooseMove(mustBlock, Difficulty::Easy, seed, tt).move == 3);
    }
}

TEST_CASE("Medium searches 3 to 5 plies and picks within the margin of the best") {
    TranspositionTable tt(16);
    std::set<int> depthsSeen;
    for (const Position& pos : test::randomPositions(20, 35)) {
        for (std::uint64_t seed = 0; seed < 10; ++seed) {
            tt.clear();
            const MoveChoice choice = chooseMove(pos, Difficulty::Medium, seed, tt);
            const int emptyCells = MAX_MOVES - pos.moves();
            CHECK(choice.depth >= std::min(MEDIUM_MIN_DEPTH, emptyCells));
            CHECK(choice.depth <= MEDIUM_MAX_DEPTH);
            depthsSeen.insert(choice.depth);
            tt.clear();
            const RootScores r = rootScores(pos, choice.depth, tt);
            CHECK(choice.score == r.scores[choice.move]);
            CHECK(choice.score >= best(r) - MEDIUM_MARGIN);
        }
    }
    // Every depth from 3 to 5 is used.
    for (int d = MEDIUM_MIN_DEPTH; d <= MEDIUM_MAX_DEPTH; ++d) CHECK(depthsSeen.count(d) == 1);
}
