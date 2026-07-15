#include "position.hpp"
#include "magic.hpp"
#include <sstream>
#include <cctype>

namespace zobrist {
uint64_t pieceKey[12][64];
uint64_t sideKey;
uint64_t castleKey[16];
uint64_t epFileKey[8];

namespace {
uint64_t rngState = 0x2545F4914F6CDD1DULL;
uint64_t nextRand() {
  uint64_t x = rngState;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  rngState = x;
  return x;
}
} // namespace

void init() {
  for (int p = 0; p < 12; p++)
    for (int s = 0; s < 64; s++)
      pieceKey[p][s] = nextRand();
  sideKey = nextRand();
  for (int i = 0; i < 16; i++) castleKey[i] = nextRand();
  for (int i = 0; i < 8; i++) epFileKey[i] = nextRand();
}
} // namespace zobrist

namespace {

void removePiece(Position& pos, Color c, PieceType t, Square s) {
  int idx = pieceIndex(c, t);
  pos.pieceBB[idx] &= ~sqBB(s);
  pos.occByColor[c] &= ~sqBB(s);
  pos.occAll &= ~sqBB(s);
  pos.mailbox[s] = -1;
  pos.zobrist ^= zobrist::pieceKey[idx][s];
}

void addPiece(Position& pos, Color c, PieceType t, Square s) {
  int idx = pieceIndex(c, t);
  pos.pieceBB[idx] |= sqBB(s);
  pos.occByColor[c] |= sqBB(s);
  pos.occAll |= sqBB(s);
  pos.mailbox[s] = int8_t(idx);
  pos.zobrist ^= zobrist::pieceKey[idx][s];
}

void movePieceBB(Position& pos, Color c, PieceType t, Square from, Square to) {
  removePiece(pos, c, t, from);
  addPiece(pos, c, t, to);
}

uint64_t computeZobristFromScratch(const Position& pos) {
  uint64_t z = 0;
  for (int s = 0; s < 64; s++) {
    int idx = pos.mailbox[s];
    if (idx >= 0) z ^= zobrist::pieceKey[idx][s];
  }
  if (pos.sideToMove == BLACK) z ^= zobrist::sideKey;
  z ^= zobrist::castleKey[pos.castling];
  if (pos.epSquare >= 0) z ^= zobrist::epFileKey[fileOf(pos.epSquare)];
  return z;
}

} // namespace

