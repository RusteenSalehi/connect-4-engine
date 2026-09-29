#include <doctest/doctest.h>

#include "c4/position.hpp"

#include <initializer_list>
#include <string>
#include <utility>

using namespace c4;

namespace {

// Builds a bitboard from (col, row) pairs, so edge-case tests read like board coordinates.
Bitboard cells(std::initializer_list<std::pair<int, int>> list) {
    Bitboard b = 0;
    for (auto [col, row] : list) {
        b |= Bitboard{1} << bitIndex(col, row);
    }
    return b;
}

bool cellSet(Bitboard b, int col, int row) {
    return col >= 0 && col < WIDTH && row >= 0 && row < HEIGHT &&
           (b & (Bitboard{1} << bitIndex(col, row))) != 0;
}

// Slow but obviously correct checker in plain (col, row) coordinates. Used as the reference
// for the bitboard version.
bool naiveFour(Bitboard b) {
    const int dirs[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    for (int col = 0; col < WIDTH; ++col) {
        for (int row = 0; row < HEIGHT; ++row) {
            for (auto& d : dirs) {
                bool all = true;
                for (int k = 0; k < 4; ++k) {
                    all = all && cellSet(b, col + k * d[0], row + k * d[1]);
                }
                if (all) {
                    return true;
                }
            }
        }
    }
    return false;
}

Position parse(const std::string& s) {
    auto p = Position::fromString(s);
    REQUIRE(p.has_value());
    return *p;
}

}  // namespace

TEST_CASE("wins are detected in every direction, including at the board edges") {
    // Vertical, bottom of the leftmost column and top of the rightmost column.
    CHECK(hasFourInARow(cells({{0, 0}, {0, 1}, {0, 2}, {0, 3}})));
    CHECK(hasFourInARow(cells({{6, 2}, {6, 3}, {6, 4}, {6, 5}})));
    // Horizontal, bottom-left corner and top-right corner.
    CHECK(hasFourInARow(cells({{0, 0}, {1, 0}, {2, 0}, {3, 0}})));
    CHECK(hasFourInARow(cells({{3, 5}, {4, 5}, {5, 5}, {6, 5}})));
    // Diagonal going up to the right, touching the bottom and right edges, and the left and top.
    CHECK(hasFourInARow(cells({{3, 0}, {4, 1}, {5, 2}, {6, 3}})));
    CHECK(hasFourInARow(cells({{0, 2}, {1, 3}, {2, 4}, {3, 5}})));
    // Diagonal going down to the right, touching the left and top edges, and the right and bottom.
    CHECK(hasFourInARow(cells({{0, 5}, {1, 4}, {2, 3}, {3, 2}})));
    CHECK(hasFourInARow(cells({{3, 3}, {4, 2}, {5, 1}, {6, 0}})));
    // Three in a row is not a win.
    CHECK_FALSE(hasFourInARow(cells({{0, 0}, {1, 0}, {2, 0}})));
}

TEST_CASE("no false wins across column boundaries") {
    // Top three cells of column 0 plus the bottom cell of column 1. Without the sentinel row
    // these would be four consecutive bits and a vertical shift would call it a win.
    CHECK_FALSE(hasFourInARow(cells({{0, 3}, {0, 4}, {0, 5}, {1, 0}})));
    CHECK_FALSE(hasFourInARow(cells({{2, 4}, {2, 5}, {3, 0}, {3, 1}})));

    // Exhaustive-style cross-check: many pseudo-random boards, bitboard vs naive checker.
    // A fixed-seed linear congruential generator keeps the test deterministic.
    Bitboard playable = 0;
    for (int col = 0; col < WIDTH; ++col) {
        playable |= columnMask(col);
    }
    std::uint64_t state = 12345;
    int wins = 0;
    for (int i = 0; i < 20000; ++i) {
        Bitboard b = 0;
        for (int k = 0; k < 2; ++k) {
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            b = (b << 32) ^ (state >> 32);
        }
        // Sparse boards (AND of two randoms, about 25 percent density) hit near-miss patterns
        // more often than dense boards, which are almost always wins.
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        b &= (state ^ (state >> 29)) & playable;
        REQUIRE(hasFourInARow(b) == naiveFour(b));
        wins += naiveFour(b) ? 1 : 0;
    }
    // Guard against a generator that only ever produces wins or only non-wins.
    CHECK(wins > 1000);
    CHECK(wins < 19000);
}

TEST_CASE("playing and isWinningMove agree on real games") {
    // First player stacks column 1 while the second player stacks column 2.
    Position p = parse("121212");
    CHECK(p.isWinningMove(0));
    CHECK_FALSE(p.isWinningMove(2));
    CHECK_FALSE(p.lastMoverWon());
    p.play(0);
    CHECK(p.lastMoverWon());

    // Horizontal win for the first player on the bottom row: 1, 2, 3, 4.
    Position h = parse("1122334");
    CHECK(h.lastMoverWon());
}

TEST_CASE("a full column rejects play") {
    Position p = parse("111111");
    CHECK_FALSE(p.canPlay(0));
    CHECK(p.canPlay(1));
    CHECK_FALSE(Position::fromString("1111111").has_value());
    // Out-of-range columns are never playable.
    CHECK_FALSE(p.canPlay(-1));
    CHECK_FALSE(p.canPlay(WIDTH));
}

TEST_CASE("draw detection at 42 plies") {
    // A full-board game with no four in a row, found by a depth-first search over legal moves.
    const std::string drawGame = "111111222222333333544444455555666666777777";
    REQUIRE(drawGame.size() == MAX_MOVES);
    Position p = parse(drawGame);
    CHECK(p.moves() == MAX_MOVES);
    CHECK(p.isDraw());
    for (int col = 0; col < WIDTH; ++col) {
        CHECK_FALSE(p.canPlay(col));
    }
    // One ply earlier the board is not full, so it is not a draw yet.
    CHECK_FALSE(parse(drawGame.substr(0, MAX_MOVES - 1)).isDraw());
}

TEST_CASE("fromString round trip") {
    const std::string moves = "4453627";
    // Parsing must give exactly the position reached by calling play() move by move.
    Position manual;
    for (char ch : moves) {
        manual.play(ch - '1');
    }
    CHECK(parse(moves) == manual);
    CHECK(parse(moves).moves() == static_cast<int>(moves.size()));

    // Parsing a prefix and playing the rest gives the same position as parsing everything.
    Position partial = parse(moves.substr(0, 3));
    for (char ch : moves.substr(3)) {
        partial.play(ch - '1');
    }
    CHECK(partial == manual);

    CHECK(parse("") == Position{});
    CHECK_FALSE(Position::fromString("08").has_value());   // digits outside 1 to 7
    CHECK_FALSE(Position::fromString("4a").has_value());   // not a digit
    CHECK_FALSE(Position::fromString("12121213").has_value());  // move after a win
}

TEST_CASE("ascii print shows absolute players") {
    std::string board = parse("45").toAscii();
    // Bottom row is the last board line before the column labels.
    CHECK(board.find(". . . X O . .\n1 2 3 4 5 6 7\n") != std::string::npos);
}

TEST_CASE("mirror reflects left to right") {
    CHECK(parse("1234").mirror() == parse("7654"));
    CHECK(parse("4").mirror() == parse("4"));  // the center column maps to itself
    Position p = parse("4453627");
    CHECK(p.mirror().mirror() == p);
    CHECK(p.mirror().moves() == p.moves());
    // A winning threat in column 1 becomes a winning threat in column 7.
    Position threat = parse("121212");
    CHECK(threat.mirror().isWinningMove(6));
}
