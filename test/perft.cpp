// Perft correctness harness — the critical gate for a bitboard rewrite,
// since move-generation bugs are the most common and most silently
// corrupting bug class in a chess engine. Verifies move generation against
// known-good node counts for standard test positions.
#include "../src/position.hpp"
#include "../src/movegen.hpp"
#include "../src/magic.hpp"
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

static std::uint64_t perft(Position& pos, int depth) {
  if (depth == 0) return 1;
  std::vector<Move> moves;
  generateLegalMoves(pos, moves);
  if (depth == 1) return moves.size();
  std::uint64_t nodes = 0;
  for (const Move& m : moves) {
    Undo u;
    makeMove(pos, m, u);
    nodes += perft(pos, depth - 1);
    unmakeMove(pos, m, u);
  }
  return nodes;
}

struct TestCase {
  const char* name;
  const char* fen;
  std::vector<std::uint64_t> expected; // depth 1, 2, 3, ...
};

int main() {
  zobrist::init();
  magic::init();

  std::vector<TestCase> cases = {
    {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     {20, 400, 8902, 197281}},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     {48, 2039, 97862}},
    {"cpw-pos4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     {6, 264, 9467}},
  };

  bool allPass = true;
  for (auto& tc : cases) {
    Position pos;
    pos.setFromFEN(tc.fen);
    for (size_t d = 0; d < tc.expected.size(); d++) {
      Position work = pos;
      std::uint64_t got = perft(work, static_cast<int>(d + 1));
      bool ok = (got == tc.expected[d]);
      allPass &= ok;
      std::printf("perft %-10s d%zu: %10llu (expect %10llu) %s\n",
                  tc.name, d + 1,
                  static_cast<unsigned long long>(got),
                  static_cast<unsigned long long>(tc.expected[d]),
                  ok ? "OK" : "MISMATCH");
    }
  }

  std::printf("\n%s\n", allPass ? "ALL PERFT TESTS PASSED" : "PERFT TESTS FAILED");
  return allPass ? 0 : 1;
}
