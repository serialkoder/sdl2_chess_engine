Benchmarking notes
------------------

Goal: enable headless UCI mode and run cutechess/Ordo for Elo estimation.

What’s done
- Added UCI mode (`./engine --uci`) with `uci`, `isready`, `position`, `go depth/movetime`, `bestmove` responses.
- Added helper scripts: `scripts/run_cutechess_gauntlet.sh`, `scripts/ordo_rating.sh`.
- Built Ordo from source with mutex-based spinlocks; binary at `/tmp/ordo/ordo` (not installed).

Cutechess install attempt
- Tap added: `brew tap lorenzosim/cutechess`.
- Removed broken `emacs-app` cask; unlinked old Qt 6.9.0 and linked qtbase 6.9.3_1.
- `brew install cutechess` downloads many Qt 6.9.3 deps but fails building cutechess 1.2.0 with error:
  `src/board/board.h:34: definition of type 'QStringList' conflicts with type alias of the same name (/usr/local/lib/QtCore.framework/Headers/qcontainerfwd.h)`.
- Brew warns Xcode/CLT are outdated (installed Xcode 15.4; wants 16.2), but the compile error is source-level. Cutechess build may require patching the forward declaration or using a newer release.

Next steps to resume
- Option A: update Xcode/CLT to 16.2+, rerun `brew install cutechess`.
- Option B: manual build from source and patch:
  ```
  git clone https://github.com/cutechess/cutechess.git /tmp/cutechess
  cd /tmp/cutechess && mkdir build && cd build
  qmake ..    # from Qt 6.9.3
  # if same QStringList clash appears, remove the forward declaration in src/board/board.h and include <QStringList>
  make -j4
  ```
  Use `/tmp/cutechess/build/cutechess-cli`.
- Optionally install Ordo: `sudo cp /tmp/ordo/ordo /usr/local/bin/ordo`.

Suggested gauntlet config once cutechess-cli is available
- Concurrency: 4 games (`-concurrency 4`) on i7-9750H (6C/12T); bump to 6 if thermals allow.
- Time control: smoke test `-each tc=40/10`; deeper `-each tc=40/60`.
- Games: start `GAMES=40` (20 pairs), scale to 200–400 for tighter Elo.
- Use an openings PGN (~8–20 lines) with `-repeat`.
- Example run:
  ```
  ENGINE_BIN=./build/engine STOCKFISH_BIN=/path/to/stockfish \
  OPENINGS_PGN=./openings.pgn OUT_PGN=./gauntlet.pgn GAMES=40 \
  scripts/run_cutechess_gauntlet.sh -each tc=40/10 -concurrency 4
  ```
- Rate with Ordo:
  ```
  PGN_FILE=./gauntlet.pgn OUTPUT_FILE=./ratings.txt scripts/ordo_rating.sh
  ```

