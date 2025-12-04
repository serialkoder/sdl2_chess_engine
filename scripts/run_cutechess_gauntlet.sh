#!/usr/bin/env bash
set -euo pipefail

ENGINE_BIN="${ENGINE_BIN:-./build/engine}"
STOCKFISH_BIN="${STOCKFISH_BIN:-stockfish}"
OPENINGS_PGN="${OPENINGS_PGN:-openings.pgn}"
OUT_PGN="${OUT_PGN:-output.pgn}"
GAMES="${GAMES:-20}"

# Uses cutechess-cli to run a small gauntlet and write a PGN for rating tools.
# Provide an openings PGN and use -repeat so both colors see the same lines, reducing bias.
cutechess-cli \
  -engine cmd="${ENGINE_BIN}" proto=uci name="SDL2 Chess Engine" \
  -engine cmd="${STOCKFISH_BIN}" proto=uci name="Stockfish" \
  -each tc=40/60 proto=uci \
  -openings file="${OPENINGS_PGN}" format=pgn order=random plies=12 \
  -games "${GAMES}" \
  -repeat \
  -pgnout "${OUT_PGN}"

echo "Gauntlet complete. PGN written to ${OUT_PGN}"

