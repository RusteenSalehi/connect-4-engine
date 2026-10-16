#include "c4/search.hpp"

#include <algorithm>
#include <array>
#include <chrono>

#include "c4/score.hpp"

namespace c4 {

namespace {

using Clock = std::chrono::steady_clock;

// Center columns belong to more four-in-a-row windows, so they are more often the best move.
constexpr std::array<int, WIDTH> CENTER_OUT = {3, 2, 4, 1, 5, 0, 6};
constexpr std::array<int, WIDTH> LEFT_TO_RIGHT = {0, 1, 2, 3, 4, 5, 6};

// Reading the clock on every node would cost more than the node itself.
constexpr std::uint64_t TIME_CHECK_INTERVAL = 4096;

struct Searcher {
    TranspositionTable& tt;
    const SearchOptions& opt;
    Clock::time_point start;
    SearchStats stats;
    bool timeChecks = false;  // off during the first iteration, so there is always a move
    bool stopped = false;
    int rootBestMove = -1;

    double elapsedMs() const {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    // Negamax: the score of pos for the side to move, searched `depth` more plies. With
    // alpha-beta on, the result is exact only inside (alpha, beta); outside it is a bound.
    int negamax(const Position& pos, int depth, int ply, int alpha, int beta) {
        ++stats.nodes;
        if (timeChecks && opt.timeLimitMs > 0 && stats.nodes % TIME_CHECK_INTERVAL == 0 &&
            elapsedMs() >= opt.timeLimitMs) {
            stopped = true;
        }
        if (stopped) {
            return 0;  // discarded by the caller
        }

        // A win now is the best possible result, so no search is needed. Checking here also
        // means no child is ever a finished game, so negamax never has to detect a loss.
        for (int col = 0; col < WIDTH; ++col) {
            if (pos.canPlay(col) && pos.isWinningMove(col)) {
                if (ply == 0) rootBestMove = col;
                return WIN_SCORE - (ply + 1);
            }
        }
        if (pos.moves() == MAX_MOVES) {
            return 0;  // draw
        }
        if (depth == 0) {
            return evaluate(pos, opt.weights);
        }

        const int alphaOrig = alpha;
        int ttMove = -1;
        if (opt.useTT) {
            if (auto hit = tt.probe(pos, ply)) {
                ttMove = hit->bestMove;
                // Not at the root: the root must search its moves to report the best one.
                if (ply > 0) {
                    if (auto score = usableScore(*hit, depth, alpha, beta)) return *score;
                }
            }
        }

        std::array<int, WIDTH> order{};
        int count = 0;
        // A true 64-bit hash collision could hand back another position's move, so check it.
        const int first = opt.useMoveOrdering && ttMove >= 0 && pos.canPlay(ttMove) ? ttMove : -1;
        if (first >= 0) order[count++] = first;
        for (int col : opt.useMoveOrdering ? CENTER_OUT : LEFT_TO_RIGHT) {
            if (col != first && pos.canPlay(col)) order[count++] = col;
        }

        int best = -INF_SCORE;
        int bestMove = -1;
        for (int i = 0; i < count; ++i) {
            Position child = pos;
            child.play(order[i]);
            const int score = -negamax(child, depth - 1, ply + 1, -beta, -alpha);
            if (stopped) {
                return 0;
            }
            if (score > best) {
                best = score;
                bestMove = order[i];
            }
            // Without alpha-beta the window stays (-INF, INF), so this is plain minimax.
            if (opt.useAlphaBeta) {
                alpha = std::max(alpha, score);
                if (alpha >= beta) {
                    ++stats.betaCutoffs;
                    if (i == 0) ++stats.firstMoveCutoffs;
                    break;
                }
            }
        }

        if (opt.useTT) {
            const Bound flag = best <= alphaOrig ? Bound::UPPER
                               : best >= beta    ? Bound::LOWER
                                                 : Bound::EXACT;
            tt.store(pos, ply, depth, best, flag, bestMove);
        }
        if (ply == 0) rootBestMove = bestMove;
        return best;
    }
};

// Follows TT best moves from the root. Entries can have been overwritten, so every move is
// checked for legality and the line can come out shorter than the search depth. A winning move
// at the end can make it one move longer.
std::vector<int> principalVariation(Position pos, TranspositionTable& tt, int bestMove,
                                    int depth) {
    std::vector<int> pv{bestMove};
    pos.play(bestMove);
    for (int ply = 1; ply < depth && !pos.lastMoverWon(); ++ply) {
        auto hit = tt.probe(pos, ply);
        if (!hit || hit->bestMove < 0 || !pos.canPlay(hit->bestMove)) break;
        pv.push_back(hit->bestMove);
        pos.play(hit->bestMove);
    }
    // The search sees the final move of a won line through its win check, which stores nothing in
    // the TT, so add that move here. Then a proven line ends with the win.
    for (int col = 0; col < WIDTH && !pos.lastMoverWon(); ++col) {
        if (pos.canPlay(col) && pos.isWinningMove(col)) {
            pv.push_back(col);
            break;
        }
    }
    return pv;
}

}  // namespace

SearchResult search(const Position& pos, TranspositionTable& tt, const SearchOptions& opt) {
    Searcher s{tt, opt, Clock::now()};
    const TTStats ttBefore = tt.stats();
    SearchResult result;

    // Searching past the end of the game would only repeat the last depth.
    const int maxDepth = std::min(opt.maxDepth, MAX_MOVES - pos.moves());
    // Each iteration fills the TT with best moves that order the next, deeper one, and leaves a
    // complete answer behind when time runs out.
    for (int depth = opt.useIterativeDeepening ? 1 : maxDepth; depth <= maxDepth; ++depth) {
        const int score = s.negamax(pos, depth, 0, -INF_SCORE, INF_SCORE);
        if (s.stopped) {
            break;
        }
        result.bestMove = s.rootBestMove;
        result.score = score;
        result.depthReached = depth;
        result.iterations.push_back({depth, score, s.rootBestMove, s.stats.nodes, s.elapsedMs()});
        s.timeChecks = true;
        // A proven win or loss cannot change with more depth.
        if (isMateScore(score)) {
            break;
        }
    }

    result.stats = s.stats;
    // Differences of the TT's own counters, read before the PV walk probes the table again.
    result.stats.ttProbes = tt.stats().probes - ttBefore.probes;
    result.stats.ttHits = tt.stats().hits - ttBefore.hits;
    result.timeMs = s.elapsedMs();
    result.pv = opt.useTT ? principalVariation(pos, tt, result.bestMove, result.depthReached)
                          : std::vector<int>{result.bestMove};
    return result;
}

}  // namespace c4
