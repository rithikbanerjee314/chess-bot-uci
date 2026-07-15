// Classical tapered evaluation. Weights/tables/formulas are ported from the
// JS engine's already-tuned constants (chess-bot-uci's prior engine.js,
// itself originating from the chess.html project) as a proven starting
// point — reimplemented against bitboards rather than a 64-element array
// scan, but the same evaluation *decisions*. No ongoing sync with that
// project; this is now a standalone codebase, free to diverge/retune.
#include "eval.hpp"
#include "magic.hpp"
#include <cmath>
#include <array>

namespace eval {
namespace {

constexpr int PHASE_WEIGHT[6] = {0, 1, 1, 2, 4, 0}; // P N B R Q K

constexpr int PV_MG[6] = {82, 337, 365, 477, 1025, 20000};
constexpr int PV_EG[6] = {94, 281, 297, 512, 936, 20000};

constexpr int PAWN_MG_PST[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
   3,  6,  6,-16,-16,  6,  6,  3,
   2, -3,  2,  3,  3,  2, -3,  2,
  -2, -2,  3, 28, 28,  3, -2, -2,
   3,  3, 12, 36, 36, 12,  3,  3,
   8,  8, 17, 24, 24, 17,  8,  8,
  46, 46, 46, 46, 46, 46, 46, 46,
   0,  0,  0,  0,  0,  0,  0,  0,
};
constexpr int PAWN_EG_PST[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
  -8, -8, -8, -8, -8, -8, -8, -8,
  -5, -5, -5, -5, -5, -5, -5, -5,
   2,  2,  2,  2,  2,  2,  2,  2,
  14, 14, 14, 14, 14, 14, 14, 14,
  28, 28, 28, 28, 28, 28, 28, 28,
  50, 50, 50, 50, 50, 50, 50, 50,
   0,  0,  0,  0,  0,  0,  0,  0,
};
constexpr int KNIGHT_MG_PST[64] = {
 -50,-40,-30,-30,-30,-30,-40,-50,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -30,  5, 10, 15, 15, 10,  5,-30,
 -30,  0, 15, 20, 20, 15,  0,-30,
 -30,  5, 15, 20, 20, 15,  5,-30,
 -30,  0, 10, 15, 15, 10,  0,-30,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -50,-40,-30,-30,-30,-30,-40,-50,
};
constexpr int KNIGHT_EG_PST[64] = {
 -60,-40,-25,-25,-25,-25,-40,-60,
 -40,-25, -5,  0,  0, -5,-25,-40,
 -25, -5, 15, 18, 18, 15, -5,-25,
 -25, -5, 18, 23, 23, 18, -5,-25,
 -25, -5, 18, 23, 23, 18, -5,-25,
 -25, -5, 15, 18, 18, 15, -5,-25,
 -40,-25, -5,  0,  0, -5,-25,-40,
 -60,-40,-25,-25,-25,-25,-40,-60,
};
constexpr int BISHOP_PST[64] = {
 -20,-10,-10,-10,-10,-10,-10,-20,
 -10,  5,  0,  0,  0,  0,  5,-10,
 -10,  8, 10, 10, 10, 10,  8,-10,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -10,  5,  5, 10, 10,  5,  5,-10,
 -10,  0,  5, 10, 10,  5,  0,-10,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -20,-10,-10,-10,-10,-10,-10,-20,
};
constexpr int ROOK_MG_PST[64] = {
   0,  0,  0,  5,  5,  0,  0,  0,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   5, 10, 10, 10, 10, 10, 10,  5,
   0,  0,  0,  5,  5,  0,  0,  0,
};
constexpr int ROOK_EG_PST[64] = {
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   0,  0,  0,  0,  0,  0,  0,  0,
};
constexpr int QUEEN_PST[64] = {
 -20,-10,-10, -5, -5,-10,-10,-20,
 -10,  0,  5,  0,  0,  5,  0,-10,
 -10,  5,  5,  5,  5,  5,  5,-10,
   0,  0,  5,  5,  5,  5,  0, -5,
  -5,  0,  5,  5,  5,  5,  0, -5,
 -10,  0,  5,  5,  5,  5,  0,-10,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -20,-10,-10, -5, -5,-10,-10,-20,
};
constexpr int KING_MG_PST[64] = {
  20, 30, 10,  0,  0, 10, 30, 20,
  20, 20,  0,  0,  0,  0, 20, 20,
 -10,-20,-20,-20,-20,-20,-20,-10,
 -20,-30,-30,-40,-40,-30,-30,-20,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
};
constexpr int KING_EG_PST[64] = {
 -50,-40,-30,-20,-20,-30,-40,-50,
 -30,-20,-10,  0,  0,-10,-20,-30,
 -30,-10, 20, 30, 30, 20,-10,-30,
 -30,-10, 30, 40, 40, 30,-10,-30,
 -30,-10, 30, 40, 40, 30,-10,-30,
 -30,-10, 20, 30, 30, 20,-10,-30,
 -30,-30,  0,  0,  0,  0,-30,-30,
 -50,-30,-30,-30,-30,-30,-30,-50,
};

constexpr int PASSED_MG[8] = {0, 0,  7, 16, 17, 64, 170, 0};
constexpr int PASSED_EG[8] = {0, 0, 27, 32, 41, 72, 177, 0};
constexpr int THREAT_BY_PAWN_MG[6] = {0, 48, 48, 70, 90, 0}; // indexed by PieceType (P,K unused)
constexpr int THREAT_BY_PAWN_EG[6] = {0, 38, 38, 50, 60, 0};
constexpr int ATTACK_UNIT[6] = {0, 20, 20, 40, 80, 0};
constexpr int TEMPO_BONUS = 28;

constexpr Bitboard fileMaskOf(int f) { return FILE_A_BB << f; }

Square mirrorSq(Square s) { return (7 - rankOf(s)) * 8 + fileOf(s); }

int taperRound(int mg, int eg, int phase) {
  return static_cast<int>(std::lround(double(mg * phase + eg * (MAX_PHASE - phase)) / MAX_PHASE));
}

int getPST(PieceType t, bool white, Square sq, int phase) {
  Square idx = white ? sq : mirrorSq(sq);
  switch (t) {
    case PAWN:   return taperRound(PAWN_MG_PST[idx], PAWN_EG_PST[idx], phase);
    case KNIGHT: return taperRound(KNIGHT_MG_PST[idx], KNIGHT_EG_PST[idx], phase);
    case BISHOP: return BISHOP_PST[idx];
    case ROOK:   return taperRound(ROOK_MG_PST[idx], ROOK_EG_PST[idx], phase);
    case QUEEN:  return QUEEN_PST[idx];
    case KING:   return taperRound(KING_MG_PST[idx], KING_EG_PST[idx], phase);
    default:     return 0;
  }
}

int gamePhase(const Position& pos) {
  int p = 0;
  for (int t = 0; t < 6; t++) {
    if (!PHASE_WEIGHT[t]) continue;
    int cnt = popcount(pos.pieceBB[pieceIndex(WHITE, PieceType(t))]) +
              popcount(pos.pieceBB[pieceIndex(BLACK, PieceType(t))]);
    p += cnt * PHASE_WEIGHT[t];
  }
  return p > MAX_PHASE ? MAX_PHASE : p;
}

int phase128(const Position& pos) {
  return static_cast<int>(std::lround(gamePhase(pos) * 128.0 / MAX_PHASE));
}

Bitboard whitePawnAttacksBB(Bitboard pawns) {
  return ((pawns & ~FILE_A_BB) << 7) | ((pawns & ~FILE_H_BB) << 9);
}
Bitboard blackPawnAttacksBB(Bitboard pawns) {
  return ((pawns & ~FILE_A_BB) >> 9) | ((pawns & ~FILE_H_BB) >> 7);
}

struct MgEg { int mg = 0, eg = 0; };

// ---- Pawn structure: doubled, isolated, passed (rank-scaled), connected ----
MgEg pawnStructure(Bitboard wP, Bitboard bP) {
  MgEg out;
  for (int f = 0; f < 8; f++) {
    int wCnt = popcount(wP & fileMaskOf(f));
    int bCnt = popcount(bP & fileMaskOf(f));
    if (wCnt > 1) { out.mg -= 11 * (wCnt - 1); out.eg -= 56 * (wCnt - 1); }
    if (bCnt > 1) { out.mg += 11 * (bCnt - 1); out.eg += 56 * (bCnt - 1); }
    Bitboard neighborFiles = (f > 0 ? fileMaskOf(f - 1) : 0) | (f < 7 ? fileMaskOf(f + 1) : 0);
    if (wCnt > 0 && !(wP & neighborFiles)) { out.mg -= 5; out.eg -= 15; }
    if (bCnt > 0 && !(bP & neighborFiles)) { out.mg += 5; out.eg += 15; }
  }

  Bitboard wp = wP;
  while (wp) {
    Square s = popLsb(wp);
    int f = fileOf(s), r = rankOf(s);
    Bitboard aheadFiles = (f > 0 ? fileMaskOf(f - 1) : 0) | fileMaskOf(f) | (f < 7 ? fileMaskOf(f + 1) : 0);
    Bitboard aheadRanks = (r + 1 < 8) ? (~0ULL << ((r + 1) * 8)) : 0ULL;
    if (!(bP & aheadFiles & aheadRanks)) { out.mg += PASSED_MG[r]; out.eg += PASSED_EG[r]; }
  }
  Bitboard bp = bP;
  while (bp) {
    Square s = popLsb(bp);
    int f = fileOf(s), r = rankOf(s);
    Bitboard aheadFiles = (f > 0 ? fileMaskOf(f - 1) : 0) | fileMaskOf(f) | (f < 7 ? fileMaskOf(f + 1) : 0);
    Bitboard aheadRanks = (r > 0) ? (~0ULL >> (64 - r * 8)) : 0ULL;
    if (!(wP & aheadFiles & aheadRanks)) { out.mg -= PASSED_MG[7 - r]; out.eg -= PASSED_EG[7 - r]; }
  }

  auto connectedCount = [](Bitboard pawns) {
    int n = 0;
    Bitboard p1 = pawns;
    while (p1) {
      Square s = popLsb(p1);
      int f = fileOf(s), r = rankOf(s);
      bool found = false;
      Bitboard p2 = pawns;
      while (p2 && !found) {
        Square s2 = popLsb(p2);
        if (s2 == s) continue;
        int f2 = fileOf(s2), r2 = rankOf(s2);
        if (std::abs(f2 - f) == 1 && std::abs(r2 - r) <= 1) found = true;
      }
      if (found) n++;
    }
    return n;
  };
  int wc = connectedCount(wP), bc = connectedCount(bP);
  out.mg += (wc - bc) * 8;
  out.eg += (wc - bc) * 3;
  return out;
}

// ---- Mobility: pseudo-move count (own pieces excluded) weighted by piece type ----
MgEg mobility(const Position& pos) {
  static constexpr int W[6] = {0, 4, 5, 2, 1, 0};
  static constexpr int E[6] = {0, 3, 5, 4, 2, 0};
  MgEg out;
  Bitboard occ = pos.occAll;
  for (Color c : {WHITE, BLACK}) {
    Bitboard own = pos.occByColor[c];
    int sign = (c == WHITE) ? 1 : -1;
    for (PieceType t : {KNIGHT, BISHOP, ROOK, QUEEN}) {
      Bitboard bb = pos.pieceBB[pieceIndex(c, t)];
      while (bb) {
        Square s = popLsb(bb);
        Bitboard atk;
        switch (t) {
          case KNIGHT: atk = magic::knightAttacks(s); break;
          case BISHOP: atk = magic::bishopAttacks(s, occ); break;
          case ROOK:   atk = magic::rookAttacks(s, occ); break;
          default:     atk = magic::queenAttacks(s, occ); break;
        }
        int cnt = popcount(atk & ~own);
        out.mg += sign * W[t] * cnt;
        out.eg += sign * E[t] * cnt;
      }
    }
  }
  return out;
}

// ---- Rooks: open/semi-open file bonus, 7th-rank bonus ----
MgEg rooksEval(const Position& pos, Bitboard wPawnFiles, Bitboard bPawnFiles) {
  MgEg out;
  for (Color c : {WHITE, BLACK}) {
    int sign = (c == WHITE) ? 1 : -1;
    Bitboard friendFiles = (c == WHITE) ? wPawnFiles : bPawnFiles;
    Bitboard enemyFiles = (c == WHITE) ? bPawnFiles : wPawnFiles;
    int seventh = (c == WHITE) ? 6 : 1;
    Bitboard rooks = pos.pieceBB[pieceIndex(c, ROOK)];
    while (rooks) {
      Square s = popLsb(rooks);
      int f = fileOf(s);
      bool noFriend = !(friendFiles & fileMaskOf(f));
      bool noEnemy = !(enemyFiles & fileMaskOf(f));
      if (noFriend) {
        out.mg += sign * (noEnemy ? 25 : 10);
        out.eg += sign * (noEnemy ? 18 : 5);
      }
      if (rankOf(s) == seventh) { out.mg += sign * 16; out.eg += sign * 30; }
    }
  }
  return out;
}

// ---- Knight outposts: safe from enemy pawns, deeper = stronger, bonus if pawn-supported ----
MgEg knightOutposts(const Position& pos, Bitboard wPawns, Bitboard bPawns) {
  MgEg out;
  Bitboard wKnights = pos.pieceBB[pieceIndex(WHITE, KNIGHT)];
  while (wKnights) {
    Square s = popLsb(wKnights);
    int f = fileOf(s), r = rankOf(s);
    if (r < 3) continue;
    Bitboard adjFiles = (f > 0 ? fileMaskOf(f - 1) : 0) | (f < 7 ? fileMaskOf(f + 1) : 0);
    Bitboard aheadRanks = (r + 1 < 8) ? (~0ULL << ((r + 1) * 8)) : 0ULL;
    bool safe = !(bPawns & adjFiles & aheadRanks);
    if (safe) {
      out.mg += (r >= 4) ? 22 : 10;
      out.eg += (r >= 4) ? 10 : 5;
      Bitboard supportRankMask = (r - 1 >= 0) ? (RANK_1_BB << ((r - 1) * 8)) : 0ULL;
      if (wPawns & adjFiles & supportRankMask) { out.mg += 12; out.eg += 5; }
    }
  }
  Bitboard bKnights = pos.pieceBB[pieceIndex(BLACK, KNIGHT)];
  while (bKnights) {
    Square s = popLsb(bKnights);
    int f = fileOf(s), r = rankOf(s);
    if (r > 4) continue;
    Bitboard adjFiles = (f > 0 ? fileMaskOf(f - 1) : 0) | (f < 7 ? fileMaskOf(f + 1) : 0);
    Bitboard aheadRanks = (r > 0) ? (~0ULL >> (64 - r * 8)) : 0ULL;
    bool safe = !(wPawns & adjFiles & aheadRanks);
    if (safe) {
      out.mg -= (r <= 3) ? 22 : 10;
      out.eg -= (r <= 3) ? 10 : 5;
      Bitboard supportRankMask = (r + 1 < 8) ? (RANK_1_BB << ((r + 1) * 8)) : 0ULL;
      if (bPawns & adjFiles & supportRankMask) { out.mg -= 12; out.eg -= 5; }
    }
  }
  return out;
}

// ---- Threats: pawn attacks on enemy non-pawn/king pieces ----
MgEg threats(const Position& pos) {
  MgEg out;
  Bitboard wpAtk = whitePawnAttacksBB(pos.pieceBB[pieceIndex(WHITE, PAWN)]);
  Bitboard bpAtk = blackPawnAttacksBB(pos.pieceBB[pieceIndex(BLACK, PAWN)]);
  for (PieceType t : {KNIGHT, BISHOP, ROOK, QUEEN}) {
    Bitboard bTargets = pos.pieceBB[pieceIndex(BLACK, t)] & wpAtk;
    out.mg += THREAT_BY_PAWN_MG[t] * popcount(bTargets);
    out.eg += THREAT_BY_PAWN_EG[t] * popcount(bTargets);
    Bitboard wTargets = pos.pieceBB[pieceIndex(WHITE, t)] & bpAtk;
    out.mg -= THREAT_BY_PAWN_MG[t] * popcount(wTargets);
    out.eg -= THREAT_BY_PAWN_EG[t] * popcount(wTargets);
  }
  return out;
}

// ---- Space: safe central squares (files c-f) in own half, weighted by minor count ----
int spaceMG(const Position& pos, Bitboard wPawns, Bitboard bPawns) {
  Bitboard wpAtk = whitePawnAttacksBB(wPawns);
  Bitboard bpAtk = blackPawnAttacksBB(bPawns);
  Bitboard centralFiles = fileMaskOf(2) | fileMaskOf(3) | fileMaskOf(4) | fileMaskOf(5);
  Bitboard wZone = centralFiles & (RANK_1_BB << 8 | RANK_1_BB << 16 | RANK_1_BB << 24); // ranks 2-4 (idx 1-3)
  Bitboard bZone = centralFiles & (RANK_1_BB << 32 | RANK_1_BB << 40 | RANK_1_BB << 48); // ranks 5-7 (idx 4-6)
  int wSpace = popcount(wZone & ~bpAtk & ~pos.occAll);
  int bSpace = popcount(bZone & ~wpAtk & ~pos.occAll);
  int wMinors = popcount(pos.pieceBB[pieceIndex(WHITE, KNIGHT)] | pos.pieceBB[pieceIndex(WHITE, BISHOP)]);
  int bMinors = popcount(pos.pieceBB[pieceIndex(BLACK, KNIGHT)] | pos.pieceBB[pieceIndex(BLACK, BISHOP)]);
  return (wSpace * (wMinors + 1) - bSpace * (bMinors + 1)) * 2;
}

// ---- King zone safety: pawn shelter + open-file penalty + attacker pressure ----
MgEg kingZoneSafety(const Position& pos, Color color) {
  Bitboard kingBB = pos.pieceBB[pieceIndex(color, KING)];
  if (!kingBB) return {};
  Square kSq = lsb(kingBB);
  int kR = rankOf(kSq), kF = fileOf(kSq);
  int dir = (color == WHITE) ? 1 : -1;
  Color enemy = Color(1 - color);
  Bitboard pawnBB = pos.pieceBB[pieceIndex(color, PAWN)];

  int shelter = 0;
  for (int df = -1; df <= 1; df++) {
    int sf = kF + df;
    if (sf < 0 || sf > 7) continue;
    int r1 = kR + dir, r2 = kR + 2 * dir;
    bool has1 = (r1 >= 0 && r1 < 8) && (pawnBB & sqBB(makeSquare(r1, sf)));
    bool has2 = (r2 >= 0 && r2 < 8) && (pawnBB & sqBB(makeSquare(r2, sf)));
    if (has1) shelter += 14;
    else if (has2) shelter += 7;
    else shelter -= 16;
  }
  for (int df = -1; df <= 1; df++) {
    int f = kF + df;
    if (f < 0 || f > 7) continue;
    if (!(pawnBB & fileMaskOf(f))) shelter -= 18;
  }

  Bitboard zone = magic::kingAttacks(kSq) | sqBB(kSq);
  for (int df = -1; df <= 1; df++) {
    int nf = kF + df;
    if (nf < 0 || nf > 7) continue;
    int nr = kR + 2 * dir;
    if (nr >= 0 && nr < 8) zone |= sqBB(makeSquare(nr, nf));
  }

  int attackers = 0, attackUnits = 0;
  Bitboard occ = pos.occAll;
  for (PieceType t : {KNIGHT, BISHOP, ROOK, QUEEN}) {
    Bitboard bb = pos.pieceBB[pieceIndex(enemy, t)];
    while (bb) {
      Square s = popLsb(bb);
      Bitboard atk;
      switch (t) {
        case KNIGHT: atk = magic::knightAttacks(s); break;
        case BISHOP: atk = magic::bishopAttacks(s, occ); break;
        case ROOK:   atk = magic::rookAttacks(s, occ); break;
        default:     atk = magic::queenAttacks(s, occ); break;
      }
      if (atk & zone) { attackers++; attackUnits += ATTACK_UNIT[t]; }
    }
  }
  int attack = 0;
  if (attackers >= 2) {
    attack = static_cast<int>(std::lround(attackUnits * (1.0 + (attackers - 2) * 0.3) / 4.0));
  }
  return { shelter - attack, 0 };
}

int scaleFactor(const Position& pos, int egScoreWhitePOV) {
  int wB = popcount(pos.pieceBB[pieceIndex(WHITE, BISHOP)]);
  int bB = popcount(pos.pieceBB[pieceIndex(BLACK, BISHOP)]);
  int wMinors = popcount(pos.pieceBB[pieceIndex(WHITE, KNIGHT)]) + wB;
  int bMinors = popcount(pos.pieceBB[pieceIndex(BLACK, KNIGHT)]) + bB;
  int wRooks = popcount(pos.pieceBB[pieceIndex(WHITE, ROOK)]);
  int bRooks = popcount(pos.pieceBB[pieceIndex(BLACK, ROOK)]);
  int wQ = popcount(pos.pieceBB[pieceIndex(WHITE, QUEEN)]);
  int bQ = popcount(pos.pieceBB[pieceIndex(BLACK, QUEEN)]);
  int wPawns = popcount(pos.pieceBB[pieceIndex(WHITE, PAWN)]);
  int bPawns = popcount(pos.pieceBB[pieceIndex(BLACK, PAWN)]);

  if (wB == 1 && bB == 1 && wMinors == 1 && bMinors == 1 && wRooks == 0 && bRooks == 0 && wQ == 0 && bQ == 0) {
    Bitboard wBB = pos.pieceBB[pieceIndex(WHITE, BISHOP)];
    Bitboard bBB = pos.pieceBB[pieceIndex(BLACK, BISHOP)];
    Square ws = lsb(wBB), bs = lsb(bBB);
    int wColor = (rankOf(ws) + fileOf(ws)) & 1;
    int bColor = (rankOf(bs) + fileOf(bs)) & 1;
    if (wColor != bColor) {
      int diff = wPawns > bPawns ? wPawns - bPawns : bPawns - wPawns;
      int v = 16 + diff * 4;
      return v < 36 ? v : 36;
    }
  }
  bool winnerIsWhite = egScoreWhitePOV > 0;
  int winnerPawns = winnerIsWhite ? wPawns : bPawns;
  if (winnerPawns == 0) {
    int adv = egScoreWhitePOV < 0 ? -egScoreWhitePOV : egScoreWhitePOV;
    if (adv < 400) return 8;
    if (adv < 700) return 24;
  }
  return 64;
}

MgEg evaluateMGEG(const Position& pos) {
  MgEg out;
  int wB = 0, bB = 0;

  for (Color c : {WHITE, BLACK}) {
    int sign = (c == WHITE) ? 1 : -1;
    for (int t = 0; t < 6; t++) {
      Bitboard bb = pos.pieceBB[pieceIndex(c, PieceType(t))];
      while (bb) {
        Square s = popLsb(bb);
        int pstMG = getPST(PieceType(t), c == WHITE, s, MAX_PHASE);
        int pstEG = getPST(PieceType(t), c == WHITE, s, 0);
        out.mg += sign * (PV_MG[t] + pstMG);
        out.eg += sign * (PV_EG[t] + pstEG);
        if (t == BISHOP) { if (c == WHITE) wB++; else bB++; }
      }
    }
  }
  if (wB >= 2) { out.mg += 22; out.eg += 30; }
  if (bB >= 2) { out.mg -= 22; out.eg -= 30; }

  Bitboard wPawns = pos.pieceBB[pieceIndex(WHITE, PAWN)];
  Bitboard bPawns = pos.pieceBB[pieceIndex(BLACK, PAWN)];

  MgEg ps = pawnStructure(wPawns, bPawns);
  out.mg += ps.mg; out.eg += ps.eg;

  MgEg mob = mobility(pos);
  out.mg += mob.mg; out.eg += mob.eg;

  Bitboard wPawnFiles = 0, bPawnFiles = 0;
  for (int f = 0; f < 8; f++) {
    if (wPawns & fileMaskOf(f)) wPawnFiles |= fileMaskOf(f);
    if (bPawns & fileMaskOf(f)) bPawnFiles |= fileMaskOf(f);
  }
  MgEg rk = rooksEval(pos, wPawnFiles, bPawnFiles);
  out.mg += rk.mg; out.eg += rk.eg;

  MgEg op = knightOutposts(pos, wPawns, bPawns);
  out.mg += op.mg; out.eg += op.eg;

  MgEg thr = threats(pos);
  out.mg += thr.mg; out.eg += thr.eg;

  out.mg += spaceMG(pos, wPawns, bPawns);

  MgEg wKing = kingZoneSafety(pos, WHITE);
  MgEg bKing = kingZoneSafety(pos, BLACK);
  out.mg += wKing.mg - bKing.mg;
  out.eg += (wKing.eg - bKing.eg) / 2;

  return out;
}

} // namespace

int evaluate(const Position& pos, bool includeTempo) {
  MgEg r = evaluateMGEG(pos);
  int p = phase128(pos);
  int sf = scaleFactor(pos, r.eg);
  int scaledEG = (r.eg * sf) / 64;
  int v = (r.mg * p + scaledEG * (128 - p)) / 128;
  if (includeTempo) v += (pos.sideToMove == WHITE) ? TEMPO_BONUS : -TEMPO_BONUS;
  return v;
}

int pieceValueMG(PieceType t) { return PV_MG[t]; }
int pstValue(PieceType t, bool white, Square sq, int phase) { return getPST(t, white, sq, phase); }

int fastEval(const Position& pos, bool includeTempo) {
  int mg = 0, eg = 0;
  for (Color c : {WHITE, BLACK}) {
    int sign = (c == WHITE) ? 1 : -1;
    for (int t = 0; t < 6; t++) {
      Bitboard bb = pos.pieceBB[pieceIndex(c, PieceType(t))];
      while (bb) {
        Square s = popLsb(bb);
        mg += sign * (PV_MG[t] + getPST(PieceType(t), c == WHITE, s, MAX_PHASE));
        eg += sign * (PV_EG[t] + getPST(PieceType(t), c == WHITE, s, 0));
      }
    }
  }
  int p = phase128(pos);
  int v = (mg * p + eg * (128 - p)) / 128;
  if (includeTempo) v += (pos.sideToMove == WHITE) ? TEMPO_BONUS : -TEMPO_BONUS;
  return v;
}

} // namespace eval
