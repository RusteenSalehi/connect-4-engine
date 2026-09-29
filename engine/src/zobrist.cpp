#include "c4/zobrist.hpp"

#include <bit>

namespace c4::zobrist {

namespace {

std::uint64_t hashStones(std::uint64_t stones, int player) {
    std::uint64_t h = 0;
    while (stones != 0) {
        int bit = std::countr_zero(stones);
        h ^= pieceKey(player, bit);
        stones &= stones - 1;  // clear the lowest set bit
    }
    return h;
}

}  // namespace

std::uint64_t hashFromScratch(std::uint64_t firstPlayerStones, std::uint64_t secondPlayerStones) {
    return hashStones(firstPlayerStones, 0) ^ hashStones(secondPlayerStones, 1);
}

}  // namespace c4::zobrist
