#include "uci.hpp"
#include "position.hpp"
#include "movegen.hpp"
#include "search.hpp"
#include "eval.hpp"
#include <atomic>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

const char* ENGINE_NAME = "Atlas 1.0.0";
const char* ENGINE_AUTHOR = "Rithik Banerjee";
constexpr int DEFAULT_MAX_DEPTH = 60; // headroom under search's MAX_PLY=128
constexpr long long INFINITE_GO_MS = 10 * 60 * 1000;
constexpr int DEFAULT_HASH_MB = 256;
constexpr int MIN_HASH_MB = 1;
constexpr int MAX_HASH_MB = 4096;

Position g_pos;
std::vector<std::uint64_t> g_gameKeys;

std::thread g_searchThread;
std::atomic<bool> g_stopFlag{false};

void send(const std::string& line) {
  std::cout << line << "\n" << std::flush;
}

void stopAndJoinIfRunning() {
  if (g_searchThread.joinable()) {
    g_stopFlag.store(true, std::memory_order_relaxed);
    g_searchThread.join();
  }
}

void setStartpos() {
  g_pos.setStartpos();
  g_gameKeys.clear();
  g_gameKeys.push_back(g_pos.zobrist);
}

void setFen(const std::vector<std::string>& fenTokens) {
  std::string fen;
  for (std::size_t i = 0; i < fenTokens.size(); i++) {
    if (i) fen += ' ';
    fen += fenTokens[i];
  }
  g_pos.setFromFEN(fen);
  g_gameKeys.clear();
  g_gameKeys.push_back(g_pos.zobrist);
}

void applyUciMove(const std::string& moveStr) {
  if (moveStr.size() < 4) return;
  Square from = algebraicToSquare(moveStr.substr(0, 2));
  Square to = algebraicToSquare(moveStr.substr(2, 2));
  PieceType promo = NO_PIECE_TYPE;
  if (moveStr.size() > 4) {
    switch (moveStr[4]) {
      case 'q': promo = QUEEN; break;
      case 'r': promo = ROOK; break;
      case 'b': promo = BISHOP; break;
      case 'n': promo = KNIGHT; break;
      default: break;
    }
  }
  std::vector<Move> legal;
  generateLegalMoves(g_pos, legal);
  for (auto& m : legal) {
    if (m.from == from && m.to == to && m.promo == promo) {
      Undo u;
      makeMove(g_pos, m, u);
      g_gameKeys.push_back(g_pos.zobrist);
      return;
    }
  }
  // Not a legal move for the current position — ignore (defensive; a
  // conformant GUI won't send one, but silently ignoring beats crashing).
}

void handlePosition(std::istringstream& iss) {
  stopAndJoinIfRunning();
  std::string tok;
  iss >> tok;
  if (tok == "startpos") {
    setStartpos();
    iss >> tok; // consumes "moves" if present, else leaves tok stale/empty
  } else if (tok == "fen") {
    std::vector<std::string> fenTokens;
    while (iss >> tok && tok != "moves") fenTokens.push_back(tok);
    setFen(fenTokens);
  } else {
    return;
  }
  if (tok == "moves") {
    while (iss >> tok) applyUciMove(tok);
  }
}

struct GoParams {
  bool haveDepth = false;
  int depth = 0;
  bool haveMovetime = false;
  long long movetime = 0;
  bool haveWtime = false, haveBtime = false;
  long long wtime = 0, btime = 0, winc = 0, binc = 0;
  int movestogo = 0;
  bool infinite = false;
};

// Soft/hard time split: soft stops iterative deepening from STARTING a new
// depth (a started-but-unfinished iteration is discarded, so time spent
// starting one late is mostly wasted); hard aborts mid-iteration. For
// wtime/btime, reserves a fixed per-move overhead off the top of the
// remaining clock — the in-search deadline check is coarse (every 1024
// nodes), so a budget computed purely from remaining time can be
// overshot by whatever fixed cost isn't visible to node counting.
void computeBudget(const GoParams& gp, Color stm, int& maxDepth, long long& hardMs, long long& softMs) {
  if (gp.haveDepth) {
    maxDepth = gp.depth;
    hardMs = 24LL * 60 * 60 * 1000;
    softMs = hardMs;
    return;
  }
  if (gp.haveMovetime) {
    long long ms = std::max<long long>(50, gp.movetime - 20);
    maxDepth = DEFAULT_MAX_DEPTH;
    hardMs = ms;
    softMs = ms;
    return;
  }
  if (gp.haveWtime || gp.haveBtime) {
    long long myTime = (stm == WHITE) ? gp.wtime : gp.btime;
    long long myInc = (stm == WHITE) ? gp.winc : gp.binc;
    int movesToGo = gp.movestogo > 0 ? gp.movestogo : 25;
    constexpr long long OVERHEAD_MS = 30;
    long long usable = std::max<long long>(0, myTime - OVERHEAD_MS);
    double soft = double(usable) / movesToGo + double(myInc) * 0.8;
    soft = std::min(soft, usable * 0.3);
    soft = std::max(soft, 20.0);
    double hard = std::min(soft * 3.0, usable * 0.6);
    hard = std::max(hard, 25.0);
    hard = std::min(hard, double(usable));
    soft = std::min(soft, hard);
    maxDepth = DEFAULT_MAX_DEPTH;
    hardMs = static_cast<long long>(hard);
    softMs = static_cast<long long>(soft);
    return;
  }
  if (gp.infinite) {
    maxDepth = DEFAULT_MAX_DEPTH;
    hardMs = INFINITE_GO_MS;
    softMs = INFINITE_GO_MS;
    return;
  }
  maxDepth = 12;
  hardMs = 1000;
  softMs = 1000;
}

