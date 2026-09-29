#include <doctest/doctest.h>

#include "c4/position.hpp"
#include "c4/zobrist.hpp"

#include <set>
#include <vector>

using namespace c4;

namespace {

std::uint64_t scratchHash(const Position& p) {
    return zobrist::hashFromScratch(p.firstPlayerStones(), p.secondPlayerStones());
}

}  // namespace

TEST_CASE("zobrist table has distinct nonzero keys") {
    // A zero key would make a stone invisible to the hash; a duplicate would make two different
    // stones indistinguishable. Neither is realistic from splitmix64, but it is cheap to rule out.
    std::set<std::uint64_t> seen;
    for (int player = 0; player < zobrist::PLAYERS; ++player) {
        for (int bit = 0; bit < zobrist::SQUARES; ++bit) {
            std::uint64_t k = zobrist::pieceKey(player, bit);
            CHECK(k != 0);
            seen.insert(k);
        }
    }
    CHECK(seen.size() == static_cast<std::size_t>(zobrist::PLAYERS * zobrist::SQUARES));
}

TEST_CASE("incremental hash equals a from-scratch recomputation over seeded random games") {
    SplitMix64 rng(2026);
    for (int game = 0; game < 200; ++game) {
        Position p;
        REQUIRE(p.hash() == 0);
        while (p.moves() < MAX_MOVES && !p.lastMoverWon()) {
            std::vector<int> legal;
            for (int col = 0; col < WIDTH; ++col) {
                if (p.canPlay(col)) {
                    legal.push_back(col);
                }
            }
            p.play(legal[rng.below(static_cast<int>(legal.size()))]);
            REQUIRE(p.hash() == scratchHash(p));
        }
        // mirror() rebuilds its hash from scratch; it must match what play() would have produced.
        CHECK(p.mirror().hash() == scratchHash(p.mirror()));
    }
}

TEST_CASE("transpositions reach the same hash and the same key") {
    // First player plays columns 1 and 3, second player plays 2 and 4, in a different order.
    Position a = *Position::fromString("1234");
    Position b = *Position::fromString("3214");
    CHECK(a.hash() == b.hash());
    CHECK(a.key() == b.key());
    CHECK(a == b);

    // Swapping which player owns the stones is a different position and must hash differently.
    Position c = *Position::fromString("2143");
    CHECK(c.hash() != a.hash());
    CHECK(c.key() != a.key());
}

TEST_CASE("mirror hash matches playing the mirrored moves") {
    Position mirrored = Position::fromString("4453627")->mirror();
    Position played = *Position::fromString("4435261");
    CHECK(mirrored.hash() == played.hash());
    CHECK(mirrored == played);
}
