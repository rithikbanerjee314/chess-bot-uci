#include "movegen.hpp"
#include "magic.hpp"

namespace {

void addMove(std::vector<Move>& moves, Square from, Square to, bool isCapture,
             PieceType promo = NO_PIECE_TYPE, bool isEP = false, bool isDoublePush = false, bool isCastle = false) {
  moves.push_back(Move{from, to, promo, isCapture, isEP, isDoublePush, isCastle});
}

void addPieceMoves(std::vector<Move>& moves, Square from, Bitboard attacks, Bitboard enemy) {
  Bitboard a = attacks;
  while (a) {
    Square to = popLsb(a);
    addMove(moves, from, to, bool(enemy & sqBB(to)));
  }
}

} // namespace

void generatePseudoMoves(const Position& pos, std::vector<Move>& moves) {
  Color us = pos.sideToMove, them = Color(1 - us);
  Bitboard own = pos.occByColor[us], enemy = pos.occByColor[them], occ = pos.occAll;

  // ---- Pawns ----
  Bitboard pawns = pos.pieceBB[pieceIndex(us, PAWN)];
  int pushDir = (us == WHITE) ? 8 : -8;
  int startRank = (us == WHITE) ? 1 : 6;
  int promoRank = (us == WHITE) ? 7 : 0;
  Bitboard p = pawns;
  while (p) {
    Square s = popLsb(p);
    int r = rankOf(s);
    Square one = s + pushDir;
    if (one >= 0 && one < 64 && !(occ & sqBB(one))) {
      if (rankOf(one) == promoRank) {
        for (PieceType pt : {QUEEN, ROOK, BISHOP, KNIGHT}) addMove(moves, s, one, false, pt);
      } else {
        addMove(moves, s, one, false);
        if (r == startRank) {
          Square two = one + pushDir;
          if (!(occ & sqBB(two))) addMove(moves, s, two, false, NO_PIECE_TYPE, false, true);
        }
      }
    }
    Bitboard atk = magic::pawnAttacks(s, us == WHITE) & enemy;
    Bitboard a = atk;
    while (a) {
      Square to = popLsb(a);
      if (rankOf(to) == promoRank) {
        for (PieceType pt : {QUEEN, ROOK, BISHOP, KNIGHT}) addMove(moves, s, to, true, pt);
      } else {
        addMove(moves, s, to, true);
      }
    }
    if (pos.epSquare >= 0 && (magic::pawnAttacks(s, us == WHITE) & sqBB(pos.epSquare))) {
      addMove(moves, s, pos.epSquare, true, NO_PIECE_TYPE, true);
    }
  }

  // ---- Knights ----
  Bitboard knights = pos.pieceBB[pieceIndex(us, KNIGHT)];
  while (knights) {
    Square s = popLsb(knights);
    addPieceMoves(moves, s, magic::knightAttacks(s) & ~own, enemy);
  }
  // ---- Bishops ----
  Bitboard bishops = pos.pieceBB[pieceIndex(us, BISHOP)];
  while (bishops) {
    Square s = popLsb(bishops);
    addPieceMoves(moves, s, magic::bishopAttacks(s, occ) & ~own, enemy);
  }
  // ---- Rooks ----
  Bitboard rooks = pos.pieceBB[pieceIndex(us, ROOK)];
  while (rooks) {
    Square s = popLsb(rooks);
    addPieceMoves(moves, s, magic::rookAttacks(s, occ) & ~own, enemy);
  }
  // ---- Queens ----
  Bitboard queens = pos.pieceBB[pieceIndex(us, QUEEN)];
  while (queens) {
    Square s = popLsb(queens);
    addPieceMoves(moves, s, magic::queenAttacks(s, occ) & ~own, enemy);
  }
  // ---- King (incl. castling) ----
  Bitboard kingBB = pos.pieceBB[pieceIndex(us, KING)];
  if (kingBB) {
    Square s = lsb(kingBB);
    addPieceMoves(moves, s, magic::kingAttacks(s) & ~own, enemy);

    int rank = (us == WHITE) ? 0 : 7;
    Square kingStart = makeSquare(rank, 4);
    if (s == kingStart) {
      uint8_t kRight = (us == WHITE) ? CASTLE_WK : CASTLE_BK;
      uint8_t qRight = (us == WHITE) ? CASTLE_WQ : CASTLE_BQ;
      if ((pos.castling & kRight) &&
          !(occ & (sqBB(makeSquare(rank, 5)) | sqBB(makeSquare(rank, 6)))) &&
          pos.mailbox[makeSquare(rank, 7)] == pieceIndex(us, ROOK) &&
          !isSquareAttacked(pos, makeSquare(rank, 4), them) &&
          !isSquareAttacked(pos, makeSquare(rank, 5), them) &&
          !isSquareAttacked(pos, makeSquare(rank, 6), them)) {
        addMove(moves, s, makeSquare(rank, 6), false, NO_PIECE_TYPE, false, false, true);
      }
      if ((pos.castling & qRight) &&
          !(occ & (sqBB(makeSquare(rank, 1)) | sqBB(makeSquare(rank, 2)) | sqBB(makeSquare(rank, 3)))) &&
          pos.mailbox[makeSquare(rank, 0)] == pieceIndex(us, ROOK) &&
          !isSquareAttacked(pos, makeSquare(rank, 4), them) &&
          !isSquareAttacked(pos, makeSquare(rank, 3), them) &&
          !isSquareAttacked(pos, makeSquare(rank, 2), them)) {
        addMove(moves, s, makeSquare(rank, 2), false, NO_PIECE_TYPE, false, false, true);
      }
    }
  }
}

void generateLegalMoves(Position& pos, std::vector<Move>& moves) {
  std::vector<Move> pseudo;
  pseudo.reserve(48);
  generatePseudoMoves(pos, pseudo);
  moves.clear();
  moves.reserve(pseudo.size());
  Color mover = pos.sideToMove;
  for (const Move& m : pseudo) {
    Undo u;
    makeMove(pos, m, u);
    if (!inCheck(pos, mover)) moves.push_back(m);
    unmakeMove(pos, m, u);
  }
}
