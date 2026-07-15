#pragma once
#include "position.hpp"
#include <vector>

// Pseudo-legal: obeys piece movement rules and doesn't move onto own
// pieces, but may leave the mover's own king in check.
void generatePseudoMoves(const Position& pos, std::vector<Move>& moves);

// Fully legal moves. Takes Position by non-const reference and uses
// make/unmake internally to test each candidate for leaving the king in
// check — no board copies, unlike a naive "copy, apply, test" approach.
// pos is restored to its original state before this function returns.
void generateLegalMoves(Position& pos, std::vector<Move>& moves);
