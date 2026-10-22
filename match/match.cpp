// c4match: plays the difficulty levels against each other, to show they differ in strength.
//
//   c4match   (no options; Hard vs Impossible takes about 20 minutes)
//
// Each pairing plays all 49 two-ply openings twice, colors swapped (98 games). Every move has a
// fixed seed, so Easy, Medium and Hard games replay exactly. Impossible stops on a 1 s budget, so
// how deep it gets, and therefore its games, can vary a little between runs and machines.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "c4/difficulty.hpp"
#include "c4/position.hpp"
#include "c4/tt.hpp"

using namespace c4;

namespace {

constexpr std::uint64_t SEED = 0xC4'3A7C'0000'0001ULL;
constexpr int TT_SIZE_LOG2 = 20;  // the size the web game uses

struct Level {
    const char* name;
    Difficulty difficulty;
};

struct MatchResult {
    int wins = 0;  // all counts from the stronger level's point of view
    int draws = 0;
    int losses = 0;
    int games() const { return wins + draws + losses; }
    double score() const { return wins + 0.5 * draws; }
};

// Plays one game from `pos` to the end. Returns 1, 0.5 or 0 for `a`. Move m of the game uses
// seed gameSeed + m.
double playGame(Position pos, Difficulty a, Difficulty b, bool aMovesFirst,
                std::uint64_t gameSeed, TranspositionTable& tt) {
    while (!pos.lastMoverWon() && !pos.isDraw()) {
        // After a two-ply opening the first player is to move whenever the stone count is even.
        const bool aToMove = (pos.moves() % 2 == 0) == aMovesFirst;
        // An empty table for every move, so neither side reads the other's search and a replay
        // with the same seeds gives the same game.
        tt.clear();
        pos.play(chooseMove(pos, aToMove ? a : b, gameSeed + pos.moves(), tt).move);
    }
    if (pos.isDraw()) return 0.5;
    // The side to move now is the one that did not make the winning move.
    const bool aJustMoved = (pos.moves() % 2 == 1) == aMovesFirst;
    return aJustMoved ? 1.0 : 0.0;
}

MatchResult playMatch(Difficulty stronger, Difficulty weaker, TranspositionTable& tt) {
    MatchResult result;
    std::uint64_t gameSeed = SEED;
    for (const Position& opening : twoPlyOpenings()) {
        for (bool strongerMovesFirst : {true, false}) {
            const double s = playGame(opening, stronger, weaker, strongerMovesFirst, gameSeed, tt);
            gameSeed += 64;  // more than the 42 moves a game can have, so no seed repeats
            if (s == 1.0) ++result.wins;
            else if (s == 0.5) ++result.draws;
            else ++result.losses;
        }
    }
    return result;
}

}  // namespace

int main() {
    const Level pairings[][2] = {
        {{"Medium", Difficulty::Medium}, {"Easy", Difficulty::Easy}},
        {{"Hard", Difficulty::Hard}, {"Medium", Difficulty::Medium}},
        {{"Impossible", Difficulty::Impossible}, {"Hard", Difficulty::Hard}},
    };
    TranspositionTable tt(TT_SIZE_LOG2);

    std::printf("All 49 two-ply openings, each played twice with colors swapped (98 games per "
                "pairing). Results are from the stronger level's point of view.\n\n");
    std::printf("| Pairing | Games | Wins | Draws | Losses | Score | Minutes |\n");
    std::printf("|---|---:|---:|---:|---:|---:|---:|\n");
    for (const auto& [stronger, weaker] : pairings) {
        const auto start = std::chrono::steady_clock::now();
        const MatchResult m = playMatch(stronger.difficulty, weaker.difficulty, tt);
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        std::printf("| %s vs %s | %d | %d | %d | %d | %.1f%% | %.1f |\n", stronger.name,
                    weaker.name, m.games(), m.wins, m.draws, m.losses,
                    100.0 * m.score() / m.games(), elapsed.count() / 60);
        std::fflush(stdout);  // one line per pairing, visible while the next one runs
    }
    return 0;
}
