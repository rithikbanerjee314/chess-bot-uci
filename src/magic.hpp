#pragma once
#include "bitboard.hpp"

// Precomputed attack tables (knight/king/pawn) and magic-bitboard sliding
// attack lookups (rook/bishop/queen). magic::init() must be called once
// before any of the lookup functions are used — it generates magic numbers
// via randomized search (standard technique, ~seconds at most) and builds
// all attack tables. Nothing here is persisted between runs; it's cheap
// enough to redo on every process start.
namespace magic {

void init();

Bitboard knightAttacks(Square s);
Bitboard kingAttacks(Square s);
Bitboard pawnAttacks(Square s, bool white);

Bitboard rookAttacks(Square s, Bitboard occupied);
Bitboard bishopAttacks(Square s, Bitboard occupied);
inline Bitboard queenAttacks(Square s, Bitboard occupied) {
  return rookAttacks(s, occupied) | bishopAttacks(s, occupied);
}

// Slow, no-lookup-table ray attack generation. Used internally to build the
// magic tables, but also exposed since it's occasionally useful (e.g. SEE's
// x-ray re-scans don't need blazing speed and this avoids a magic-table
// dependency for tooling/tests).
Bitboard rookAttacksSlow(Square s, Bitboard occupied);
Bitboard bishopAttacksSlow(Square s, Bitboard occupied);

} // namespace magic
