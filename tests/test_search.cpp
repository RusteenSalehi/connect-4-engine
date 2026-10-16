#include <doctest/doctest.h>

#include "c4/eval.hpp"
#include "c4/position.hpp"
#include "c4/score.hpp"
#include "c4/search.hpp"
#include "c4/tt.hpp"

#include <algorithm>
#include <array>

#include "test_util.hpp"

using namespace c4;
using c4::test::parse;
using c4::test::randomPositions;

namespace {

// Plain depth-limited minimax with the same scoring rules as the search, but no pruning, no TT
// and no ordering. Slow and obviously correct: the reference every search variant must match.
int minimax(const Position& pos, int depth, int ply) {
    for (int col = 0; col < WIDTH; ++col) {
        if (pos.canPlay(col) && pos.isWinningMove(col)) return WIN_SCORE - (ply + 1);
    }
    if (pos.moves() == MAX_MOVES) return 0;
    if (depth == 0) return evaluate(pos);
    int best = -INF_SCORE;
    for (int col = 0; col < WIDTH; ++col) {
        if (pos.canPlay(col)) {
            Position child = pos;
            child.play(col);
            best = std::max(best, -minimax(child, depth - 1, ply + 1));
        }
    }
    return best;
}

SearchOptions withToggles(int bits, int depth) {
    SearchOptions opt;
    opt.maxDepth = depth;
    opt.useAlphaBeta = (bits & 1) != 0;
    opt.useTT = (bits & 2) != 0;
    opt.useMoveOrdering = (bits & 4) != 0;
    opt.useIterativeDeepening = (bits & 8) != 0;
    return opt;
}

bool isLegalLine(Position pos, const std::vector<int>& moves) {
    for (int col : moves) {
        if (pos.lastMoverWon() || !pos.canPlay(col)) return false;
        pos.play(col);
    }
    return true;
}

}  // namespace

TEST_CASE("every toggle combination returns the minimax score and a minimax-best move") {
    // Small table so clearing it before every search is cheap, and so index collisions (and
    // always-replace evictions) happen often enough to be exercised.
    TranspositionTable tt(16);
    for (const Position& pos : randomPositions(60, 21)) {
        for (int depth = 1; depth <= 6; ++depth) {
            // The minimax score of each root move. Their maximum is the minimax score of pos, and
            // they also show whether the reported move really is a best one.
            std::array<int, WIDTH> moveScore{};
            moveScore.fill(-INF_SCORE);
            for (int col = 0; col < WIDTH; ++col) {
                if (!pos.canPlay(col)) continue;
                Position child = pos;
                child.play(col);
                moveScore[col] = pos.isWinningMove(col) ? WIN_SCORE - 1
                                                         : -minimax(child, depth - 1, 1);
            }
            const int expected = *std::max_element(moveScore.begin(), moveScore.end());
            for (int bits = 0; bits < 16; ++bits) {
                CAPTURE(bits);
                CAPTURE(depth);
                tt.clear();
                const SearchResult r = search(pos, tt, withToggles(bits, depth));
                CHECK(r.score == expected);
                REQUIRE(pos.canPlay(r.bestMove));
                CHECK(moveScore[r.bestMove] == expected);
            }
        }
    }
}

TEST_CASE("after the best move, a proven result is one ply closer, through a reused TT") {
    // The next search runs with the TT left over from this one, as in a real game, so it reads
    // mate scores stored one ply further from its root. That is the case the TT's mate distance
    // adjustment exists for, and shallow fresh searches barely reach it.
    TranspositionTable tt(16);
    SearchOptions opt;
    opt.maxDepth = 12;
    int checked = 0;
    for (const Position& pos : randomPositions(100, 22)) {
        tt.clear();
        const SearchResult parent = search(pos, tt, opt);
        Position child = pos;
        child.play(parent.bestMove);
        if (!isMateScore(parent.score) || child.lastMoverWon()) continue;
        // Win on ply k for the parent is loss on ply k - 1 for the child, and vice versa.
        const int expected = isWinScore(parent.score) ? -parent.score - 1 : -parent.score + 1;
        CHECK(search(child, tt, opt).score == expected);
        ++checked;
    }
    CHECK(checked >= 10);  // guard: the sample must really contain proven results
}

TEST_CASE("rootScores gives every legal root move its exact minimax score") {
    TranspositionTable tt(16);
    for (const Position& pos : randomPositions(20, 23)) {
        for (int depth = 1; depth <= 4; ++depth) {
            tt.clear();
            const RootScores r = rootScores(pos, depth, tt);
            REQUIRE(r.complete);
            for (int col = 0; col < WIDTH; ++col) {
                CAPTURE(col);
                if (!pos.canPlay(col)) {
                    CHECK(r.scores[col] == -INF_SCORE);
                    continue;
                }
                Position child = pos;
                child.play(col);
                const int expected =
                    pos.isWinningMove(col) ? WIN_SCORE - 1 : -minimax(child, depth - 1, 1);
                CHECK(r.scores[col] == expected);
            }
        }
    }
}

TEST_CASE("the best of rootScores equals the search score at the same depth") {
    TranspositionTable tt(16);
    for (const Position& pos : randomPositions(30, 24)) {
        for (int depth = 1; depth <= 8; ++depth) {
            CAPTURE(depth);
            tt.clear();
            const RootScores r = rootScores(pos, depth, tt);
            tt.clear();
            SearchOptions opt;
            opt.maxDepth = depth;
            CHECK(*std::max_element(r.scores.begin(), r.scores.end()) ==
                  search(pos, tt, opt).score);
        }
    }
}

TEST_CASE("a time limit stops rootScores and marks it incomplete") {
    TranspositionTable tt(20);
    CHECK_FALSE(rootScores(Position{}, 30, tt, 20).complete);
}

