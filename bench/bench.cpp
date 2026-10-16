// c4bench: measures the search and the difficulty levels on a fixed suite of random positions
// and prints Markdown tables.
//
//   c4bench [--depths 2,4,6] [--positions N] [--ablation-depth D]
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "c4/difficulty.hpp"
#include "c4/position.hpp"
#include "c4/search.hpp"
#include "c4/tt.hpp"

using namespace c4;

namespace {

// Fixed, so every run and every machine measures the same positions with the same table.
constexpr std::uint64_t SEED = 0xC4BE'7C40'0000'0001ULL;
constexpr int TT_SIZE_LOG2 = 20;  // 2^20 entries of 16 bytes = 16 MiB, the engine default
// Middle-game positions. Earlier ones are mostly transpositions of each other, and later ones are
// often proven wins or losses that end the search after a few plies.
constexpr int MIN_PLIES = 4;
constexpr int MAX_PLIES = 16;
// Pure minimax visits about 7^depth nodes, so it gets its own, shallower depth.
constexpr int MINIMAX_DEPTH = 7;

struct Summary {
    std::uint64_t nodes = 0;
    std::uint64_t ttProbes = 0;
    std::uint64_t ttHits = 0;
    std::uint64_t betaCutoffs = 0;
    std::uint64_t firstMoveCutoffs = 0;
    std::vector<double> timesMs;  // one search per position: the time to choose a move
    int depthSum = 0;             // difficulty table only: the depth each move was chosen at
};

void add(Summary& s, const SearchStats& stats, double timeMs) {
    s.nodes += stats.nodes;
    s.ttProbes += stats.ttProbes;
    s.ttHits += stats.ttHits;
    s.betaCutoffs += stats.betaCutoffs;
    s.firstMoveCutoffs += stats.firstMoveCutoffs;
    s.timesMs.push_back(timeMs);
}

// Searches every position with a cleared TT, so no position benefits from the one before.
Summary run(const std::vector<Position>& positions, TranspositionTable& tt,
            const SearchOptions& opt) {
    Summary s;
    for (const Position& pos : positions) {
        tt.clear();
        const SearchResult r = search(pos, tt, opt);
        add(s, r.stats, r.timeMs);
    }
    return s;
}

// The same for chooseMove. Position i uses seed SEED + i, so the random levels are reproducible.
Summary runDifficulty(const std::vector<Position>& positions, TranspositionTable& tt,
                      Difficulty difficulty) {
    Summary s;
    for (std::size_t i = 0; i < positions.size(); ++i) {
        tt.clear();
        const MoveChoice c = chooseMove(positions[i], difficulty, SEED + i, tt);
        add(s, c.stats, c.timeMs);
        s.depthSum += c.depth;
    }
    return s;
}

double totalMs(const Summary& s) {
    return std::accumulate(s.timesMs.begin(), s.timesMs.end(), 0.0);
}
double meanMs(const Summary& s) { return totalMs(s) / s.timesMs.size(); }
double maxMs(const Summary& s) { return *std::max_element(s.timesMs.begin(), s.timesMs.end()); }
double medianMs(Summary s) {
    std::sort(s.timesMs.begin(), s.timesMs.end());
    const std::size_t n = s.timesMs.size();
    return n % 2 == 1 ? s.timesMs[n / 2] : (s.timesMs[n / 2 - 1] + s.timesMs[n / 2]) / 2;
}
double meanNodes(const Summary& s) { return static_cast<double>(s.nodes) / s.timesMs.size(); }

// In millions. Below a millisecond in total the clock resolution dominates, so such rows say
// "n/a".
std::string millionNodesPerSecond(const Summary& s) {
    const double ms = totalMs(s);
    if (ms < 1.0) return "n/a";
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.2f", s.nodes / (ms * 1000.0));
    return buf;
}

std::string percent(std::uint64_t part, std::uint64_t whole) {
    if (whole == 0) return "n/a";
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.1f%%", 100.0 * part / whole);
    return buf;
}

SearchOptions toDepth(int depth) {
    SearchOptions opt;
    opt.maxDepth = depth;
    return opt;
}

void printDepthTable(const std::vector<Position>& positions, TranspositionTable& tt,
                     const std::vector<int>& depths) {
    std::printf("## Time per move by depth (all techniques on)\n\n");
    std::printf("| Depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s | TT hit rate | "
                "First-move cutoff rate |\n");
    std::printf("|---:|---:|---:|---:|---:|---:|---:|---:|\n");
    for (int depth : depths) {
        const Summary s = run(positions, tt, toDepth(depth));
        std::printf("| %d | %.0f | %.3f | %.3f | %.3f | %s | %s | %s |\n", depth, meanNodes(s),
                    meanMs(s), medianMs(s), maxMs(s), millionNodesPerSecond(s).c_str(),
                    percent(s.ttHits, s.ttProbes).c_str(),
                    percent(s.firstMoveCutoffs, s.betaCutoffs).c_str());
    }
    std::printf("\n");
}

void printAblationTable(const std::vector<Position>& positions, TranspositionTable& tt,
                        int depth) {
    SearchOptions noAlphaBeta = toDepth(depth);
    noAlphaBeta.useAlphaBeta = false;
    SearchOptions noTT = toDepth(depth);
    noTT.useTT = false;
    SearchOptions noOrdering = toDepth(depth);
    noOrdering.useMoveOrdering = false;
    SearchOptions noDeepening = toDepth(depth);
    noDeepening.useIterativeDeepening = false;
    SearchOptions minimax = toDepth(MINIMAX_DEPTH);
    minimax.useAlphaBeta = false;
    minimax.useTT = false;
    minimax.useMoveOrdering = false;
    minimax.useIterativeDeepening = false;

    struct Row {
        const char* name;
        SearchOptions opt;
        bool baseline;  // "Nodes vs all on" compares later rows with the last baseline row
    };
    const std::vector<Row> rows = {
        {"All on", toDepth(depth), true},
        {"No alpha-beta", noAlphaBeta, false},
        {"No transposition table", noTT, false},
        {"No move ordering", noOrdering, false},
        {"No iterative deepening", noDeepening, false},
        {"All on", toDepth(MINIMAX_DEPTH), true},
        {"All off (pure minimax)", minimax, false},
    };

    std::printf("## Ablation (one technique off at a time)\n\n");
    std::printf("| Configuration | Depth | Mean nodes | Nodes vs all on | Mean ms | M nodes/s | "
                "TT hit rate | First-move cutoff rate |\n");
    std::printf("|---|---:|---:|---:|---:|---:|---:|---:|\n");
    std::uint64_t baselineNodes = 0;
    for (const Row& row : rows) {
        const Summary s = run(positions, tt, row.opt);
        if (row.baseline) baselineNodes = s.nodes;
        std::printf("| %s | %d | %.0f | %.1fx | %.3f | %s | %s | %s |\n", row.name,
                    row.opt.maxDepth, meanNodes(s), static_cast<double>(s.nodes) / baselineNodes,
                    meanMs(s), millionNodesPerSecond(s).c_str(),
                    percent(s.ttHits, s.ttProbes).c_str(),
                    percent(s.firstMoveCutoffs, s.betaCutoffs).c_str());
    }
    std::printf("\n");
}

void printDifficultyTable(const std::vector<Position>& positions, TranspositionTable& tt) {
    struct Level {
        const char* name;
        Difficulty difficulty;
    };
    const Level levels[] = {
        {"Easy", Difficulty::Easy},
        {"Medium", Difficulty::Medium},
        {"Hard", Difficulty::Hard},
        {"Impossible", Difficulty::Impossible},
    };
    std::printf("## Difficulty levels (chooseMove)\n\n");
    std::printf("| Level | Mean depth | Mean nodes | Mean ms | Median ms | Max ms | M nodes/s |\n");
    std::printf("|---|---:|---:|---:|---:|---:|---:|\n");
    for (const Level& level : levels) {
        const Summary s = runDifficulty(positions, tt, level.difficulty);
        std::printf("| %s | %.1f | %.0f | %.3f | %.3f | %.3f | %s |\n", level.name,
                    static_cast<double>(s.depthSum) / positions.size(), meanNodes(s), meanMs(s),
                    medianMs(s), maxMs(s), millionNodesPerSecond(s).c_str());
    }
    std::printf("\n");
}

// Parses "2,4,6" into depths; returns false on anything that is not a positive integer list.
bool parseDepths(const char* text, std::vector<int>& depths) {
    depths.clear();
    std::string_view rest(text);
    while (!rest.empty()) {
        const std::size_t comma = rest.find(',');
        const int d = std::atoi(std::string(rest.substr(0, comma)).c_str());
        if (d < 1 || d > MAX_MOVES) return false;
        depths.push_back(d);
        rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
    }
    return !depths.empty();
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<int> depths = {2, 4, 6, 8, 10, 12, 14, 16};
    int positionCount = 100;
    int ablationDepth = 10;

    for (int i = 1; i < argc; i += 2) {
        const std::string_view flag = argv[i];
        const char* value = i + 1 < argc ? argv[i + 1] : "";
        bool ok = false;
        if (flag == "--depths") {
            ok = parseDepths(value, depths);
        } else if (flag == "--positions") {
            positionCount = std::atoi(value);
            ok = positionCount > 0;
        } else if (flag == "--ablation-depth") {
            ablationDepth = std::atoi(value);
            ok = ablationDepth >= 1 && ablationDepth <= MAX_MOVES;
        }
        if (!ok) {
            std::fprintf(stderr, "usage: c4bench [--depths 2,4,6] [--positions N] "
                                 "[--ablation-depth D]\n");
            return 1;
        }
    }

    const std::vector<Position> positions =
        randomPositions(positionCount, MIN_PLIES, MAX_PLIES, SEED);
    TranspositionTable tt(TT_SIZE_LOG2);

    std::printf("# c4bench results\n\n");
    std::printf("%d random positions of %d to %d plies (fixed seed), one search per position, "
                "TT of 2^%d entries cleared before every search. Times are per search, i.e. "
                "per move.\n\n",
                positionCount, MIN_PLIES, MAX_PLIES, TT_SIZE_LOG2);
    printDepthTable(positions, tt, depths);
    printAblationTable(positions, tt, ablationDepth);
    printDifficultyTable(positions, tt);
    return 0;
}
