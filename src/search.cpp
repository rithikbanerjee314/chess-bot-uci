// Negamax with the same enhancement stack already validated (via self-play:
// +497 Elo over the pre-rewrite version) in the JS engine this project
// started from: TT, null-move pruning, reverse futility pruning, futility
// pruning, late-move reductions, PVS, aspiration windows, mate-distance
// pruning, killer/history move ordering, check extensions, and
// repetition detection. Reimplemented against Position's make/unmake
// (no board copies) and a real SEE (bitboard swap algorithm) instead of
// JS's from-scratch scratch-board version.
#include "search.hpp"
#include "movegen.hpp"
#include "eval.hpp"
#include "tt.hpp"
#include "magic.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace search {
namespace {

using Clock = std::chrono::steady_clock;

constexpr int MATE_SCORE = eval::MATE_SCORE;
constexpr int NEG_INF = -1000000;
constexpr int QMAX = 6;
constexpr int MAX_PLY = 128;

struct TimeoutException {};

TranspositionTable g_tt(256); // CCRL's suggested default
Move g_killers[MAX_PLY][2];
int g_history[12][64];

std::uint64_t g_nodeCount = 0;
Clock::time_point g_hardDeadline;
std::atomic<bool>* g_stopFlag = nullptr;

std::unordered_set<std::uint64_t> g_prevKeySet;
std::vector<std::uint64_t> g_pathKeys;

struct PathGuard {
  std::vector<std::uint64_t>& path;
  PathGuard(std::vector<std::uint64_t>& p, std::uint64_t key) : path(p) { path.push_back(key); }
  ~PathGuard() { path.pop_back(); }
};

struct NullUndo { int epSquare; std::uint64_t zobrist; };

void makeNullMove(Position& pos, NullUndo& u) {
  u.epSquare = pos.epSquare;
  u.zobrist = pos.zobrist;
  if (pos.epSquare >= 0) pos.zobrist ^= zobrist::epFileKey[fileOf(pos.epSquare)];
  pos.epSquare = -1;
  pos.sideToMove = Color(1 - pos.sideToMove);
  pos.zobrist ^= zobrist::sideKey;
}
void unmakeNullMove(Position& pos, const NullUndo& u) {
  pos.sideToMove = Color(1 - pos.sideToMove);
  pos.epSquare = u.epSquare;
  pos.zobrist = u.zobrist;
}

bool timeUp() {
  if (g_stopFlag && g_stopFlag->load(std::memory_order_relaxed)) return true;
  return Clock::now() >= g_hardDeadline;
}

void checkTime() {
  if ((++g_nodeCount & 1023) == 0 && timeUp()) throw TimeoutException{};
}

bool hasNonPawnMaterial(const Position& pos, Color c) {
  return pos.pieceBB[pieceIndex(c, KNIGHT)] || pos.pieceBB[pieceIndex(c, BISHOP)] ||
         pos.pieceBB[pieceIndex(c, ROOK)] || pos.pieceBB[pieceIndex(c, QUEEN)];
}

// ---- Static exchange evaluation ----
Square leastValuableAttacker(const Position& pos, Bitboard occ, Square sq, Color byColor, PieceType& outType) {
  Bitboard pawnAtk = magic::pawnAttacks(sq, byColor != WHITE) & pos.pieceBB[pieceIndex(byColor, PAWN)] & occ;
  if (pawnAtk) { outType = PAWN; return lsb(pawnAtk); }
  Bitboard knightAtk = magic::knightAttacks(sq) & pos.pieceBB[pieceIndex(byColor, KNIGHT)] & occ;
  if (knightAtk) { outType = KNIGHT; return lsb(knightAtk); }
  Bitboard bishopAtk = magic::bishopAttacks(sq, occ) & pos.pieceBB[pieceIndex(byColor, BISHOP)] & occ;
  if (bishopAtk) { outType = BISHOP; return lsb(bishopAtk); }
  Bitboard rookAtk = magic::rookAttacks(sq, occ) & pos.pieceBB[pieceIndex(byColor, ROOK)] & occ;
  if (rookAtk) { outType = ROOK; return lsb(rookAtk); }
  Bitboard queenAtk = magic::queenAttacks(sq, occ) & pos.pieceBB[pieceIndex(byColor, QUEEN)] & occ;
  if (queenAtk) { outType = QUEEN; return lsb(queenAtk); }
  Bitboard kingAtk = magic::kingAttacks(sq) & pos.pieceBB[pieceIndex(byColor, KING)] & occ;
  if (kingAtk) { outType = KING; return lsb(kingAtk); }
  return -1;
}

// Net material outcome (side-to-move POV) of the swap on `to`. Standard
// swap algorithm: alternate cheapest-attacker recaptures on a shrinking
// occupancy bitboard, then negamax the resulting gain list. X-rays emerge
// naturally since removing an attacker from `occ` exposes sliders behind it.
int seeCapture(const Position& pos, Square from, Square to, bool isEP) {
  Bitboard occ = pos.occAll;
  Color side = colorOf(pos.mailbox[from]);
  PieceType occupantType = typeOf(pos.mailbox[from]);

  int gain[32];
  int d = 0;
  PieceType capturedType = isEP ? PAWN : (pos.mailbox[to] >= 0 ? typeOf(pos.mailbox[to]) : NO_PIECE_TYPE);
  gain[0] = (capturedType != NO_PIECE_TYPE) ? eval::pieceValueMG(capturedType) : 0;

  if (isEP) {
    Square epCapSq = (side == WHITE) ? to - 8 : to + 8;
    occ &= ~sqBB(epCapSq);
  }
  occ &= ~sqBB(from);
  side = Color(1 - side);

  while (true) {
    PieceType attackerType;
    Square attackerSq = leastValuableAttacker(pos, occ, to, side, attackerType);
    if (attackerSq < 0) break;
    d++;
    gain[d] = eval::pieceValueMG(occupantType) - gain[d - 1];
    occ &= ~sqBB(attackerSq);
    occupantType = attackerType;
    side = Color(1 - side);
    if (d >= 30) break; // safety cap, never realistically reached
  }
  while (d > 0) {
    gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
    d--;
  }
  return gain[0];
}

// ---- Lazy evaluation ----
int evaluateLazy(const Position& pos, int alpha, int beta) {
  constexpr int LAZY_MARGIN = 380;
  int fastWhitePov = eval::fastEval(pos, true);
  int fastStm = (pos.sideToMove == WHITE) ? fastWhitePov : -fastWhitePov;
  if (fastStm + LAZY_MARGIN <= alpha || fastStm - LAZY_MARGIN >= beta) return fastStm;
  int fullWhitePov = eval::evaluate(pos, true);
  return (pos.sideToMove == WHITE) ? fullWhitePov : -fullWhitePov;
}

// ---- Move ordering: TT move, then MVV-LVA captures / EP / promotions, then killers/history ----
int moveOrderScore(const Position& pos, const Move& mv, const Move& ttMove, int ply) {
  int pieceIdx = pos.mailbox[mv.from];
  if (pieceIdx < 0) return 0;
  PieceType t = typeOf(pieceIdx);
  bool white = colorOf(pieceIdx) == WHITE;

  if (!ttMove.isNull() && mv == ttMove) return 1000000000;

  int s = 0;
  if (mv.isCapture && !mv.isEP) {
    PieceType capType = typeOf(pos.mailbox[mv.to]);
    s += 100000 + 10 * eval::pieceValueMG(capType) - eval::pieceValueMG(t);
  } else if (mv.isEP) {
    s += 100100;
  } else if (mv.promo == NO_PIECE_TYPE) {
    const Move* kil = g_killers[ply];
    if (!kil[0].isNull() && kil[0] == mv) s += 90000;
    else if (!kil[1].isNull() && kil[1] == mv) s += 80000;
    else s += g_history[pieceIdx][mv.to];
  }
  if (mv.promo != NO_PIECE_TYPE) s += 95000 + eval::pieceValueMG(mv.promo);
  s += eval::pstValue(t, white, mv.to, eval::MAX_PHASE) - eval::pstValue(t, white, mv.from, eval::MAX_PHASE);
  return s;
}

void orderMoves(const Position& pos, std::vector<Move>& moves, const Move& ttMove, int ply) {
  std::vector<std::pair<int, Move>> scored;
  scored.reserve(moves.size());
  for (auto& m : moves) scored.emplace_back(moveOrderScore(pos, m, ttMove, ply), m);
  std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  for (std::size_t i = 0; i < moves.size(); i++) moves[i] = scored[i].second;
}

// ---- Quiescence: captures+promotions when quiet, all evasions when in check ----
int quiesce(Position& pos, int alpha, int beta, int qdepth, int ply) {
  checkTime();
  bool inCheckNow = inCheck(pos, pos.sideToMove);

  if (inCheckNow) {
    if (qdepth <= -3) {
      int full = eval::evaluate(pos, true);
      return (pos.sideToMove == WHITE) ? full : -full;
    }
    std::vector<Move> evasions;
    generateLegalMoves(pos, evasions);
    if (evasions.empty()) return -(MATE_SCORE - ply);
    orderMoves(pos, evasions, Move{}, 0);
    int best = NEG_INF;
    for (const Move& mv : evasions) {
      Undo u;
      makeMove(pos, mv, u);
      int score = -quiesce(pos, -beta, -alpha, qdepth - 1, ply + 1);
      unmakeMove(pos, mv, u);
      if (score > best) best = score;
      if (best >= beta) return best;
      if (best > alpha) alpha = best;
    }
    return best;
  }

  int standPat = evaluateLazy(pos, alpha, beta);
  if (standPat >= beta) return standPat;
  int best = standPat;
  if (standPat > alpha) alpha = standPat;
  if (qdepth <= 0) return best;

  std::vector<Move> moves;
  generateLegalMoves(pos, moves);
  std::vector<Move> captures;
  captures.reserve(moves.size());
  for (auto& m : moves) if (m.isCapture || m.promo != NO_PIECE_TYPE) captures.push_back(m);
  if (captures.empty()) return best;

  orderMoves(pos, captures, Move{}, 0);
  constexpr int DELTA_MARGIN = 200;
  for (const Move& mv : captures) {
    int targetVal = 0;
    if (mv.isCapture) {
      PieceType capType = mv.isEP ? PAWN : typeOf(pos.mailbox[mv.to]);
      targetVal = eval::pieceValueMG(capType);
    }
    int gain = targetVal + (mv.promo != NO_PIECE_TYPE ? eval::pieceValueMG(mv.promo) - eval::pieceValueMG(PAWN) : 0);
    if (standPat + gain + DELTA_MARGIN < alpha) continue;
    if (mv.promo == NO_PIECE_TYPE && seeCapture(pos, mv.from, mv.to, mv.isEP) < 0) continue;

    Undo u;
    makeMove(pos, mv, u);
    int score = -quiesce(pos, -beta, -alpha, qdepth - 1, ply + 1);
    unmakeMove(pos, mv, u);
    if (score > best) best = score;
    if (best >= beta) return best;
    if (best > alpha) alpha = best;
  }
  return best;
}

// ---- Main negamax: TT / NMP / RFP / futility / LMR / PVS / check ext. / mate-distance / repetition ----
int negamax(Position& pos, int depth, int alpha, int beta, int ply, bool allowNull) {
  checkTime();

  std::uint64_t hash = pos.zobrist;
  if (ply > 0 && (g_prevKeySet.count(hash) ||
      std::find(g_pathKeys.begin(), g_pathKeys.end(), hash) != g_pathKeys.end())) {
    return 0;
  }

  PathGuard guard(g_pathKeys, hash);

  bool inCheckNow = inCheck(pos, pos.sideToMove);
  if (inCheckNow) depth += 1;

  int alphaMD = std::max(alpha, -MATE_SCORE + ply);
  int betaMD = std::min(beta, MATE_SCORE - ply);
  if (alphaMD >= betaMD) return alphaMD;
  alpha = alphaMD; beta = betaMD;

  if (depth <= 0) return quiesce(pos, alpha, beta, QMAX, ply);

  const TTEntry* tt = g_tt.probe(hash);
  Move ttMove;
  if (tt) {
    ttMove = tt->move;
    if (tt->depth >= depth) {
      int v = tt->score;
      if (tt->bound == BOUND_EXACT) return v;
      if (tt->bound == BOUND_LOWER && v >= beta) return v;
      if (tt->bound == BOUND_UPPER && v <= alpha) return v;
    }
  }

  std::vector<Move> moves;
  generateLegalMoves(pos, moves);
  if (moves.empty()) return inCheckNow ? -(MATE_SCORE - ply) : 0;

  bool haveStatic = false;
  int staticEval = 0;
  if (!inCheckNow) {
    staticEval = evaluateLazy(pos, alpha, beta);
    haveStatic = true;
  }

  if (haveStatic && depth <= 4 && std::abs(beta) < MATE_SCORE - 100) {
    if (staticEval - 120 * depth >= beta) return staticEval;
  }

  if (allowNull && !inCheckNow && depth >= 3 && haveStatic && staticEval >= beta &&
      hasNonPawnMaterial(pos, pos.sideToMove)) {
    int R = (depth >= 6) ? 3 : 2;
    NullUndo nu;
    makeNullMove(pos, nu);
    int score = -negamax(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, false);
    unmakeNullMove(pos, nu);
    if (score >= beta) return score;
  }

  orderMoves(pos, moves, ttMove, ply);

  int best = NEG_INF;
  Move bestMove = moves[0];
  Bound bound = BOUND_UPPER;
  int moveIdx = 0;

  int fpEval = (depth <= 2 && haveStatic) ? staticEval : NEG_INF;

  for (const Move& mv : moves) {
    bool isQuiet = !mv.isCapture && mv.promo == NO_PIECE_TYPE;

    if (fpEval != NEG_INF && isQuiet && moveIdx > 0) {
      int fpMargin = (depth == 1) ? 300 : 500;
      if (fpEval + fpMargin <= alpha) continue;
    }

    Undo u;
    makeMove(pos, mv, u);
    int score;
    if (moveIdx == 0) {
      score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, true);
    } else {
      int reduction = 0;
      if (depth >= 3 && isQuiet && !inCheckNow && moveIdx >= 3) {
        reduction = 1 + static_cast<int>(std::log2(double(depth)) * std::log2(double(moveIdx)) / 2.5);
        if (reduction > depth - 2) reduction = depth - 2;
        if (reduction < 0) reduction = 0;
      }
      score = -negamax(pos, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, true);
      if (score > alpha && (reduction > 0 || score < beta)) {
        score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, true);
      }
    }
    unmakeMove(pos, mv, u);

