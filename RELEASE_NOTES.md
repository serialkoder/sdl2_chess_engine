# Release Notes

## Current (main)
- Added history browser: saved games list, replay controls, click-to-jump move list, SAN/UCI toggle, autoplay, and per-mode annotations.
- Export helpers: PGN export to `exports/`, copy PGN/FEN to clipboard.
- Play-mode move list with navigation (|< < LIVE > >|); browsing uses a view board so live play stays intact.
- Annotations on board (arrows/circles with color modifiers), preview while dragging, auto-clear with PIN toggle, and per-mode CLEAR.
- Captured pieces tray and material diff for the currently viewed position (works in play and history).
- Pixel font fixes: full glyph coverage for move text/labels and safe fallback to `?`.