TEST_CASE("an immediate win is played and scored as a win in one") {
    TranspositionTable tt(16);
    const SearchResult r = search(parse("121212"), tt);
    CHECK(r.bestMove == 0);
    CHECK(r.score == WIN_SCORE - 1);
    CHECK(r.depthReached == 1);  // a proven win stops iterative deepening at once
}

TEST_CASE("the opponent's immediate win is blocked") {
    // The first player has three on the bottom row (columns 1 to 3); only column 4 stops it.
    TranspositionTable tt(16);
    SearchOptions opt;
    opt.maxDepth = 6;
    CHECK(search(parse("11223"), tt, opt).bestMove == 3);
}

TEST_CASE("win and loss scores carry the distance, and deepening stops once proven") {
    TranspositionTable tt(16);
    SearchOptions opt;
    opt.maxDepth = 10;

    // The first player has columns 2 and 3 on the bottom row. Column 4 makes an open three that
    // cannot be blocked on both ends: a win on ply 3.
    SearchResult win = search(parse("2737"), tt, opt);
    CHECK(win.bestMove == 3);
    CHECK(win.score == WIN_SCORE - 3);
    // Depth 2 already proves it: the win check at the leaf sees the winning move on ply 3.
    CHECK(win.depthReached == 2);
    // The PV ends with the win: move 4, any reply, then the winning move.
    REQUIRE(win.pv.size() == 3);
    CHECK(win.pv.front() == 3);
    Position afterPv = parse("2737");
    CHECK(isLegalLine(afterPv, win.pv));
    for (int col : win.pv) afterPv.play(col);
    CHECK(afterPv.lastMoverWon());

    // The same position one move later, from the loser's side: lost on ply 2.
    tt.clear();
    SearchResult loss = search(parse("27374"), tt, opt);
    CHECK(loss.score == -(WIN_SCORE - 2));
    CHECK(loss.depthReached < opt.maxDepth);
    CHECK(loss.pv.size() == 2);  // a defence, then the opponent's winning move
}

TEST_CASE("each technique cuts nodes, and the counters agree with the toggles") {
    TranspositionTable tt(20);
    const Position empty;
    auto run = [&](int bits) {
        tt.clear();
        return search(empty, tt, withToggles(bits, 7));
    };
    const SearchResult minimaxOnly = run(0);
    const SearchResult alphaBetaOnly = run(1);
    const SearchResult everything = run(15);

    CHECK(minimaxOnly.score == everything.score);
    CHECK(alphaBetaOnly.stats.nodes < minimaxOnly.stats.nodes);
    CHECK(everything.stats.nodes < alphaBetaOnly.stats.nodes);

    CHECK(minimaxOnly.stats.betaCutoffs == 0);
    CHECK(alphaBetaOnly.stats.ttProbes == 0);
    CHECK(alphaBetaOnly.stats.ttHits == 0);
    CHECK(everything.stats.ttHits > 0);
    CHECK(everything.stats.ttHits <= everything.stats.ttProbes);
    CHECK(everything.stats.betaCutoffs > 0);
    CHECK(everything.stats.firstMoveCutoffs <= everything.stats.betaCutoffs);
}

TEST_CASE("iterative deepening reports every iteration and a legal principal variation") {
    TranspositionTable tt(20);
    SearchOptions opt;
    opt.maxDepth = 8;
    const SearchResult r = search(Position{}, tt, opt);
    REQUIRE(r.depthReached == 8);
    REQUIRE(r.iterations.size() == 8);
    for (int i = 0; i < 8; ++i) {
        CHECK(r.iterations[i].depth == i + 1);
        if (i > 0) CHECK(r.iterations[i].nodes >= r.iterations[i - 1].nodes);
    }
    CHECK(r.iterations.back().score == r.score);
    CHECK(r.iterations.back().nodes == r.stats.nodes);
    REQUIRE_FALSE(r.pv.empty());
    CHECK(r.pv.front() == r.bestMove);
    CHECK(static_cast<int>(r.pv.size()) <= r.depthReached + 1);  // + 1 for a final winning move
    CHECK(isLegalLine(Position{}, r.pv));
}

TEST_CASE("the search evaluates leaves with the weights from its options") {
    TranspositionTable tt(16);
    SearchOptions opt;
    opt.maxDepth = 1;
    // Depth 1 from the empty board: the best move is the center, and its score is the leaf's
    // evaluation negated (the leaf is scored for O, the root for X).
    CHECK(search(Position{}, tt, opt).score == -evaluate(parse("4")));
    opt.weights = EvalWeights{0, 0, 0};
    tt.clear();
    CHECK(search(Position{}, tt, opt).score == 0);
}

TEST_CASE("the TT carries knowledge from one search to the next") {
    TranspositionTable tt(20);
    SearchOptions opt;
    opt.maxDepth = 8;
    const SearchResult first = search(Position{}, tt, opt);
    const SearchResult second = search(Position{}, tt, opt);
    CHECK(second.score == first.score);
    CHECK(second.stats.nodes < first.stats.nodes);
}

TEST_CASE("a time limit returns the last completed iteration") {
    TranspositionTable tt(20);
    SearchOptions opt;
    opt.timeLimitMs = 100;  // the empty board cannot be solved to 42 plies in 100 ms
    const SearchResult r = search(Position{}, tt, opt);
    CHECK(r.depthReached >= 1);
    CHECK(r.depthReached < MAX_MOVES);
    CHECK(static_cast<int>(r.iterations.size()) == r.depthReached);
    CHECK(Position{}.canPlay(r.bestMove));
    // Generous bound: the clock is read every 4096 nodes, and test machines can be slow.
    CHECK(r.timeMs < 2000);
}
