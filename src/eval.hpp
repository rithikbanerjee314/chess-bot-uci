#pragma once
#include "position.hpp"

namespace eval {

constexpr int MATE_SCORE = 32000;
constexpr int MAX_PHASE = 24; // phase units used by pstValue's `phase` param (24=pure MG, 0=pure EG)

// Full evaluation, white POV centipawns. includeTempo adds the side-to-move
// bonus (omit when "whose turn" isn't meaningful for the comparison).
int evaluate(const Position& pos, bool includeTempo = true);

// Material + PST only — no pawn structure/mobility/king-safety/etc. Used by
// search's lazy-evaluation fast path.
int fastEval(const Position& pos, bool includeTempo = true);

// Exposed for search's SEE / MVV-LVA and move-ordering PST tiebreak, so
// those tables aren't duplicated between eval.cpp and search.cpp.
int pieceValueMG(PieceType t);
int pstValue(PieceType t, bool white, Square sq, int phase);

} // namespace eval
