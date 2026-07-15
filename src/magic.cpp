#include "magic.hpp"
#include <vector>
#include <algorithm>

namespace magic {
namespace {

Bitboard knightTable[64];
Bitboard kingTable[64];
Bitboard pawnTable[2][64]; // [0]=white, [1]=black

struct MagicEntry {
  Bitboard mask = 0;
  Bitboard magic = 0;
  int shift = 0;
  std::vector<Bitboard> table;
};

MagicEntry rookMagics[64];
MagicEntry bishopMagics[64];

// Deterministic xorshift64 PRNG — magic numbers only need to exist, not be
// cryptographically random, and determinism makes runs reproducible.
uint64_t rngState = 0x9E3779B97F4A7C15ULL;
uint64_t xorshift64() {
  uint64_t x = rngState;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  rngState = x;
  return x;
}
Bitboard randomSparse() { return xorshift64() & xorshift64() & xorshift64(); }

// Standard reference mask construction: for each of the 4 ray directions,
// include squares up to (but not including) the board edge in that
// direction. The excluded edge square is always part of the attack set
// regardless of its own occupancy (there's nothing beyond it to block), so
// its occupancy bit carries no information — omitting it shrinks the table.
Bitboard rookMask(Square sq) {
  Bitboard result = 0;
  int rk = rankOf(sq), fl = fileOf(sq);
  for (int r = rk + 1; r <= 6; r++) result |= sqBB(makeSquare(r, fl));
  for (int r = rk - 1; r >= 1; r--) result |= sqBB(makeSquare(r, fl));
  for (int f = fl + 1; f <= 6; f++) result |= sqBB(makeSquare(rk, f));
  for (int f = fl - 1; f >= 1; f--) result |= sqBB(makeSquare(rk, f));
  return result;
}

Bitboard bishopMask(Square sq) {
  Bitboard result = 0;
  int rk = rankOf(sq), fl = fileOf(sq);
  for (int r = rk + 1, f = fl + 1; r <= 6 && f <= 6; r++, f++) result |= sqBB(makeSquare(r, f));
  for (int r = rk + 1, f = fl - 1; r <= 6 && f >= 1; r++, f--) result |= sqBB(makeSquare(r, f));
  for (int r = rk - 1, f = fl + 1; r >= 1 && f <= 6; r--, f++) result |= sqBB(makeSquare(r, f));
  for (int r = rk - 1, f = fl - 1; r >= 1 && f >= 1; r--, f--) result |= sqBB(makeSquare(r, f));
  return result;
}

// Carry-Rippler trick: enumerates every subset of `mask`'s set bits.
std::vector<Bitboard> allSubsets(Bitboard mask) {
  std::vector<Bitboard> subsets;
  subsets.reserve(size_t(1) << popcount(mask));
  Bitboard subset = 0;
  do {
    subsets.push_back(subset);
    subset = (subset - mask) & mask;
  } while (subset != 0);
  return subsets;
}

void findMagic(Square s, bool isRook, MagicEntry& entry) {
  entry.mask = isRook ? rookMask(s) : bishopMask(s);
  int n = popcount(entry.mask);
  entry.shift = 64 - n;

  std::vector<Bitboard> occupancies = allSubsets(entry.mask);
  std::vector<Bitboard> attacks(occupancies.size());
  for (size_t i = 0; i < occupancies.size(); i++) {
    attacks[i] = isRook ? rookAttacksSlow(s, occupancies[i]) : bishopAttacksSlow(s, occupancies[i]);
  }

  entry.table.assign(occupancies.size(), BB_ALL); // BB_ALL == "unused" sentinel;
  // safe since no rook/bishop attack set (max 14/13 squares) can ever equal it.
  for (;;) {
    Bitboard candidate = randomSparse();
    if (popcount((entry.mask * candidate) >> 56) < 6) continue; // quick reject: poor bit spread
    std::fill(entry.table.begin(), entry.table.end(), BB_ALL);
    bool ok = true;
    for (size_t i = 0; i < occupancies.size() && ok; i++) {
      Bitboard idx = (occupancies[i] * candidate) >> entry.shift;
      if (entry.table[idx] == BB_ALL) entry.table[idx] = attacks[i];
      else if (entry.table[idx] != attacks[i]) ok = false;
    }
    if (ok) {
      entry.magic = candidate;
      return;
    }
  }
}

void initLeaperTables() {
  static const int knightOffsets[8][2] = {{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
  static const int kingOffsets[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
  for (Square s = 0; s < 64; s++) {
    int r = rankOf(s), f = fileOf(s);
    Bitboard nb = 0, kb = 0;
    for (auto& o : knightOffsets) {
      int nr = r + o[0], nf = f + o[1];
      if (onBoard(nr, nf)) nb |= sqBB(makeSquare(nr, nf));
    }
    for (auto& o : kingOffsets) {
      int nr = r + o[0], nf = f + o[1];
      if (onBoard(nr, nf)) kb |= sqBB(makeSquare(nr, nf));
    }
    knightTable[s] = nb;
    kingTable[s] = kb;

    Bitboard wp = 0, bp = 0;
    if (onBoard(r + 1, f - 1)) wp |= sqBB(makeSquare(r + 1, f - 1));
    if (onBoard(r + 1, f + 1)) wp |= sqBB(makeSquare(r + 1, f + 1));
    if (onBoard(r - 1, f - 1)) bp |= sqBB(makeSquare(r - 1, f - 1));
    if (onBoard(r - 1, f + 1)) bp |= sqBB(makeSquare(r - 1, f + 1));
    pawnTable[0][s] = wp;
    pawnTable[1][s] = bp;
  }
}

bool g_initialized = false;

} // namespace

Bitboard rookAttacksSlow(Square s, Bitboard occupied) {
  static const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
  Bitboard attacks = 0;
  for (auto& d : dirs) {
    int r = rankOf(s) + d[0], f = fileOf(s) + d[1];
    while (onBoard(r, f)) {
      Square sq = makeSquare(r, f);
      attacks |= sqBB(sq);
      if (occupied & sqBB(sq)) break;
      r += d[0]; f += d[1];
    }
  }
  return attacks;
}

Bitboard bishopAttacksSlow(Square s, Bitboard occupied) {
  static const int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
  Bitboard attacks = 0;
  for (auto& d : dirs) {
    int r = rankOf(s) + d[0], f = fileOf(s) + d[1];
    while (onBoard(r, f)) {
      Square sq = makeSquare(r, f);
      attacks |= sqBB(sq);
      if (occupied & sqBB(sq)) break;
      r += d[0]; f += d[1];
    }
  }
  return attacks;
}

void init() {
  if (g_initialized) return;
  initLeaperTables();
  for (Square s = 0; s < 64; s++) {
    findMagic(s, true, rookMagics[s]);
    findMagic(s, false, bishopMagics[s]);
  }
  g_initialized = true;
}

Bitboard knightAttacks(Square s) { return knightTable[s]; }
Bitboard kingAttacks(Square s) { return kingTable[s]; }
Bitboard pawnAttacks(Square s, bool white) { return pawnTable[white ? 0 : 1][s]; }

Bitboard rookAttacks(Square s, Bitboard occupied) {
  const MagicEntry& e = rookMagics[s];
  Bitboard idx = ((occupied & e.mask) * e.magic) >> e.shift;
  return e.table[idx];
}

Bitboard bishopAttacks(Square s, Bitboard occupied) {
  const MagicEntry& e = bishopMagics[s];
  Bitboard idx = ((occupied & e.mask) * e.magic) >> e.shift;
  return e.table[idx];
}

} // namespace magic
