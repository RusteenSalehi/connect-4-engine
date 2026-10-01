#include "c4/tt.hpp"

#include <bit>
#include <stdexcept>

namespace c4 {

std::optional<int> usableScore(const TTHit& hit, int depth, int alpha, int beta) {
    // A shallower search cannot stand in for a deeper one: it saw less of the tree.
    if (hit.depth < depth) {
        return std::nullopt;
    }
    switch (hit.flag) {
        case Bound::EXACT:
            return hit.score;
        case Bound::LOWER:
            // True score >= stored score >= beta: the node fails high whatever its exact value.
            if (hit.score >= beta) return hit.score;
            break;
        case Bound::UPPER:
            // True score <= stored score <= alpha: the node fails low whatever its exact value.
            if (hit.score <= alpha) return hit.score;
            break;
        case Bound::NONE:
            break;
    }
    return std::nullopt;
}

TranspositionTable::TranspositionTable(std::size_t entryCount) {
    if (!std::has_single_bit(entryCount)) {
        throw std::invalid_argument("TranspositionTable size must be a power of two");
    }
    entries_.resize(entryCount);
#ifdef C4_TT_VERIFY
    verifyKeys_.resize(entryCount);
#endif
}

std::optional<TTHit> TranspositionTable::probe(const Position& pos, int ply) {
    ++stats_.probes;
    const std::size_t index = indexOf(pos.hash());
    const TTEntry& e = entries_[index];
    // Many positions share each slot; only the full 64-bit hash says whether this one is ours.
    if (e.flag == Bound::NONE || e.key != pos.hash()) {
        return std::nullopt;
    }
    ++stats_.hits;
#ifdef C4_TT_VERIFY
    // Same 64-bit hash, different position: a true Zobrist collision. We only count it and still
    // return the hit, so a verify build searches exactly like a normal build and the count
    // measures what the normal build silently suffers.
    if (verifyKeys_[index] != pos.key()) {
        ++stats_.collisions;
    }
#endif
    return TTHit{fromTTScore(e.score, ply), e.depth, e.flag,
                 e.bestMove == NO_MOVE ? -1 : static_cast<int>(e.bestMove)};
}

void TranspositionTable::store(const Position& pos, int ply, int depth, int score, Bound flag,
                               int bestMove) {
    ++stats_.stores;
    const std::size_t index = indexOf(pos.hash());
    TTEntry& e = entries_[index];
    if (e.flag != Bound::NONE && e.key != pos.hash()) {
        ++stats_.overwrites;
    }
    e.key = pos.hash();
    // Every score fits: real scores lie within +/- WIN_SCORE (10000), well inside int16.
    e.score = static_cast<std::int16_t>(toTTScore(score, ply));
    e.depth = static_cast<std::uint8_t>(depth);
    e.flag = flag;
    e.bestMove = bestMove < 0 ? NO_MOVE : static_cast<std::uint8_t>(bestMove);
#ifdef C4_TT_VERIFY
    verifyKeys_[index] = pos.key();
#endif
}

void TranspositionTable::clear() {
    for (TTEntry& e : entries_) {
        e = TTEntry{};
    }
#ifdef C4_TT_VERIFY
    for (std::uint64_t& k : verifyKeys_) {
        k = 0;
    }
#endif
    resetStats();
}

}  // namespace c4
