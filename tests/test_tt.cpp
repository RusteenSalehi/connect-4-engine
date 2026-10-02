#include <doctest/doctest.h>

#include "c4/position.hpp"
#include "c4/score.hpp"
#include "c4/tt.hpp"

#include <string>

using namespace c4;

namespace {

Position parse(const std::string& s) {
    auto p = Position::fromString(s);
    REQUIRE(p.has_value());
    return *p;
}

}  // namespace

TEST_CASE("TT store/probe round trip") {
    TranspositionTable tt(10);
    Position p = parse("4453");
    CHECK_FALSE(tt.probe(p, 0).has_value());  // empty table

    tt.store(p, 0, 7, 123, Bound::EXACT, 2);
    auto hit = tt.probe(p, 0);
    REQUIRE(hit.has_value());
    CHECK(hit->score == 123);
    CHECK(hit->depth == 7);
    CHECK(hit->flag == Bound::EXACT);
    CHECK(hit->bestMove == 2);

    // "No best move" survives the 255 encoding as -1.
    tt.store(p, 0, 3, -45, Bound::UPPER, -1);
    hit = tt.probe(p, 0);
    REQUIRE(hit.has_value());
    CHECK(hit->bestMove == -1);
    CHECK(hit->score == -45);
    CHECK(hit->flag == Bound::UPPER);

    CHECK(tt.stats().probes == 3);
    CHECK(tt.stats().hits == 2);
}

TEST_CASE("empty board (hash 0) is not mistaken for a stored entry") {
    // A zero-filled slot has key 0, the same as the empty board's hash. The NONE flag is what
    // keeps this a miss.
    TranspositionTable tt(4);
    Position empty;
    REQUIRE(empty.hash() == 0);
    CHECK_FALSE(tt.probe(empty, 0).has_value());
}

TEST_CASE("a mismatched key returns a miss") {
    // With 2 slots, two of the seven one-move positions must share a slot (pigeonhole).
    TranspositionTable tt(1);
    Position a, b;
    bool found = false;
    for (int i = 0; i < WIDTH && !found; ++i) {
        for (int j = i + 1; j < WIDTH && !found; ++j) {
            Position pi, pj;
            pi.play(i);
            pj.play(j);
            if ((pi.hash() & 1) == (pj.hash() & 1)) {
                a = pi;
                b = pj;
                found = true;
            }
        }
    }
    REQUIRE(found);
    REQUIRE(a.hash() != b.hash());

    tt.store(a, 0, 5, 10, Bound::EXACT, 3);
    CHECK_FALSE(tt.probe(b, 0).has_value());  // same slot, different hash
    CHECK(tt.probe(a, 0).has_value());

    // Always-replace: storing b evicts a.
    tt.store(b, 0, 1, 20, Bound::EXACT, 4);
    CHECK_FALSE(tt.probe(a, 0).has_value());
    REQUIRE(tt.probe(b, 0).has_value());
    CHECK(tt.probe(b, 0)->score == 20);
}

TEST_CASE("mate score adjustment round trip at different plies") {
    // Pure conversion functions: storing then probing at the same ply returns the input.
    for (int ply = 0; ply <= MAX_MOVES; ++ply) {
        for (int score : {0, 37, -500, 5000, -5000, WIN_SCORE - 1, WIN_SCORE - MAX_MOVES,
                          -(WIN_SCORE - 1), -(WIN_SCORE - MAX_MOVES)}) {
            CHECK(fromTTScore(toTTScore(score, ply), ply) == score);
        }
    }

    // Through the table: at ply 4 the side to move can win 3 plies later, i.e. at root ply 7.
    TranspositionTable tt(10);
    Position p = parse("44");
    tt.store(p, 4, 6, WIN_SCORE - 7, Bound::EXACT, 3);
    // Reached at ply 2 in another search, it is still a win 3 plies later: root ply 5.
    CHECK(tt.probe(p, 2)->score == WIN_SCORE - 5);
    // Reached at ply 10: root ply 13.
    CHECK(tt.probe(p, 10)->score == WIN_SCORE - 13);

    // The same for a loss.
    tt.store(p, 4, 6, -(WIN_SCORE - 9), Bound::EXACT, 3);
    CHECK(tt.probe(p, 0)->score == -(WIN_SCORE - 5));
    CHECK(tt.probe(p, 8)->score == -(WIN_SCORE - 13));

    // Heuristic scores never change with ply.
    tt.store(p, 4, 6, 250, Bound::EXACT, 3);
    CHECK(tt.probe(p, 0)->score == 250);
    CHECK(tt.probe(p, 20)->score == 250);
}

TEST_CASE("flag semantics") {
    const int alpha = -10;
    const int beta = 10;
    auto hit = [](int score, int depth, Bound flag) { return TTHit{score, depth, flag, -1}; };

    // EXACT is usable whenever the depth is sufficient, inside or outside the window.
    CHECK(usableScore(hit(3, 5, Bound::EXACT), 5, alpha, beta) == 3);
    CHECK(usableScore(hit(50, 6, Bound::EXACT), 5, alpha, beta) == 50);
    // Too shallow: never usable, even when exact.
    CHECK_FALSE(usableScore(hit(3, 4, Bound::EXACT), 5, alpha, beta).has_value());

    // LOWER bound: only proves a fail high if it already reaches beta.
    CHECK(usableScore(hit(10, 5, Bound::LOWER), 5, alpha, beta) == 10);
    CHECK(usableScore(hit(40, 5, Bound::LOWER), 5, alpha, beta) == 40);
    CHECK_FALSE(usableScore(hit(9, 5, Bound::LOWER), 5, alpha, beta).has_value());

    // UPPER bound: only proves a fail low if it is already at or below alpha.
    CHECK(usableScore(hit(-10, 5, Bound::UPPER), 5, alpha, beta) == -10);
    CHECK(usableScore(hit(-40, 5, Bound::UPPER), 5, alpha, beta) == -40);
    CHECK_FALSE(usableScore(hit(-9, 5, Bound::UPPER), 5, alpha, beta).has_value());
}

TEST_CASE("TT default size is 2^20 entries, and clear empties it") {
    CHECK(TranspositionTable().size() == (std::size_t{1} << 20));
    TranspositionTable tt(3);
    CHECK(tt.size() == 8);

    Position p = parse("1");
    tt.store(p, 0, 1, 1, Bound::EXACT, 0);
    tt.clear();
    CHECK_FALSE(tt.probe(p, 0).has_value());
    CHECK(tt.stats().probes == 1);  // the probe just above, after the reset
    CHECK(tt.stats().hits == 0);
}