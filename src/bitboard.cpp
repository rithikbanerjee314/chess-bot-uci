#include "bitboard.hpp"
#include <sstream>

std::string squareToAlgebraic(Square s) {
  std::string out;
  out += static_cast<char>('a' + fileOf(s));
  out += static_cast<char>('1' + rankOf(s));
  return out;
}

Square algebraicToSquare(const std::string& s) {
  int file = s[0] - 'a';
  int rank = s[1] - '1';
  return makeSquare(rank, file);
}

std::string bitboardToString(Bitboard b) {
  std::ostringstream out;
  for (int rank = 7; rank >= 0; rank--) {
    for (int file = 0; file < 8; file++) {
      out << ((b & sqBB(makeSquare(rank, file))) ? '1' : '.');
      out << ' ';
    }
    out << '\n';
  }
  return out.str();
}
