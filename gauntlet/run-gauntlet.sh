#!/usr/bin/env bash
# Runs ChessBotUCI (uci.js) as a gauntlet against Stockfish at several
# UCI_LimitStrength levels, using cutechess-cli. Results (PGN) land in
# gauntlet/results/ — feed that into `ordo` or `bayeselo` for an Elo
# estimate anchored to Stockfish's known ratings at each level.
#
# Requires cutechess-cli and Stockfish on PATH, or point at them explicitly:
#   STOCKFISH_CMD=/path/to/stockfish CUTECHESS_CMD=/path/to/cutechess-cli ./run-gauntlet.sh
#
# Tunable via env vars (defaults shown):
#   TC=10+0.1        # time control per cutechess-cli syntax
#   ROUNDS=20         # rounds per opponent (each round = 2 games, colors swapped)
#   CONCURRENCY=4     # games running in parallel
#   ELOS="1500 1800 2100 2400"   # Stockfish UCI_Elo levels to play against
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

STOCKFISH_CMD="${STOCKFISH_CMD:-stockfish}"
CUTECHESS_CMD="${CUTECHESS_CMD:-cutechess-cli}"
TC="${TC:-10+0.1}"
ROUNDS="${ROUNDS:-20}"
CONCURRENCY="${CONCURRENCY:-4}"
ELOS="${ELOS:-1500 1800 2100 2400}"

OUT_DIR="$SCRIPT_DIR/results"
mkdir -p "$OUT_DIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
PGN_OUT="$OUT_DIR/gauntlet-$STAMP.pgn"

ENGINE_ARGS=(-engine cmd=node arg="$REPO_DIR/uci.js" name=ChessBotUCI)
for elo in $ELOS; do
  ENGINE_ARGS+=(-engine cmd="$STOCKFISH_CMD" option.UCI_LimitStrength=true "option.UCI_Elo=$elo" option.Threads=1 "name=SF-$elo")
done

echo "Gauntlet: ChessBotUCI vs Stockfish @ [$ELOS], tc=$TC, $ROUNDS rounds/opponent, concurrency=$CONCURRENCY"
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
