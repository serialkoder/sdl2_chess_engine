# Chess Engine & SDL2 GUI – Roadmap

This document captures planned improvements and next steps for the C++17 chess engine and SDL2 GUI.

## 1. Search Improvements

- **Deeper / smarter search**
  - Increase default engine depth (e.g. 5–6) and rely more on `timeLimitMs` for time control.
  - Add check extensions so forcing sequences with checks are searched more deeply.
  - Consider selective extensions for passed pawn pushes or immediate recaptures.

- **Move ordering**
  - Implement MVV–LVA ordering for captures (most valuable victim / least valuable attacker).
  - Add a small killer-move table and history heuristic to prioritize moves that caused cutoffs.
  - Order moves roughly: TT best move → captures (MVV–LVA) → promotions → killer moves → other quiet moves.

- **Pruning / reductions (later)**
  - Add null-move pruning with a depth reduction to quickly detect obvious cutoffs.
  - Add late-move reductions (LMR) for low-priority moves late in the move list at deeper nodes.

## 2. Evaluation Improvements

- **King safety**
  - Penalize a king with weak pawn cover and open/semi-open files nearby.
  - Encourage castling in the opening/middlegame; penalize an uncastled king after some move threshold.
  - Add bonuses/penalties for enemy pieces near the king.

- **Pawn structure**
  - Detect and score:
    - Passed pawns (with bonuses that grow as they advance).
    - Isolated and doubled pawns (penalties).
    - Backward pawns on open or semi-open files.

- **Piece activity**
  - Bonuses for:
    - Developed minor pieces.
    - Rooks on open or semi-open files.
    - Knights in the centre; mild penalties for edge/corner knights.
  - Penalize trapped or clearly misplaced pieces.

- **Game phase and scaling**
  - Track material to estimate phase (opening → endgame).
  - Blend between different king piece–square tables for middlegame vs endgame.

## 3. Transposition Table & Time Management

- **TT refinement**
  - Replace the `std::unordered_map` TT with a fixed-size table indexed by `zobrist_key % TTSize`.
  - Store key, depth, score, node type, and best move per entry.
  - Normalize mate scores (distance-to-mate) so they remain consistent across depths.

- **Time management**
  - Implement simple time allocation across moves instead of a fixed per-move `engineTimeMs`.
  - Use iterative deepening and stop search gracefully as time expires, keeping the best completed iteration.

## 4. GUI / UX Enhancements

- **UI features**
  - Display side-to-move, score, depth, and NPS in a status area or side panel.
  - Show move list (simple algebraic or PGN-style) and last-move highlighting for both sides.
  - Add a “new game” control and optionally a “flip board” option.

- **Input / usability**
  - Highlight the last move made.
  - Optional: show legal move hints when hovering over a piece.
  - Expose engine settings (search depth, time limit) via keyboard shortcuts or a simple menu.

## 5. Testing & Tooling

- **Perft and regression**
  - Keep using `chess_perft` to validate move generation after any changes to movegen or make/undo.
  - Add more standard test positions (e.g. Kiwipete) with known perft results.

- **Engine diagnostics**
  - Add a CLI mode to:
    - Search a given FEN at fixed depth.
    - Print score and principal variation for debugging evaluation and search behaviour.

