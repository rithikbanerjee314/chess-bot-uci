#pragma once
#include <cstdint>
#include <string>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

// A bitboard is a 64-bit mask, one bit per square. Square index = rank*8 +
// file (a1=0, h1=7, a8=56, h8=63) — the standard LERF (little-endian
// rank-file) convention, and the same rank*8+file scheme the original
// JS engine used, so eval/search logic ports without a mental remapping.
using Bitboard = std::uint64_t;
using Square = int;

constexpr Bitboard BB_ONE = 1ULL;
constexpr Bitboard BB_EMPTY = 0ULL;
constexpr Bitboard BB_ALL = ~0ULL;

constexpr Bitboard sqBB(Square s) { return BB_ONE << s; }
constexpr int fileOf(Square s) { return s & 7; }
constexpr int rankOf(Square s) { return s >> 3; }
constexpr Square makeSquare(int rank, int file) { return rank * 8 + file; }
constexpr bool onBoard(int rank, int file) { return rank >= 0 && rank < 8 && file >= 0 && file < 8; }

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
constexpr Bitboard FILE_H_BB = FILE_A_BB << 7;
constexpr Bitboard RANK_1_BB = 0x00000000000000FFULL;
constexpr Bitboard RANK_8_BB = RANK_1_BB << 56;

inline int popcount(Bitboard b) {
#if defined(_MSC_VER)
  return static_cast<int>(__popcnt64(b));
#else
  return __builtin_popcountll(b);
#endif
}

// Index of the least-significant set bit. Undefined if b == 0 (callers must
// check emptiness first, as is standard for bitboard code).
inline Square lsb(Bitboard b) {
#if defined(_MSC_VER)
  unsigned long idx;
  _BitScanForward64(&idx, b);
  return static_cast<Square>(idx);
#else
  return __builtin_ctzll(b);
#endif
}

// Pops (clears) and returns the least-significant set bit's square index.
inline Square popLsb(Bitboard& b) {
  Square s = lsb(b);
  b &= b - 1;
  return s;
}

std::string squareToAlgebraic(Square s);
Square algebraicToSquare(const std::string& s);
std::string bitboardToString(Bitboard b);
