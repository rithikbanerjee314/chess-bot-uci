#pragma once
#include "position.hpp"
#include <atomic>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace search {

struct SearchResult {
  Move bestMove;
  int scoreWhitePov = 0; // white-POV centipawns, matches eval::evaluate's convention
  int depth = 0;
  std::uint64_t nodes = 0;
};

// Clears TT/killers/history — call at the start of a new game.
void newGame();

// Resizes the transposition table (megabytes).
void setHashSizeMB(std::size_t mb);

// Iterative deepening from depth 1 to maxDepth with a wall-clock budget.
// softMs: stop STARTING a new iteration past this point (a started-but-
// unfinished iteration is discarded, so time spent starting one late is
// mostly wasted). hardMs: aborts mid-iteration; the deepest *fully
// completed* iteration's move is returned. stopFlag: checked periodically
// so a UCI `stop` command can preempt the search from another thread —
// unlike the JS engine's version, this one is genuinely interruptible.
// prevKeys: zobrist hashes of every position already reached in the game,
// so the search scores a return to any of them as a repetition draw.
SearchResult think(Position& pos, int maxDepth, long long hardMs, long long softMs,
                    const std::vector<std::uint64_t>& prevKeys, std::atomic<bool>& stopFlag);

} // namespace search
