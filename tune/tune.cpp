// c4tune: tunes EvalWeights by hill climbing on engine-vs-engine matches.
//
//   c4tune [--depth D]   (search depth for the tuning matches, default 4)
//
// A match plays all 49 two-ply openings twice, colors swapped. The engine is deterministic, so a
// match between the same two weight sets always gives the same result.
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "c4/eval.hpp"
#include "c4/position.hpp"
#include "c4/search.hpp"
#include "c4/tt.hpp"

using namespace c4;

namespace {

constexpr int ROUNDS = 60;  // 10 passes over the 6 candidate changes
constexpr int STEP = 1;
// A candidate must clearly beat the current best, not just edge it, to be accepted.
constexpr double ACCEPT_SCORE = 0.55;
// Validation uses a depth the tuner never saw, to check the gain is not tied to depth 4.
constexpr int VALIDATION_DEPTH = 6;
constexpr int TT_SIZE_LOG2 = 16;  // plenty for depth 6, and quick to clear before every move

struct Weight {
    const char* name;
    int EvalWeights::*field;
};
constexpr Weight WEIGHTS[] = {
    {"threeOpen", &EvalWeights::threeOpen},
    {"twoOpen", &EvalWeights::twoOpen},
    {"centerStone", &EvalWeights::centerStone},
};

struct MatchResult {
    int wins = 0;  // all counts from the first weight set's point of view
    int draws = 0;
    int losses = 0;
    int games() const { return wins + draws + losses; }
    double score() const { return wins + 0.5 * draws; }
    double fraction() const { return score() / games(); }
};

std::vector<Position> twoPlyOpenings() {
    std::vector<Position> openings;
    for (int first = 0; first < WIDTH; ++first) {
        for (int second = 0; second < WIDTH; ++second) {
            Position p;
            p.play(first);
            p.play(second);
            openings.push_back(p);
        }
    }
    return openings;
}

// Plays one game from `pos` to the end. Returns 1, 0.5 or 0 for the side with weights `a`.
double playGame(Position pos, const EvalWeights& a, const EvalWeights& b, bool aMovesFirst,
                int depth, TranspositionTable& tt) {
    SearchOptions opt;
    opt.maxDepth = depth;
    while (!pos.lastMoverWon() && !pos.isDraw()) {
        // After a two-ply opening the first player is to move whenever the stone count is even.
        const bool aToMove = (pos.moves() % 2 == 0) == aMovesFirst;
        opt.weights = aToMove ? a : b;
        // The two players must not read each other's scores, so the table starts empty.
        tt.clear();
        pos.play(search(pos, tt, opt).bestMove);
    }
    if (pos.isDraw()) return 0.5;
    // The side to move now is the one that did not make the winning move.
    const bool aJustMoved = (pos.moves() % 2 == 1) == aMovesFirst;
    return aJustMoved ? 1.0 : 0.0;
}

MatchResult playMatch(const EvalWeights& a, const EvalWeights& b, int depth,
                      TranspositionTable& tt) {
    MatchResult result;
    for (const Position& opening : twoPlyOpenings()) {
        for (bool aMovesFirst : {true, false}) {
            const double s = playGame(opening, a, b, aMovesFirst, depth, tt);
            if (s == 1.0) ++result.wins;
            else if (s == 0.5) ++result.draws;
            else ++result.losses;
        }
    }
    return result;
}

void printWeights(const EvalWeights& w) {
    std::printf("{threeOpen %d, twoOpen %d, centerStone %d}", w.threeOpen, w.twoOpen,
                w.centerStone);
}

void printMatch(const char* label, const MatchResult& m) {
    std::printf("%s: +%d =%d -%d, %.1f / %d (%.1f%%)\n", label, m.wins, m.draws, m.losses,
                m.score(), m.games(), 100.0 * m.fraction());
}

}  // namespace

int main(int argc, char** argv) {
    int depth = 4;
    if (argc == 3 && std::string_view(argv[1]) == "--depth") {
        depth = std::atoi(argv[2]);
    }
    if ((argc != 1 && argc != 3) || depth < 1 || depth > MAX_MOVES) {
        std::fprintf(stderr, "usage: c4tune [--depth D]\n");
        return 1;
    }

    TranspositionTable tt(TT_SIZE_LOG2);
    const EvalWeights original;
    std::printf("Tuning depth %d, %d rounds, step %d, accept above %.0f%%\n", depth, ROUNDS, STEP,
                100 * ACCEPT_SCORE);
    std::printf("Start: ");
    printWeights(original);
    std::printf("\n");
    // With colors swapped, identical weights must score exactly 50%; anything else is a bug.
    printMatch("Sanity check, defaults vs defaults", playMatch(original, original, depth, tt));
    std::printf("\n");

    // Round r tries weight (r / 2) % 3, plus on even rounds and minus on odd rounds.
    EvalWeights best = original;
    int accepted = 0;
    for (int round = 0; round < ROUNDS; ++round) {
        const Weight& w = WEIGHTS[(round / 2) % 3];
        const int delta = round % 2 == 0 ? STEP : -STEP;
        EvalWeights candidate = best;
        candidate.*w.field += delta;
        const MatchResult m = playMatch(candidate, best, depth, tt);
        // Rejected rounds are printed too, so the output shows how close each candidate came.
        char label[64];
        std::snprintf(label, sizeof label, "Round %2d: %s %+d vs best", round + 1, w.name, delta);
        printMatch(label, m);
        if (m.fraction() > ACCEPT_SCORE) {
            best = candidate;
            ++accepted;
            std::printf("          accepted, new best ");
            printWeights(best);
            std::printf("\n");
        }
    }

    std::printf("\n%d of %d changes accepted. Final: ", accepted, ROUNDS);
    printWeights(best);
    std::printf("\n");
    printMatch("Final vs original at the tuning depth", playMatch(best, original, depth, tt));
    char label[64];
    std::snprintf(label, sizeof label, "Final vs original at the validation depth (%d)",
                  VALIDATION_DEPTH);
    printMatch(label, playMatch(best, original, VALIDATION_DEPTH, tt));
    return 0;
}