void Position::setStartpos() {
  setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Position::setFromFEN(const std::string& fen) {
  *this = Position{};

  std::istringstream iss(fen);
  std::string boardPart, sideText, castleText, epText;
  int half = 0, full = 1;
  iss >> boardPart >> sideText >> castleText >> epText;
  if (!(iss >> half)) half = 0;
  if (!(iss >> full)) full = 1;

  int rank = 7, file = 0;
  for (char c : boardPart) {
    if (c == '/') {
      rank--;
      file = 0;
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
      file += c - '0';
    } else {
      Color color = std::isupper(static_cast<unsigned char>(c)) ? WHITE : BLACK;
      PieceType type;
      switch (std::tolower(static_cast<unsigned char>(c))) {
        case 'p': type = PAWN; break;
        case 'n': type = KNIGHT; break;
        case 'b': type = BISHOP; break;
        case 'r': type = ROOK; break;
        case 'q': type = QUEEN; break;
        case 'k': type = KING; break;
        default: type = PAWN; break;
      }
      Square sq = makeSquare(rank, file);
      int idx = pieceIndex(color, type);
      pieceBB[idx] |= sqBB(sq);
      occByColor[color] |= sqBB(sq);
      occAll |= sqBB(sq);
      mailbox[sq] = int8_t(idx);
      file++;
    }
  }

  sideToMove = (sideText == "w") ? WHITE : BLACK;

  castling = 0;
  for (char c : castleText) {
    if (c == 'K') castling |= CASTLE_WK;
    if (c == 'Q') castling |= CASTLE_WQ;
    if (c == 'k') castling |= CASTLE_BK;
    if (c == 'q') castling |= CASTLE_BQ;
  }

  epSquare = (epText == "-" || epText.empty()) ? -1 : algebraicToSquare(epText);
  halfmoveClock = half;
  fullmoveNumber = full;

  zobrist = computeZobristFromScratch(*this);
}

std::string Position::toFEN() const {
  std::ostringstream out;
  for (int rank = 7; rank >= 0; rank--) {
    int emptyRun = 0;
    for (int file = 0; file < 8; file++) {
      Square sq = makeSquare(rank, file);
      int idx = mailbox[sq];
      if (idx < 0) {
        emptyRun++;
        continue;
      }
      if (emptyRun) { out << emptyRun; emptyRun = 0; }
      static const char* names[12] = {"P","N","B","R","Q","K","p","n","b","r","q","k"};
      out << names[idx];
    }
    if (emptyRun) out << emptyRun;
    if (rank > 0) out << '/';
  }
  out << (sideToMove == WHITE ? " w " : " b ");
  std::string cr;
  if (castling & CASTLE_WK) cr += 'K';
  if (castling & CASTLE_WQ) cr += 'Q';
  if (castling & CASTLE_BK) cr += 'k';
  if (castling & CASTLE_BQ) cr += 'q';
  out << (cr.empty() ? "-" : cr) << ' ';
  out << (epSquare < 0 ? "-" : squareToAlgebraic(epSquare)) << ' ';
  out << halfmoveClock << ' ' << fullmoveNumber;
  return out.str();
}

bool isSquareAttacked(const Position& pos, Square sq, Color byColor) {
  Bitboard occ = pos.occAll;
  bool white = (byColor == WHITE);

  // Pawns: flipping the "white" flag gives the "which squares attack sq"
  // pattern (a white pawn attacking sq sits where a black pawn placed on sq
  // would itself attack, and vice versa) — the standard bitboard trick.
  if (magic::pawnAttacks(sq, !white) & pos.pieceBB[pieceIndex(byColor, PAWN)]) return true;
  if (magic::knightAttacks(sq) & pos.pieceBB[pieceIndex(byColor, KNIGHT)]) return true;
  if (magic::kingAttacks(sq) & pos.pieceBB[pieceIndex(byColor, KING)]) return true;

  Bitboard rq = pos.pieceBB[pieceIndex(byColor, ROOK)] | pos.pieceBB[pieceIndex(byColor, QUEEN)];
  if (magic::rookAttacks(sq, occ) & rq) return true;

  Bitboard bq = pos.pieceBB[pieceIndex(byColor, BISHOP)] | pos.pieceBB[pieceIndex(byColor, QUEEN)];
  if (magic::bishopAttacks(sq, occ) & bq) return true;

  return false;
}

void makeMove(Position& pos, const Move& m, Undo& undo) {
  Color us = pos.sideToMove, them = Color(1 - us);
  int movingIdx = pos.mailbox[m.from];
  PieceType movingType = typeOf(movingIdx);

  undo.capturedIndex = -1;
  undo.capturedSquare = -1;
  undo.castling = pos.castling;
  undo.epSquare = pos.epSquare;
  undo.halfmoveClock = pos.halfmoveClock;
  undo.zobrist = pos.zobrist;

  if (pos.epSquare >= 0) pos.zobrist ^= zobrist::epFileKey[fileOf(pos.epSquare)];
  pos.zobrist ^= zobrist::castleKey[pos.castling];

  if (m.isEP) {
    Square capSq = (us == WHITE) ? m.to - 8 : m.to + 8;
    undo.capturedIndex = pos.mailbox[capSq];
    undo.capturedSquare = capSq;
    removePiece(pos, them, PAWN, capSq);
  } else if (m.isCapture) {
    undo.capturedIndex = pos.mailbox[m.to];
    undo.capturedSquare = m.to;
    removePiece(pos, them, typeOf(undo.capturedIndex), m.to);
  }

  movePieceBB(pos, us, movingType, m.from, m.to);

  if (m.promo != NO_PIECE_TYPE) {
    removePiece(pos, us, PAWN, m.to);
    addPiece(pos, us, m.promo, m.to);
  }

  if (m.isCastle) {
    int rank = (us == WHITE) ? 0 : 7;
    if (fileOf(m.to) == 6) movePieceBB(pos, us, ROOK, makeSquare(rank, 7), makeSquare(rank, 5));
    else                   movePieceBB(pos, us, ROOK, makeSquare(rank, 0), makeSquare(rank, 3));
  }

  uint8_t newCastling = pos.castling;
  if (movingType == KING) {
    newCastling &= (us == WHITE) ? uint8_t(~(CASTLE_WK | CASTLE_WQ)) : uint8_t(~(CASTLE_BK | CASTLE_BQ));
  }
  auto clearRightForSquare = [&](Square s) {
    if (s == 0)  newCastling &= ~CASTLE_WQ;
    if (s == 7)  newCastling &= ~CASTLE_WK;
    if (s == 56) newCastling &= ~CASTLE_BQ;
    if (s == 63) newCastling &= ~CASTLE_BK;
  };
  clearRightForSquare(m.from);
  clearRightForSquare(m.to);
  pos.castling = newCastling;

  pos.epSquare = -1;
  if (m.isDoublePush) {
    pos.epSquare = (us == WHITE) ? m.from + 8 : m.from - 8;
  }

  if (movingType == PAWN || m.isCapture) pos.halfmoveClock = 0;
  else pos.halfmoveClock++;

  if (us == BLACK) pos.fullmoveNumber++;

  pos.sideToMove = them;
  pos.zobrist ^= zobrist::sideKey;
  pos.zobrist ^= zobrist::castleKey[pos.castling];
  if (pos.epSquare >= 0) pos.zobrist ^= zobrist::epFileKey[fileOf(pos.epSquare)];
}

void unmakeMove(Position& pos, const Move& m, const Undo& undo) {
  Color us = (pos.sideToMove == WHITE) ? BLACK : WHITE; // side that made this move
  Color them = pos.sideToMove;
  pos.sideToMove = us;

  if (m.isCastle) {
    int rank = (us == WHITE) ? 0 : 7;
    if (fileOf(m.to) == 6) movePieceBB(pos, us, ROOK, makeSquare(rank, 5), makeSquare(rank, 7));
    else                   movePieceBB(pos, us, ROOK, makeSquare(rank, 3), makeSquare(rank, 0));
  }

  if (m.promo != NO_PIECE_TYPE) {
    removePiece(pos, us, m.promo, m.to);
    addPiece(pos, us, PAWN, m.from);
  } else {
    movePieceBB(pos, us, typeOf(pos.mailbox[m.to]), m.to, m.from);
  }

  if (m.isEP) {
    addPiece(pos, them, PAWN, undo.capturedSquare);
  } else if (undo.capturedIndex >= 0) {
    addPiece(pos, them, typeOf(undo.capturedIndex), m.to);
  }

  pos.castling = undo.castling;
  pos.epSquare = undo.epSquare;
  pos.halfmoveClock = undo.halfmoveClock;
  pos.zobrist = undo.zobrist; // snapshot restore — simpler and foolproof vs. reversing XORs
  if (us == BLACK) pos.fullmoveNumber--;
}

std::string moveToUci(const Move& m) {
  std::string s = squareToAlgebraic(m.from) + squareToAlgebraic(m.to);
  if (m.promo != NO_PIECE_TYPE) {
    static const char promoChar[6] = {'p', 'n', 'b', 'r', 'q', 'k'};
    s += promoChar[m.promo];
  }
  return s;
}
