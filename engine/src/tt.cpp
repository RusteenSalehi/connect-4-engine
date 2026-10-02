#include "c4/tt.hpp"

#include <algorithm>

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

TranspositionTable::TranspositionTable(int sizeLog2) : entries_(std::size_t{1} << sizeLog2) {}

std::optional<TTHit> TranspositionTable::probe(const Position& pos, int ply) {
    ++stats_.probes;
    const TTEntry& e = entries_[indexOf(pos.hash())];
    // Many positions share each slot; only the full 64-bit hash says whether this one is ours.
    if (e.flag == Bound::NONE || e.key != pos.hash()) {
        return std::nullopt;
    }
    ++stats_.hits;
    return TTHit{fromTTScore(e.score, ply), e.depth, e.flag,
                 e.bestMove == NO_MOVE ? -1 : static_cast<int>(e.bestMove)};
}

void TranspositionTable::store(const Position& pos, int ply, int depth, int score, Bound flag,
                               int bestMove) {
    TTEntry& e = entries_[indexOf(pos.hash())];
    e.key = pos.hash();
    // Every score fits: real scores lie within +/- WIN_SCORE (10000), well inside int16.
    e.score = static_cast<std::int16_t>(toTTScore(score, ply));
    e.depth = static_cast<std::uint8_t>(depth);
    e.flag = flag;
    e.bestMove = bestMove < 0 ? NO_MOVE : static_cast<std::uint8_t>(bestMove);
}

void TranspositionTable::clear() {
    std::fill(entries_.begin(), entries_.end(), TTEntry{});
    stats_ = {};
}

}  // namespace c4
