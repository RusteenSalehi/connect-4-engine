#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "c4/zobrist.hpp"

namespace c4 {

using Bitboard = std::uint64_t;

inline constexpr int WIDTH = 7;
inline constexpr int HEIGHT = 6;
// Each column uses HEIGHT + 1 bits. The extra bit on top (the sentinel row) is never set, so a
// shift that walks off the top of one column lands on an empty bit instead of the bottom of the
// next column. That is what makes shift-and-AND win detection safe.
inline constexpr int COLUMN_BITS = HEIGHT + 1;
inline constexpr int MAX_MOVES = WIDTH * HEIGHT;  // 42

// zobrist.hpp cannot include this header (this header includes it), so it states the board size
// on its own. This check keeps the two in sync.
static_assert(zobrist::SQUARES == WIDTH * COLUMN_BITS);

// Bit index of (col, row), with row 0 at the bottom.
constexpr int bitIndex(int col, int row) { return col * COLUMN_BITS + row; }

// Lowest playable cell of a column.
constexpr Bitboard bottomMask(int col) { return Bitboard{1} << bitIndex(col, 0); }
// Highest playable cell of a column (just under the sentinel).
constexpr Bitboard topMask(int col) { return Bitboard{1} << bitIndex(col, HEIGHT - 1); }
// All HEIGHT playable cells of a column, sentinel excluded.
constexpr Bitboard columnMask(int col) {
    return ((Bitboard{1} << HEIGHT) - 1) << bitIndex(col, 0);
}

// True if the stones contain four in a row in any direction.
bool hasFourInARow(Bitboard stones);

// A Connect-4 position. Small and trivially copyable on purpose: the search copies it for each
// child (copy-make) instead of undoing moves.
class Position {
public:
    // Empty board, first player to move.
    Position() = default;

    // Parses a move string of digits '1' to '7', one per ply, first player first.
    // Returns nullopt on a bad character, a move into a full column, or any move after the game
    // has already been won, because such a string does not describe a reachable game.
    static std::optional<Position> fromString(std::string_view moves);

    bool canPlay(int col) const { return col >= 0 && col < WIDTH && (mask_ & topMask(col)) == 0; }

    // Drops a stone for the side to move. Precondition: canPlay(col).
    void play(int col);

    // Would playing col win for the side to move? Checked without playing it.
    // Precondition: canPlay(col).
    bool isWinningMove(int col) const;

    // Did the move that led to this position win? Useful after play() in the CLI and in tests;
    // the search never needs it because it checks isWinningMove before recursing.
    bool lastMoverWon() const { return hasFourInARow(current_ ^ mask_); }

    // Board full and nobody has four in a row.
    bool isDraw() const { return moves_ == MAX_MOVES && !lastMoverWon(); }

    // Number of stones on the board (plies played).
    int moves() const { return moves_; }

    // A perfect key: two different positions never share a key.
    // Why it is unique: in a column holding h stones, the mask bits are the value 2^h - 1 and the
    // side-to-move's bits are some value c in [0, 2^h - 1]. Their sum lies in [2^h - 1, 2^(h+1) - 2],
    // and those ranges do not overlap for different h, so the sum reveals both h and c. The
    // largest sum (h = 6) is 126, which still fits in the column's 7 bits, so no carry reaches the
    // next column. Knowing h per column gives the stone count, which gives the side to move.
    std::uint64_t key() const { return current_ + mask_; }

    // Zobrist hash, maintained incrementally by play(). Unlike key() it can collide, but its bits
    // are uniformly mixed, which makes it a good transposition table index.
    std::uint64_t hash() const { return hash_; }

    Bitboard current() const { return current_; }  // stones of the side to move
    Bitboard mask() const { return mask_; }         // all stones
    Bitboard opponent() const { return current_ ^ mask_; }

    // Stones by absolute player (first player = the one who moved first). The side to move is
    // the first player exactly when an even number of stones is on the board.
    Bitboard firstPlayerStones() const { return (moves_ % 2 == 0) ? current_ : opponent(); }
    Bitboard secondPlayerStones() const { return (moves_ % 2 == 0) ? opponent() : current_; }

    // The same position reflected left to right (column c becomes column WIDTH - 1 - c).
    Position mirror() const;

    // Multi-line ASCII board. 'X' is the first player, 'O' the second, '.' empty.
    std::string toAscii() const;

    bool operator==(const Position&) const = default;

private:
    // The bit where the next stone in col would land.
    Bitboard nextStone(int col) const { return (mask_ + bottomMask(col)) & columnMask(col); }

    Bitboard current_ = 0;
    Bitboard mask_ = 0;
    int moves_ = 0;
    std::uint64_t hash_ = 0;  // the empty board hashes to 0 (XOR of no keys)
};

}  // namespace c4
