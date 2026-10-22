// For a position (Impossible to move), prints the move Impossible's rule would pick
// at each fixed depth (best exact root score, ties to the most central column) and the score of
// a given column, until a time budget runs out or the board is searched to the end.
//   c4probe <budget seconds>   stdin lines: "<moves> <column 1-7 to watch>"
//   output lines: "<moves> d <depth> pick <col> pickscore <s> watch <s> ms <cumulative>"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "c4/position.hpp"
#include "c4/score.hpp"
#include "c4/search.hpp"
#include "c4/tt.hpp"

using namespace c4;

int main(int argc, char** argv) {
    const double budgetMs = (argc > 1 ? std::atof(argv[1]) : 60) * 1000;
    TranspositionTable tt(20);
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string moves; int watch;
        in >> moves >> watch;
        const Position pos = *Position::fromString(moves);
        tt.clear();  // Impossible starts each move from an empty table in c4match
        const auto start = std::chrono::steady_clock::now();
        for (int depth = 1; depth <= MAX_MOVES - pos.moves(); ++depth) {
            const RootScores r = rootScores(pos, depth, tt);
            int pick = -1;
            for (int col : CENTER_OUT) {
                if (!pos.canPlay(col)) continue;
                if (pick < 0 || r.scores[col] > r.scores[pick]) pick = col;
            }
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            std::printf("%s d %d pick %d pickscore %d watch %d ms %.0f\n", moves.c_str(), depth, pick + 1,
                        r.scores[pick], r.scores[watch - 1], ms);
            std::fflush(stdout);
            if (ms > budgetMs) break;
        }
    }
}
