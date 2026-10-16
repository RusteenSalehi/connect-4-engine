#pragma once

// Helpers shared by several test files.

#include <doctest/doctest.h>

#include "c4/position.hpp"

#include <string>
#include <vector>

namespace c4::test {

inline Position parse(const std::string& s) {
    auto p = Position::fromString(s);
    REQUIRE(p.has_value());
    return *p;
}

// The tests' position mix: anything from the empty board to a nearly full one.
inline std::vector<Position> randomPositions(int count, std::uint64_t seed) {
    return c4::randomPositions(count, 0, MAX_MOVES - 3, seed);
}

}  // namespace c4::test
