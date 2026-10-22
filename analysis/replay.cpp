// Replays c4match's Impossible vs Hard pairing exactly (same seeds, same procedure)
// and logs every game. Output, one line per game:
//   game <g> opening <2 digits> impossible <first|second> result <win|draw|loss> moves <digits>
//   then per move: "move <ply> <who> <col 1-7> depth <d> score <s> ms <t>"
#include <cstdint>
#include <cstdio>
#include <string>

#include "c4/difficulty.hpp"
#include "c4/position.hpp"
#include "c4/tt.hpp"

using namespace c4;

constexpr std::uint64_t SEED = 0xC4'3A7C'0000'0001ULL;  // same as match/match.cpp

int main() {
    TranspositionTable tt(20);
    std::uint64_t gameSeed = SEED;
    int g = 0;
    for (const Position& opening : twoPlyOpenings()) {
        for (bool impFirst : {true, false}) {
            Position pos = opening;
            std::string moves, log;
            // Recover the opening's two columns from the first two stones.
            for (int c = 0; c < WIDTH; ++c) {
                Position t; t.play(c);
                for (int d = 0; d < WIDTH; ++d) { Position u = t; u.play(d); if (u == opening) { moves = std::to_string(c + 1) + std::to_string(d + 1); } }
            }
            const std::string openingStr = moves;
            while (!pos.lastMoverWon() && !pos.isDraw()) {
                const bool impToMove = (pos.moves() % 2 == 0) == impFirst;
                tt.clear();
                const MoveChoice mc = chooseMove(pos, impToMove ? Difficulty::Impossible : Difficulty::Hard,
                                                 gameSeed + pos.moves(), tt);
                char buf[160];
                std::snprintf(buf, sizeof buf, "move %d %s %d depth %d score %d ms %.1f\n", pos.moves(),
                              impToMove ? "imp" : "hard", mc.move + 1, mc.depth, mc.score, mc.timeMs);
                log += buf;
                pos.play(mc.move);
                moves += std::to_string(mc.move + 1);
            }
            const char* result;
            if (pos.isDraw()) result = "draw";
            else {
                const bool impJustMoved = (pos.moves() % 2 == 1) == impFirst;
                result = impJustMoved ? "win" : "loss";
            }
            std::printf("game %d opening %s impossible %s result %s moves %s\n%s", g, openingStr.c_str(),
                        impFirst ? "first" : "second", result, moves.c_str(), log.c_str());
            std::fflush(stdout);
            gameSeed += 64;
            ++g;
        }
    }
    return 0;
}