void searchThreadFunc(GoParams gp) {
  int maxDepth;
  long long hardMs, softMs;
  computeBudget(gp, g_pos.sideToMove, maxDepth, hardMs, softMs);

  search::SearchResult res = search::think(g_pos, maxDepth, hardMs, softMs, g_gameKeys, g_stopFlag);

  if (res.bestMove.isNull()) {
    send("bestmove 0000");
  } else {
    int scoreStm = (g_pos.sideToMove == WHITE) ? res.scoreWhitePov : -res.scoreWhitePov;
    send("info depth " + std::to_string(res.depth) + " score cp " + std::to_string(scoreStm) +
         " nodes " + std::to_string(res.nodes));
    send("bestmove " + moveToUci(res.bestMove));
  }
}

void handleGo(std::istringstream& iss) {
  GoParams gp;
  std::string tok;
  while (iss >> tok) {
    if (tok == "wtime") { iss >> gp.wtime; gp.haveWtime = true; }
    else if (tok == "btime") { iss >> gp.btime; gp.haveBtime = true; }
    else if (tok == "winc") { iss >> gp.winc; }
    else if (tok == "binc") { iss >> gp.binc; }
    else if (tok == "movestogo") { iss >> gp.movestogo; }
    else if (tok == "depth") { iss >> gp.depth; gp.haveDepth = true; }
    else if (tok == "movetime") { iss >> gp.movetime; gp.haveMovetime = true; }
    else if (tok == "infinite") { gp.infinite = true; }
    else if (tok == "nodes" || tok == "mate") { long long dummy; iss >> dummy; }
    else if (tok == "ponder") { /* pondering not implemented; treated as a normal go */ }
  }

  stopAndJoinIfRunning();
  g_stopFlag.store(false, std::memory_order_relaxed);
  g_searchThread = std::thread(searchThreadFunc, gp);
}

// Only "Hash" is implemented (name/value tokens, case-sensitive match per
// UCI convention). Unknown option names are accepted and silently ignored,
// per the UCI spec.
void handleSetOption(std::istringstream& iss) {
  std::string tok, name, value;
  iss >> tok; // "name"
  while (iss >> tok && tok != "value") {
    if (!name.empty()) name += ' ';
    name += tok;
  }
  while (iss >> tok) {
    if (!value.empty()) value += ' ';
    value += tok;
  }
  if (name == "Hash") {
    try {
      int mb = std::stoi(value);
      mb = std::max(MIN_HASH_MB, std::min(MAX_HASH_MB, mb));
      stopAndJoinIfRunning();
      search::setHashSizeMB(static_cast<std::size_t>(mb));
    } catch (...) {
      // malformed value — ignore, per UCI convention
    }
  }
}

} // namespace

void runUciLoop() {
  setStartpos();
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "uci") {
      send(std::string("id name ") + ENGINE_NAME);
      send(std::string("id author ") + ENGINE_AUTHOR);
      send("option name Hash type spin default " + std::to_string(DEFAULT_HASH_MB) +
           " min " + std::to_string(MIN_HASH_MB) + " max " + std::to_string(MAX_HASH_MB));
      send("uciok");
    } else if (cmd == "isready") {
      send("readyok");
    } else if (cmd == "ucinewgame") {
      stopAndJoinIfRunning();
      search::newGame();
      setStartpos();
    } else if (cmd == "position") {
      handlePosition(iss);
    } else if (cmd == "go") {
      handleGo(iss);
    } else if (cmd == "stop") {
      stopAndJoinIfRunning();
    } else if (cmd == "quit") {
      stopAndJoinIfRunning();
      break;
    } else if (cmd == "setoption") {
      handleSetOption(iss);
    } else if (cmd == "debug" || cmd == "ponderhit") {
      // accepted and ignored
    }
    // unknown commands ignored, per UCI convention
  }
  stopAndJoinIfRunning();
}
