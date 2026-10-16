#pragma once

// Helpers shared by several test files.

#include <doctest/doctest.h>

#include "c4/position.hpp"
#include "c4/zobrist.hpp"

#include <string>
#include <vector>

namespace c4::test {

inline Position parse(const std::string& s) {
    auto p = Position::fromString(s);
    REQUIRE(p.has_value());
    return *p;
}

// Non-terminal positions from seeded random playouts, used by the property tests.
inline std::vector<Position> randomPositions(int count, std::uint64_t seed) {
    std::vector<Position> out;
    SplitMix64 rng(seed);
    while (static_cast<int>(out.size()) < count) {
        Position p;
        const int plies = rng.below(MAX_MOVES - 2);
        bool ok = true;
        for (int i = 0; i < plies && ok; ++i) {
            int col = rng.below(WIDTH);
            // Skip full columns and winning moves, so every prefix stays non-terminal.
            int tries = 0;
            while ((!p.canPlay(col) || p.isWinningMove(col)) && tries < WIDTH) {
                col = (col + 1) % WIDTH;
                ++tries;
            }
            if (tries == WIDTH) {
                ok = false;  // no safe move: discard this playout
            } else {
                p.play(col);
            }
        }
        if (ok) {
            out.push_back(p);
        }
    }
    return out;
}

}  // namespace c4::test
