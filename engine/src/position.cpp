#include "c4/position.hpp"

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
    for (int col = 0; col < WIDTH; ++col) {
        int shift = (WIDTH - 1 - 2 * col) * COLUMN_BITS;
        // Move column col to column WIDTH - 1 - col. The shift direction depends on which half
        // of the board the column is in, and C++ has no negative shift, hence the branch.
        auto moveColumn = [&](Bitboard b) {
            Bitboard bits = b & columnMask(col);
            return shift >= 0 ? bits << shift : bits >> -shift;
        };
        m.current_ |= moveColumn(current_);
        m.mask_ |= moveColumn(mask_);
    }
    m.moves_ = moves_;
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
