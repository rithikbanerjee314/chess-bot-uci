#pragma once
#include "bitboard.hpp"
#include <string>

enum PieceType : int { PAWN = 0, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_PIECE_TYPE = 6 };
enum Color : int { WHITE = 0, BLACK = 1 };

constexpr int pieceIndex(Color c, PieceType t) { return c * 6 + t; }
constexpr Color colorOf(int pieceIdx) { return Color(pieceIdx / 6); }
constexpr PieceType typeOf(int pieceIdx) { return PieceType(pieceIdx % 6); }

// Castling-rights bit layout, also used as the zobrist::castleKey index.
constexpr uint8_t CASTLE_WK = 1, CASTLE_WQ = 2, CASTLE_BK = 4, CASTLE_BQ = 8;

// Plain (not bit-packed) move representation. The isCapture/isEP/
// isDoublePush/isCastle flags are generation-time metadata that make
// makeMove/unmakeMove simpler — cheaper to compute once at generation time
// than to re-derive on every make.
struct Move {
  Square from = -1;
  Square to = -1;
  PieceType promo = NO_PIECE_TYPE;
  bool isCapture = false;
  bool isEP = false;
  bool isDoublePush = false;
  bool isCastle = false;

  bool isNull() const { return from < 0; }
  bool operator==(const Move& o) const {
    return from == o.from && to == o.to && promo == o.promo;
  }
};

namespace zobrist {
void init();
extern uint64_t pieceKey[12][64];
extern uint64_t sideKey;
extern uint64_t castleKey[16];
extern uint64_t epFileKey[8];
} // namespace zobrist

struct Position {
  Bitboard pieceBB[12] = {};
  Bitboard occByColor[2] = {};
  Bitboard occAll = 0;
  int8_t mailbox[64];

  Color sideToMove = WHITE;
  uint8_t castling = 0;
  int epSquare = -1;
  int halfmoveClock = 0;
  int fullmoveNumber = 1;
  uint64_t zobrist = 0;

  Position() { for (auto& m : mailbox) m = -1; }

  void setStartpos();
  void setFromFEN(const std::string& fen);
  std::string toFEN() const;

  bool empty(Square s) const { return mailbox[s] < 0; }
  int pieceAt(Square s) const { return mailbox[s]; } // -1 or 0..11
};

struct Undo {
  int capturedIndex = -1;
  Square capturedSquare = -1;
  uint8_t castling = 0;
  int epSquare = -1;
  int halfmoveClock = 0;
  uint64_t zobrist = 0;
};

void makeMove(Position& pos, const Move& m, Undo& undo);
void unmakeMove(Position& pos, const Move& m, const Undo& undo);

bool isSquareAttacked(const Position& pos, Square sq, Color byColor);
inline bool inCheck(const Position& pos, Color c) {
  Bitboard kingBB = pos.pieceBB[pieceIndex(c, KING)];
  if (!kingBB) return false;
  return isSquareAttacked(pos, lsb(kingBB), Color(1 - c));
}

std::string moveToUci(const Move& m);
