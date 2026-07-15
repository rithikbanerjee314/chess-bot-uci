#pragma once
#include "position.hpp"
#include <vector>
#include <cstdint>

enum Bound : std::uint8_t { BOUND_NONE = 0, BOUND_EXACT, BOUND_LOWER, BOUND_UPPER };

struct TTEntry {
  std::uint64_t key = 0;
  std::int16_t score = 0;
  std::int8_t depth = -1;
  Bound bound = BOUND_NONE;
  Move move;
};

class TranspositionTable {
public:
  explicit TranspositionTable(std::size_t sizeMB = 64) { resize(sizeMB); }

  void resize(std::size_t sizeMB) {
    std::size_t bytes = sizeMB * 1024ULL * 1024ULL;
    std::size_t count = bytes / sizeof(TTEntry);
    std::size_t pow2 = 1;
    while (pow2 * 2 <= count) pow2 *= 2;
    if (pow2 == 0) pow2 = 1;
    table_.assign(pow2, TTEntry{});
    mask_ = pow2 - 1;
  }

  void clear() { std::fill(table_.begin(), table_.end(), TTEntry{}); }

  const TTEntry* probe(std::uint64_t key) const {
    const TTEntry& e = table_[key & mask_];
    return (e.bound != BOUND_NONE && e.key == key) ? &e : nullptr;
  }

  // Always-replace-by-depth: a deeper (or equal) search overrides whatever
  // was in the slot, regardless of which position it belonged to.
  void store(std::uint64_t key, int depth, int score, Bound bound, const Move& move) {
    TTEntry& e = table_[key & mask_];
    if (e.bound == BOUND_NONE || e.depth <= depth) {
      e.key = key;
      e.score = static_cast<std::int16_t>(score);
      e.depth = static_cast<std::int8_t>(depth);
      e.bound = bound;
      e.move = move;
    }
  }

private:
  std::vector<TTEntry> table_;
  std::size_t mask_ = 0;
};
