# Chess Engine & SDL2 GUI – Roadmap

This document captures planned improvements and next steps for the C++17 chess engine and SDL2 GUI.

## 1. Search Improvements

- **Deeper / smarter search** — Implemented: iterative deepening with default depth raised (UI uses depth 6 with time budget), check/passed/recapture extensions; still room to tune depth/time defaults.

- **Move ordering** — Implemented: TT best move, MVV–LVA captures, promotions priority, killer table, history heuristic.

- **Pruning / reductions** — Implemented: null-move pruning and late-move reductions for low-priority quiets.

## 2. Evaluation Improvements

- **King safety** — Implemented: pawn shield/open-file penalties, castling incentive/un-castled penalty after move threshold, nearby enemy piece pressure.

- **Pawn structure** — Implemented: passed pawn scaling by rank, isolated, doubled, and backward pawn penalties.

- **Piece activity** — Implemented: developed/central minors, rooks on open/semi-open/7th, edge knight penalties, mild queen activity bonuses.

- **Game phase and scaling** — Implemented: material-based phase with blended midgame/endgame king tables and scores.

## 3. Transposition Table & Time Management

- **TT refinement** — Implemented: fixed-size TT keyed by `zobrist_key % TTSize`, stores depth/node type/best move, normalized mate scores.

- **Time management** — Implemented: simple per-move budget helper feeding iterative deepening; search stops on time while keeping last completed iteration.

## 4. GUI / UX Enhancements

- **UI features** — Pending: status area (side, score, depth, NPS), move list + last-move highlight, “new game” control, optional “flip board”.

- **Input / usability** — Pending: last-move highlight, optional legal-move hints, engine settings via shortcuts/menu.

## 5. Testing & Tooling

- **Perft and regression** — Pending: expand standard perft positions (e.g. Kiwipete) to validate movegen after changes.

- **Engine diagnostics** — Pending: CLI mode to search a given FEN at fixed depth and print score/PV for eval/search debugging.
