// Weak solver for Connect-4 (win / draw / loss for the side to move), written
// independently of the engine, after Pascal Pons' published solver design: bitboards, pruning to
// non-losing moves, threat-count move ordering, an upper-bound TT and null-window search.
//
//   c4solve <budget seconds>   reads move strings (digits 1-7) on stdin, one per line, and prints
//                              "<moves> <W|D|L|?> <seconds>"   (? = budget ran out)
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using u64 = std::uint64_t;
constexpr int W = 7, H = 6;

constexpr u64 bottomMask() { u64 m = 0; for (int c = 0; c < W; ++c) m |= u64{1} << (c * (H + 1)); return m; }
constexpr u64 BOTTOM = bottomMask();
constexpr u64 BOARD = BOTTOM * ((u64{1} << H) - 1);
constexpr u64 columnMask(int c) { return ((u64{1} << H) - 1) << (c * (H + 1)); }
constexpr u64 topMask(int c) { return u64{1} << (H - 1 + c * (H + 1)); }

u64 winningCells(u64 p, u64 mask) {  // empty cells that would complete four for the stones p
    u64 r = (p << 1) & (p << 2) & (p << 3);
    for (int s : {H + 1, H, H + 2}) {
        u64 q = (p << s) & (p << 2 * s);
        r |= q & (p << 3 * s);
        r |= q & (p >> s);
        q = (p >> s) & (p >> 2 * s);
        r |= q & (p << s);
        r |= q & (p >> 3 * s);
    }
    return r & (BOARD ^ mask);
}

bool hasFour(u64 p) {
    for (int s : {1, H + 1, H, H + 2}) {
        u64 m = p & (p >> s);
        if (m & (m >> 2 * s)) return true;
    }
    return false;
}

struct Pos {
    u64 cur = 0, mask = 0;  // cur: stones of the side to move
    int moves = 0;
    u64 possible() const { return (mask + BOTTOM) & BOARD; }
    u64 ownWins() const { return winningCells(cur, mask); }
    u64 oppWins() const { return winningCells(cur ^ mask, mask); }
    bool canWinNext() const { return ownWins() & possible(); }
    void play(u64 move) { cur ^= mask; mask |= move; ++moves; }
    u64 key() const { return cur + mask; }
    u64 nonLosingMoves() const {
        u64 pm = possible();
        const u64 opp = oppWins();
        const u64 forced = pm & opp;
        if (forced) {
            if (forced & (forced - 1)) return 0;  // two threats: lost
            pm = forced;
        }
        return pm & ~(opp >> 1);  // never play directly below an opponent threat
    }
    int moveScore(u64 move) const { return std::popcount(winningCells(cur | move, mask)); }
};

constexpr int MIN_SCORE = -(W * H) / 2 + 3;

struct TT {
    struct Entry { u64 key = 0; std::uint8_t val = 0; };
    std::vector<Entry> t;
    int shift;
    explicit TT(int log2) : t(std::size_t{1} << log2), shift(64 - log2) {}
    std::size_t idx(u64 k) const { return (k * 0x9E3779B97F4A7C15ULL) >> shift; }
    void put(u64 k, int v) { t[idx(k)] = {k, static_cast<std::uint8_t>(v)}; }
    int get(u64 k) const { const Entry& e = t[idx(k)]; return e.key == k ? e.val : 0; }
};

struct Timeout {};

struct Solver {
    TT tt{23};
    u64 nodes = 0;
    std::chrono::steady_clock::time_point deadline;
    static constexpr int ORDER[W] = {3, 2, 4, 1, 5, 0, 6};

    int negamax(const Pos& P, int alpha, int beta) {
        if ((++nodes & 0xFFFFF) == 0 && std::chrono::steady_clock::now() > deadline) throw Timeout{};
        const u64 next = P.nonLosingMoves();
        if (!next) return -(W * H - P.moves) / 2;
        if (P.moves >= W * H - 2) return 0;
        const int lo = -(W * H - 2 - P.moves) / 2;
        if (alpha < lo) { alpha = lo; if (alpha >= beta) return alpha; }
        int hi = (W * H - 1 - P.moves) / 2;
        if (int v = tt.get(P.key())) hi = v + MIN_SCORE - 1;
        if (beta > hi) { beta = hi; if (alpha >= beta) return beta; }
        u64 cand[W]; int score[W]; int n = 0;
        for (int i = 0; i < W; ++i) {  // center first; stable sort keeps that order on ties
            const u64 m = next & columnMask(ORDER[i]);
            if (m) { cand[n] = m; score[n] = P.moveScore(m); ++n; }
        }
        for (int i = 1; i < n; ++i)
            for (int j = i; j > 0 && score[j] > score[j - 1]; --j) { std::swap(cand[j], cand[j - 1]); std::swap(score[j], score[j - 1]); }
        for (int i = 0; i < n; ++i) {
            Pos P2 = P;
            P2.play(cand[i]);
            const int s = -negamax(P2, -beta, -alpha);
            if (s >= beta) return s;
            if (s > alpha) alpha = s;
        }
        tt.put(P.key(), alpha - MIN_SCORE + 1);
        return alpha;
    }

    // Sign of the exact score: +1 win, 0 draw, -1 loss for the side to move.
    int weak(const Pos& P) {
        if (P.canWinNext()) return 1;
        int lo = -1, hi = 1;
        while (lo < hi) {
            int med = lo + (hi - lo) / 2;
            if (med <= 0 && lo / 2 < med) med = lo / 2;
            else if (med >= 0 && hi / 2 > med) med = hi / 2;
            const int r = negamax(P, med, med + 1);
            if (r <= med) hi = r; else lo = r;
        }
        return lo;
    }
};

int main(int argc, char** argv) {
    const double budget = argc > 1 ? std::atof(argv[1]) : 60;
    Solver s;
    std::string line;
    while (std::getline(std::cin, line)) {
        Pos P;
        bool over = false;  // the last move already won
        for (char ch : line) {
            const int c = ch - '1';
            const u64 move = (P.mask + BOTTOM) & columnMask(c);
            P.play(move);
            if (hasFour(P.cur ^ P.mask)) over = true;  // the player who just moved
        }
        const auto start = std::chrono::steady_clock::now();
        s.deadline = start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(budget));
        char r;
        if (over) r = 'L';
        else if (P.moves == W * H) r = 'D';
        else {
            try { const int v = s.weak(P); r = v > 0 ? 'W' : v < 0 ? 'L' : 'D'; }
            catch (Timeout) { r = '?'; }
        }
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::printf("%s %c %.2f\n", line.c_str(), r, secs);
        std::fflush(stdout);
    }
}
