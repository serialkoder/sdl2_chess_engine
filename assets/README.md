# Chess assets

This directory contains the graphical assets used by the Pygame chess game.  The
pieces were obtained from open‑licensed sources on Wikimedia Commons and the
board was generated programmatically.  An optional application icon was
downloaded from Wikimedia Commons as well.

## Pieces

The twelve piece images originate from the **standard transparent chess set**
created by the Wikimedian artist **Cburnett**.  Each piece is 60×60 pixels
with a transparent background.  The files were downloaded from their
respective pages on Wikimedia Commons and renamed to match chess notation.

| Original filename | Description (Commons) | Saved as |
| --- | --- | --- |
| `Chess_klt60.png` | White king (light) | `pieces/wK.png` |
| `Chess_qlt60.png` | White queen (light) | `pieces/wQ.png` |
| `Chess_rlt60.png` | White rook (light) | `pieces/wR.png` |
| `Chess_blt60.png` | White bishop (light) | `pieces/wB.png` |
| `Chess_nlt60.png` | White knight (light) | `pieces/wN.png` |
| `Chess_plt60.png` | White pawn (light) | `pieces/wP.png` |
| `Chess_kdt60.png` | Black king (dark) | `pieces/bK.png` |
| `Chess_qdt60.png` | Black queen (dark) | `pieces/bQ.png` |
| `Chess_rdt60.png` | Black rook (dark) | `pieces/bR.png` |
| `Chess_bdt60.png` | Black bishop (dark) | `pieces/bB.png` |
| `Chess_ndt60.png` | Black knight (dark) | `pieces/bN.png` |
| `Chess_pdt60.png` | Black pawn (dark) | `pieces/bP.png` |

**License:** The original chess piece images are licensed under the
Creative Commons **Attribution‑Share Alike 3.0 Unported** license (CC BY‑SA 3.0).
The file description on Wikimedia Commons specifies that anyone may share or
remix the images provided they give appropriate credit and distribute
derivative works under the same license【129790275719008†L131-L161】.

**Attribution:** “Chess piece images by Cburnett — CC BY‑SA 3.0” with links
to the individual files on Wikimedia Commons.

## Board

The board image `board.png` (640×640 pixels) was generated via a short
Python script using the Pillow library.  It consists of an 8×8 grid of
squares sized 80 pixels each, with light squares colored `#F0D9B5` and dark
squares colored `#B58863`.  As this image was created from scratch for this
project, it is released into the public domain.

## Icon

The optional application icon `icons/app_icon.png` was downloaded from
Wikimedia Commons (file `Darkking.png`).  It depicts a simple black king
silhouette.  The author dedicated the work to the public domain via the
Creative Commons CC0 1.0 Universal Public Domain Dedication, allowing it to
be used without restriction【271345349959596†L141-L151】.
