// The engine's WebAssembly API. Stateless by design: JavaScript passes the whole move string on
// every call, so undo and new game are just a different string. Only the TT (a cache that never
// changes a result's correctness) and the last search's stats persist between calls.
// Every number is returned as a double so JavaScript never sees a BigInt.

#include <cstdint>
#include <optional>

#include "c4/difficulty.hpp"
#include "c4/eval.hpp"
#include "c4/position.hpp"
#include "c4/tt.hpp"

using namespace c4;

namespace {

TranspositionTable tt(20);  // 2^20 entries, 16 MiB
MoveChoice last;

}  // namespace

extern "C" {

// Column 0 to 6 for the side to move, or -1 if the moves are invalid, the game is over, or the
// difficulty is not 0 (Easy) to 3 (Impossible). `seed` should be an integer from 0 to 2^53.
int c4_choose_move(const char* moves, int difficulty, double seed) {
    const std::optional<Position> pos = Position::fromString(moves);
    if (!pos || pos->lastMoverWon() || pos->moves() == MAX_MOVES || difficulty < 0 ||
        difficulty > static_cast<int>(Difficulty::Impossible)) {
        return -1;
    }
    last = chooseMove(*pos, static_cast<Difficulty>(difficulty), static_cast<std::uint64_t>(seed),
                      tt);
    return last.move;
}

double c4_last_depth() { return last.depth; }
double c4_last_score() { return last.score; }
double c4_last_nodes() { return static_cast<double>(last.stats.nodes); }
double c4_last_time_ms() { return last.timeMs; }
double c4_last_nps() { return last.timeMs > 0 ? last.stats.nodes / (last.timeMs / 1000) : 0; }

// Bitmask (bit col * 7 + row, row 0 at the bottom) of the cells that form the last mover's four
// in a row, or 0 if the game is not won or the moves are invalid. A move that completes two lines
// at once (or five in a row) marks all of their cells. At most 49 bits, so a double is exact.
double c4_winning_cells(const char* moves) {
    const std::optional<Position> pos = Position::fromString(moves);
    if (!pos || !pos->lastMoverWon()) {
        return 0;
    }
    const Bitboard stones = pos->opponent();  // the last mover's stones
    Bitboard cells = 0;
    for (Bitboard window : WINDOWS) {
        if ((stones & window) == window) cells |= window;
    }
    return static_cast<double>(cells);
}

}  // extern "C"
