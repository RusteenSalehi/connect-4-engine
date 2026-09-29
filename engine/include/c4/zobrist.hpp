#pragma once

#include <array>
#include <cstdint>

namespace c4 {

// splitmix64: a tiny, fast, well-mixed 64-bit generator. Used for the Zobrist table and for
// every seeded random game in tests and benchmarks. We use our own generator rather than
// <random> distributions because std::uniform_int_distribution is implementation-defined:
// MSVC and libstdc++ would produce different "random" positions from the same seed, and
// benchmark suites would not be comparable across machines.
class SplitMix64 {
public:
    explicit constexpr SplitMix64(std::uint64_t seed) : state_(seed) {}

    constexpr std::uint64_t next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    // An integer in [0, n). Plain modulo has a bias of about n / 2^64, which is irrelevant for
    // picking one of 7 columns.
    constexpr int below(int n) { return static_cast<int>(next() % static_cast<std::uint64_t>(n)); }

private:
    std::uint64_t state_;
};

namespace zobrist {

// Fixed seed so the table, and therefore every hash and TT index, is reproducible.
inline constexpr std::uint64_t SEED = 0xC4C4'2026'0000'0001ULL;

inline constexpr int PLAYERS = 2;
// One key per bit index of the board layout (7 columns x 7 bits). The 7 sentinel bits get keys
// too; they are never used, but sizing by bit index keeps the lookup a direct array access.
inline constexpr int SQUARES = 49;

// Z[player][bit]. The player index is absolute (0 = first player, 1 = second player), because a
// hash must describe the stones themselves; "side to move" flips every ply and would make the
// same stone hash differently depending on whose turn it is.
//
// There is deliberately no side-to-move key. In chess a position's side to move is independent
// of the pieces, so it needs its own key. In Connect-4 the side to move is fully determined by
// the number of stones (even = first player), and the stones are already in the hash, so a
// side-to-move key would add nothing.
struct Table {
    std::array<std::array<std::uint64_t, SQUARES>, PLAYERS> keys{};
};

constexpr Table makeTable() {
    Table t;
    SplitMix64 rng(SEED);
    for (auto& player : t.keys) {
        for (auto& key : player) {
            key = rng.next();
        }
    }
    return t;
}

// Built at compile time, so there is no static initialization order to worry about and no
// startup cost.
inline constexpr Table TABLE = makeTable();

constexpr std::uint64_t pieceKey(int player, int bit) { return TABLE.keys[player][bit]; }

// Hash computed from nothing but the stones. play() maintains the same value incrementally;
// this function is the reference that tests compare against, and mirror() uses it.
std::uint64_t hashFromScratch(std::uint64_t firstPlayerStones, std::uint64_t secondPlayerStones);

}  // namespace zobrist
}  // namespace c4
