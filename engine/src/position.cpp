#include "c4/position.hpp"

#include <bit>

namespace c4 {

bool hasFourInARow(Bitboard stones) {
    // Shift amounts that step to the neighbouring cell in each direction:
    // 1 = up the same column, COLUMN_BITS = same row in the next column,
    // COLUMN_BITS - 1 = next column one row down, COLUMN_BITS + 1 = next column one row up.
    constexpr int directions[] = {1, COLUMN_BITS, COLUMN_BITS - 1, COLUMN_BITS + 1};
    for (int d : directions) {
        // pairs has a bit set wherever a stone and its neighbour in direction d are both set.
        Bitboard pairs = stones & (stones >> d);
        // Two pairs that are 2 steps apart make four in a row. Any step that crosses a column
        // edge reads a sentinel bit, which is always 0, so no false wins across columns.
        if (pairs & (pairs >> (2 * d))) {
            return true;
        }
    }
    return false;
}

std::optional<Position> Position::fromString(std::string_view moves) {
    Position pos;
    for (char ch : moves) {
        int col = ch - '1';
        if (col < 0 || col >= WIDTH || !pos.canPlay(col) || pos.lastMoverWon()) {
            return std::nullopt;
        }
        pos.play(col);
    }
    return pos;
}

void Position::play(int col) {
    Bitboard stone = nextStone(col);
    // The mover's absolute index is the parity of the stone count before the move: the first
    // player moves on plies 0, 2, 4, ... XOR-ing its key in is all the hash update needs.
    hash_ ^= zobrist::pieceKey(moves_ % 2, std::countr_zero(stone));
    // After the move it is the opponent's turn, so "current" must become the opponent's stones.
    // XOR with the old mask gives exactly those (the new stone is not in the old mask, so it
    // correctly ends up belonging to the player who just moved, now the "opponent").
    current_ ^= mask_;
    mask_ |= stone;
    ++moves_;
}

bool Position::isWinningMove(int col) const {
    return hasFourInARow(current_ | nextStone(col));
}

Position Position::mirror() const {
    Position m;
    const Bitboard lowColumn = columnMask(0);
    for (int col = 0; col < WIDTH; ++col) {
        // Bring column col down to bits 0..5, then lift it into column WIDTH - 1 - col.
        const int from = bitIndex(col, 0);
        const int to = bitIndex(WIDTH - 1 - col, 0);
        m.current_ |= ((current_ >> from) & lowColumn) << to;
        m.mask_ |= ((mask_ >> from) & lowColumn) << to;
    }
    m.moves_ = moves_;
    // Mirroring moves every stone to a new bit, so the hash is rebuilt rather than patched.
    m.hash_ = zobrist::hashFromScratch(m.firstPlayerStones(), m.secondPlayerStones());
    return m;
}

std::string Position::toAscii() const {
    Bitboard first = firstPlayerStones();
    std::string out;
    for (int row = HEIGHT - 1; row >= 0; --row) {
        for (int col = 0; col < WIDTH; ++col) {
            Bitboard cell = Bitboard{1} << bitIndex(col, row);
            char c = (mask_ & cell) == 0 ? '.' : ((first & cell) != 0 ? 'X' : 'O');
            out += c;
            out += (col + 1 < WIDTH) ? ' ' : '\n';
        }
    }
    out += "1 2 3 4 5 6 7\n";
    return out;
}

}  // namespace c4
