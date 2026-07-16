#!/usr/bin/env bash
# Runs the compiled C++ engine as a gauntlet against Stockfish at several
# UCI_LimitStrength levels, using cutechess-cli. Results (PGN) land in
# gauntlet/results/ — feed that into `ordo`, `bayeselo`, or
# gauntlet/compute-elo.js for an Elo estimate anchored to Stockfish's known
# ratings at each level.
#
# Requires cutechess-cli and Stockfish on PATH, or point at them explicitly:
#   STOCKFISH_CMD=/path/to/stockfish CUTECHESS_CMD=/path/to/cutechess-cli ./run-gauntlet.sh
#
# Tunable via env vars (defaults shown):
#   ENGINE_CMD=../build/bin/atlas-hero   # path to the compiled UCI binary
#   TC=30+0.3         # time control per cutechess-cli syntax
#   ROUNDS=15          # rounds per opponent (each round = 2 games, colors swapped)
#   CONCURRENCY=4      # games running in parallel
#   ELOS="2200 2500 2800 3100"   # Stockfish UCI_Elo levels to play against —
#                                 # raise these as the engine gets stronger
#                                 # so the anchors still bracket its real Elo
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

STOCKFISH_CMD="${STOCKFISH_CMD:-stockfish}"
CUTECHESS_CMD="${CUTECHESS_CMD:-cutechess-cli}"
ENGINE_CMD="${ENGINE_CMD:-$REPO_DIR/build/bin/atlas-hero}"
TC="${TC:-30+0.3}"
ROUNDS="${ROUNDS:-15}"
CONCURRENCY="${CONCURRENCY:-4}"
ELOS="${ELOS:-2200 2500 2800 3100}"

OUT_DIR="$SCRIPT_DIR/results"
mkdir -p "$OUT_DIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
PGN_OUT="$OUT_DIR/gauntlet-$STAMP.pgn"

ENGINE_ARGS=(-engine cmd="$ENGINE_CMD" name=Atlas-Hero)
for elo in $ELOS; do
  ENGINE_ARGS+=(-engine cmd="$STOCKFISH_CMD" option.UCI_LimitStrength=true "option.UCI_Elo=$elo" option.Threads=1 "name=SF-$elo")
done

echo "Gauntlet: Atlas-Hero vs Stockfish @ [$ELOS], tc=$TC, $ROUNDS rounds/opponent, concurrency=$CONCURRENCY"
echo "PGN output: $PGN_OUT"

"$CUTECHESS_CMD" \
  "${ENGINE_ARGS[@]}" \
  -each proto=uci tc="$TC" \
  -tournament gauntlet \
  -rounds "$ROUNDS" -games 2 -repeat \
  -openings file="$SCRIPT_DIR/openings.pgn" format=pgn order=random policy=round \
  -concurrency "$CONCURRENCY" \
  -recover \
  -ratinginterval 10 \
  -pgnout "$PGN_OUT"

echo "Done. Games saved to $PGN_OUT"
echo "Feed this PGN into ordo/bayeselo (anchored to the SF-XXXX names' known Elo) for an estimate."