    if (score > best) { best = score; bestMove = mv; }
    if (best > alpha) { alpha = best; bound = BOUND_EXACT; }
    if (alpha >= beta) {
      bound = BOUND_LOWER;
      if (isQuiet) {
        Move* kil = g_killers[ply];
        if (kil[0].isNull() || !(kil[0] == mv)) { kil[1] = kil[0]; kil[0] = mv; }
        int pieceIdx = pos.mailbox[mv.from];
        if (pieceIdx >= 0) g_history[pieceIdx][mv.to] += depth * depth;
      }
      break;
    }
    moveIdx++;
  }

  g_tt.store(hash, depth, best, bound, bestMove);
  return best;
}

} // namespace

void newGame() {
  g_tt.clear();
  for (auto& row : g_killers) { row[0] = Move{}; row[1] = Move{}; }
  for (auto& row : g_history) for (auto& v : row) v = 0;
}

void setHashSizeMB(std::size_t mb) { g_tt.resize(mb); }

SearchResult think(Position& pos, int maxDepth, long long hardMs, long long softMs,
                    const std::vector<std::uint64_t>& prevKeys, std::atomic<bool>& stopFlag) {
  SearchResult result;
  std::vector<Move> moves;
  generateLegalMoves(pos, moves);
  if (moves.empty()) return result;

  if (moves.size() == 1) {
    result.bestMove = moves[0];
    result.scoreWhitePov = eval::evaluate(pos, true);
    result.depth = 0;
    return result;
  }

  g_nodeCount = 0;
  g_hardDeadline = Clock::now() + std::chrono::milliseconds(hardMs);
  auto softDeadline = Clock::now() + std::chrono::milliseconds(softMs);
  g_stopFlag = &stopFlag;

  for (auto& row : g_killers) { row[0] = Move{}; row[1] = Move{}; }
  for (auto& row : g_history) for (auto& v : row) v /= 2;

  g_prevKeySet.clear();
  for (auto k : prevKeys) g_prevKeySet.insert(k);
  g_pathKeys.clear();

  Move bestMove = moves[0];
  int bestScoreStm = NEG_INF;
  int prevScoreStm = 0;
  int lastCompletedDepth = 0;

  for (int depth = 1; depth <= maxDepth; depth++) {
    orderMoves(pos, moves, bestMove, 0);

    Move iterBest = moves[0];
    int iterScore = NEG_INF;
    bool timedOut = false;

    int lo = (depth == 1) ? NEG_INF : prevScoreStm - 50;
    int hi = (depth == 1) ? -NEG_INF : prevScoreStm + 50;
    int attempt = 0;

    for (;;) {
      iterBest = moves[0];
      iterScore = NEG_INF;
      int alpha = lo, beta = hi;
      int mvIdx = 0;
      try {
        for (const Move& mv : moves) {
          Undo u;
          makeMove(pos, mv, u);
          int score;
          if (mvIdx == 0) {
            score = -negamax(pos, depth - 1, -beta, -alpha, 1, true);
          } else {
            score = -negamax(pos, depth - 1, -alpha - 1, -alpha, 1, true);
            if (score > alpha && score < beta) {
              score = -negamax(pos, depth - 1, -beta, -alpha, 1, true);
            }
          }
          unmakeMove(pos, mv, u);
          if (score > iterScore) { iterScore = score; iterBest = mv; }
          if (iterScore > alpha) alpha = iterScore;
          mvIdx++;
        }
      } catch (const TimeoutException&) {
        timedOut = true;
        break;
      }

      if (iterScore <= lo) { lo = NEG_INF; attempt++; if (attempt < 2) continue; }
      else if (iterScore >= hi) { hi = -NEG_INF; attempt++; if (attempt < 2) continue; }
      break;
    }

    if (!timedOut) {
      bestMove = iterBest;
      bestScoreStm = iterScore;
      prevScoreStm = iterScore;
      lastCompletedDepth = depth;
      auto it = std::find(moves.begin(), moves.end(), iterBest);
      if (it != moves.begin() && it != moves.end()) {
        Move mv = *it;
        moves.erase(it);
        moves.insert(moves.begin(), mv);
      }
    }

    if (timedOut) break;
    if (Clock::now() > softDeadline) break;
    if (stopFlag.load(std::memory_order_relaxed)) break;
  }

  result.bestMove = bestMove;
  result.scoreWhitePov = (pos.sideToMove == WHITE) ? bestScoreStm : -bestScoreStm;
  result.depth = lastCompletedDepth;
  result.nodes = g_nodeCount;
  return result;
}

} // namespace search
