// c4cli: analyze a position or play a game against the engine in the terminal.
//
//   c4cli analyze [--moves 4453] [--depth N] [--time MS]
//   c4cli play [--engine-first] [--time MS]
//
// Moves are digits 1 to 7, one per ply, first player (X) first. Without --depth or --time the
// engine thinks for 1 second.
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "c4/position.hpp"
#include "c4/score.hpp"
#include "c4/search.hpp"
#include "c4/tt.hpp"

using namespace c4;

namespace {

constexpr int DEFAULT_TIME_MS = 1000;

struct Args {
    std::string command;
    std::string moves;
    int depth = 0;   // 0 means no depth limit
    int timeMs = 0;  // 0 means no time limit
    bool engineFirst = false;
};

void printUsage() {
    std::fprintf(stderr,
                 "usage: c4cli analyze [--moves 4453] [--depth N] [--time MS]\n"
                 "       c4cli play [--engine-first] [--time MS]\n");
}

bool parseArgs(int argc, char** argv, Args& args) {
    if (argc < 2) return false;
    args.command = argv[1];
    if (args.command != "analyze" && args.command != "play") return false;
    for (int i = 2; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (flag == "--engine-first" && args.command == "play") {
            args.engineFirst = true;
            continue;
        }
        if (i + 1 >= argc) return false;
        const char* value = argv[++i];
        if (flag == "--moves" && args.command == "analyze") {
            args.moves = value;
        } else if (flag == "--depth" && args.command == "analyze") {
            args.depth = std::atoi(value);
            if (args.depth < 1 || args.depth > MAX_MOVES) return false;
        } else if (flag == "--time") {
            args.timeMs = std::atoi(value);
            if (args.timeMs < 1) return false;
        } else {
            return false;
        }
    }
    if (args.depth == 0 && args.timeMs == 0) args.timeMs = DEFAULT_TIME_MS;
    return true;
}

// Scores are from the side to move's view. A proven result shows its distance in plies (both
// players' moves count), the rest are heuristic evaluation units.
std::string describeScore(int score) {
    if (isWinScore(score)) return "win in " + std::to_string(WIN_SCORE - score) + " plies";
    if (isLossScore(score)) return "loss in " + std::to_string(WIN_SCORE + score) + " plies";
    return (score > 0 ? "+" : "") + std::to_string(score) + " (heuristic)";
}

char sideToMove(const Position& pos) { return pos.moves() % 2 == 0 ? 'X' : 'O'; }

SearchOptions searchOptions(const Args& args) {
    SearchOptions opt;
    if (args.depth > 0) opt.maxDepth = args.depth;
    opt.timeLimitMs = args.timeMs;
    return opt;
}

bool gameOver(const Position& pos) { return pos.lastMoverWon() || pos.isDraw(); }

int analyze(const Args& args) {
    const auto pos = Position::fromString(args.moves);
    if (!pos) {
        std::fprintf(stderr, "invalid move string: %s\n", args.moves.c_str());
        return 1;
    }
    std::printf("%s", pos->toAscii().c_str());
    if (gameOver(*pos)) {
        std::printf("The game is over.\n");
        return 0;
    }
    std::printf("%c to move\n\n", sideToMove(*pos));

    TranspositionTable tt;
    const SearchResult r = search(*pos, tt, searchOptions(args));

    std::printf("depth  score   best  nodes        ms\n");
    for (const Iteration& it : r.iterations) {
        std::printf("%5d  %6d  %4d  %10llu  %8.1f\n", it.depth, it.score, it.bestMove + 1,
                    static_cast<unsigned long long>(it.nodes), it.timeMs);
    }
    std::printf("\nBest move: %d\n", r.bestMove + 1);
    std::printf("Score: %s\n", describeScore(r.score).c_str());
    std::printf("Principal variation:");
    for (int col : r.pv) std::printf(" %d", col + 1);
    std::printf("\n");
    const double mnps = r.timeMs > 0 ? r.stats.nodes / (r.timeMs * 1000.0) : 0.0;
    const double hitRate =
        r.stats.ttProbes > 0 ? 100.0 * r.stats.ttHits / r.stats.ttProbes : 0.0;
    std::printf("Nodes: %llu in %.1f ms (%.2f M nodes/s), TT hit rate %.1f%%\n",
                static_cast<unsigned long long>(r.stats.nodes), r.timeMs, mnps, hitRate);
    return 0;
}

int play(const Args& args) {
    Position pos;
    // The TT is kept for the whole game, so each search starts with what the last one learned.
    TranspositionTable tt;
    const char human = args.engineFirst ? 'O' : 'X';
    std::printf("You are %c. Enter a column 1 to 7, or q to quit.\n\n", human);

    while (!gameOver(pos)) {
        std::printf("%s\n", pos.toAscii().c_str());
        if (sideToMove(pos) == human) {
            std::printf("Your move: ");
            std::string line;
            if (!std::getline(std::cin, line) || line == "q") return 0;
            const int col = line.size() == 1 ? line[0] - '1' : -1;
            if (!pos.canPlay(col)) {
                std::printf("Not a playable column.\n");
                continue;
            }
            pos.play(col);
        } else {
            const SearchResult r = search(pos, tt, searchOptions(args));
            std::printf("Engine plays %d. Depth %d, score %s.\n\n", r.bestMove + 1, r.depthReached,
                        describeScore(r.score).c_str());
            pos.play(r.bestMove);
        }
    }

    std::printf("%s\n", pos.toAscii().c_str());
    if (pos.isDraw()) {
        std::printf("Draw.\n");
    } else {
        // The side to move is the one who did not make the winning move.
        std::printf("%s\n", sideToMove(pos) == human ? "The engine wins." : "You win.");
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Args args;
    if (!parseArgs(argc, argv, args)) {
        printUsage();
        return 1;
    }
    return args.command == "analyze" ? analyze(args) : play(args);
}
