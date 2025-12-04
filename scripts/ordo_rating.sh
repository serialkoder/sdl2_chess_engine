#!/usr/bin/env bash
set -euo pipefail

PGN_FILE="${PGN_FILE:-output.pgn}"
OUTPUT_FILE="${OUTPUT_FILE:-ratings.txt}"

ordo -p "${PGN_FILE}" -o "${OUTPUT_FILE}"

echo "Ratings written to ${OUTPUT_FILE}"

