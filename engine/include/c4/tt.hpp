#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "c4/position.hpp"
#include "c4/score.hpp"

namespace c4 {

// What a stored score says about the true score of the position.
//   EXACT: the search finished inside its (alpha, beta) window, so the score is exact.
//   LOWER: the search failed high (score >= beta); the true score is at least this.
//   UPPER: the search failed low (score <= alpha); the true score is at most this.
// NONE marks an empty slot. It is needed because the empty board hashes to 0, so a zeroed
// entry would otherwise look like real data for that position.
enum class Bound : std::uint8_t { NONE = 0, EXACT = 1, LOWER = 2, UPPER = 3 };

inline constexpr std::uint8_t NO_MOVE = 255;

struct TTEntry {
    std::uint64_t key = 0;        // full Zobrist hash, to tell apart positions sharing a slot
    std::int16_t score = 0;       // node-relative (see toTTScore)
    std::uint8_t depth = 0;       // remaining depth the score was searched to, in plies
    Bound flag = Bound::NONE;
    std::uint8_t bestMove = NO_MOVE;  // column 0 to 6, or NO_MOVE
    // 3 bytes of padding follow, to keep the 8-byte alignment of key.
};
// 16 bytes means exactly four entries per 64-byte cache line, so a probe touches one line.
static_assert(sizeof(TTEntry) == 16, "TTEntry must stay 16 bytes");
// Node-relative mate scores can be up to MAX_MOVES beyond the root-relative range.
static_assert(WIN_SCORE + MAX_MOVES <= INT16_MAX, "scores must fit in TTEntry::score");

// What probe() hands back. The score is already converted back to root-relative.
struct TTHit {
    int score;
    int depth;
    Bound flag;
    int bestMove;  // -1 when the entry has no best move
};

struct TTStats {
    std::uint64_t probes = 0;
    std::uint64_t hits = 0;  // probes whose stored hash matched
};

// Mate scores are root-relative ("win at ply p from the root"), but a TT entry may be reused
// at a different ply, even in a different search. Storing the root-relative value would make
// "win in 3 from here" turn into the wrong distance when the same position is reached at another
// ply. So we store the distance from the node itself: add the node's ply on store and subtract
// it on probe (mirrored for losses). Non-mate scores do not depend on ply and pass through.
constexpr int toTTScore(int score, int ply) {
    if (isWinScore(score)) return score + ply;
    if (isLossScore(score)) return score - ply;
    return score;
}
constexpr int fromTTScore(int score, int ply) {
    if (isWinScore(score)) return score - ply;
    if (isLossScore(score)) return score + ply;
    return score;
}

// The flag rules in one place. Returns the stored score if it can replace a search to `depth`
// with window (alpha, beta), otherwise nullopt.
std::optional<int> usableScore(const TTHit& hit, int depth, int alpha, int beta);

class TranspositionTable {
public:
    // 2^sizeLog2 entries. A power-of-two size makes the index a cheap mask instead of a modulo.
    explicit TranspositionTable(int sizeLog2 = 20);

    // Looks up pos. `ply` is the node's distance from the search root, used for mate scores.
    std::optional<TTHit> probe(const Position& pos, int ply);

    // Always-replace: the new result overwrites whatever is in the slot. `bestMove` is a column
    // or -1 for none.
    void store(const Position& pos, int ply, int depth, int score, Bound flag, int bestMove);

    // Empties every slot and resets the statistics.
    void clear();

    const TTStats& stats() const { return stats_; }
    std::size_t size() const { return entries_.size(); }

private:
    std::size_t indexOf(std::uint64_t hash) const { return hash & (entries_.size() - 1); }

    std::vector<TTEntry> entries_;
    TTStats stats_;
};

}  // namespace c4
