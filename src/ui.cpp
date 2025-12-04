#include "ui.h"

#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "board.h"
#include "history.h"
#include "move.h"
#include "notation.h"
#include "search.h"

namespace ui
{
    namespace
    {
        constexpr int SquareSize = 80;
        constexpr int BoardPixels = SquareSize * 8;
        constexpr int PanelWidth = 320;
        constexpr int WindowWidth = BoardPixels + PanelWidth;
        constexpr int WindowHeight = BoardPixels;
        constexpr int PanelPadding = 12;
        constexpr int ButtonHeight = 40;
        constexpr int ButtonSpacing = 12;
        constexpr int ListRowHeight = 32;
        constexpr int HistoryControlsHeight = 80;
        constexpr int TextScale = 2;
        constexpr std::uint32_t AutoPlayIntervalMs = 250;
        constexpr std::uint32_t StatusDurationMs = 2000;
        constexpr float Pi = 3.14159265f;
        constexpr int DefaultEngineDepth = 6;
        constexpr int DefaultEngineTimeMs = 3000;

        const std::string StartingFen =
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        const SDL_Color PanelBg{40, 40, 45, 255};
        const SDL_Color ButtonBg{70, 70, 80, 255};
        const SDL_Color ButtonHover{90, 90, 110, 255};
        const SDL_Color ButtonPressed{120, 120, 150, 255};
        const SDL_Color ButtonDisabled{60, 60, 70, 255};
        const SDL_Color TextColor{235, 235, 240, 255};
        const SDL_Color ListRowAlt{50, 50, 60, 255};
        const SDL_Color ListRowSel{80, 110, 150, 255};
        const SDL_Color ArrowGreen{0, 180, 0, 180};
        const SDL_Color ArrowYellow{230, 200, 0, 180};
        const SDL_Color ArrowRed{210, 50, 50, 180};
        const SDL_Color ArrowBlue{60, 140, 230, 180};

        enum class UIMode
        {
            Play,
            History
        };

        struct GameState
        {
            std::string startFen{StartingFen};
            std::vector<std::string> movesUci;
            std::vector<std::string> sanMoves;
            bool gameOver{false};
            int engineDepth{DefaultEngineDepth};
            int engineTimeMs{DefaultEngineTimeMs};
        };

        struct GameEndInfo
        {
            bool ended{false};
            std::string result{"*"};
            std::string termination{"unknown"};
        };

        struct HistoryUIState
        {
            std::vector<GameMeta> games;
            int selectedIndex{0};
            int scrollOffset{0};
            int moveListScroll{0};
            GameRecord loaded{};
            bool loadedValid{false};
            int ply{0};
            std::vector<std::string> sanMoves;
            bool showSan{true};
            bool autoplay{false};
            std::uint32_t lastAutoTick{0};
            std::string statusText;
            std::uint32_t statusExpireMs{0};
            Board replayBoard;
        };

        struct PlayViewState
        {
            Board viewBoard;
            int viewPly{0};
            int moveListScroll{0};
            std::string statusText;
            std::uint32_t statusExpireMs{0};
        };

        struct Arrow
        {
            int from{-1};
            int to{-1};
            SDL_Color color{};
        };

        struct Circle
        {
            int square{-1};
            SDL_Color color{};
        };

        struct Annotations
        {
            std::vector<Arrow> arrows;
            std::vector<Circle> circles;
            bool dragging{false};
            int dragFromSquare{-1};
            int dragToSquare{-1};
            SDL_Color dragColor{};
        };

        struct Glyph
        {
            int width{5};
            std::array<std::uint8_t, 7> rows{};
        };

        bool hit_test(const SDL_Rect& rect, int x, int y)
        {
            return x >= rect.x && x < rect.x + rect.w &&
                   y >= rect.y && y < rect.y + rect.h;
        }

        void fill_rect(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color color)
        {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_RenderFillRect(renderer, &rect);
        }

        Glyph glyph_for_char(char ch)
        {
            switch (ch)
            {
            case '0': return {5, {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}};
            case '1': return {5, {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}};
            case '2': return {5, {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}};
            case '3': return {5, {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110}};
            case '4': return {5, {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}};
            case '5': return {5, {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}};
            case '6': return {5, {0b01110, 0b10000, 0b11110, 0b10001, 0b10001, 0b10001, 0b01110}};
            case '7': return {5, {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}};
            case '8': return {5, {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}};
            case '9': return {5, {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110}};

            case 'A': return {5, {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}};
            case 'B': return {5, {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}};
            case 'C': return {5, {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}};
            case 'D': return {5, {0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100}};
            case 'E': return {5, {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}};
            case 'F': return {5, {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}};
            case 'H': return {5, {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}};
            case 'I': return {3, {0b111, 0b010, 0b010, 0b010, 0b010, 0b010, 0b111}};
            case 'K': return {5, {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}};
            case 'L': return {5, {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}};
            case 'N': return {5, {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001}};
            case 'O': return {5, {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}};
            case 'P': return {5, {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}};
            case 'R': return {5, {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}};
            case 'S': return {5, {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}};
            case 'T': return {5, {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}};
            case 'U': return {5, {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}};
            case 'V': return {5, {0b10001, 0b10001, 0b10001, 0b01010, 0b01010, 0b00100, 0b00100}};
            case 'X': return {5, {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}};
            case 'Y': return {5, {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}};
            case 'W': return {5, {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010}};
            case 'a': return {5, {0b00000, 0b01110, 0b00001, 0b01111, 0b10001, 0b10001, 0b01111}};
            case 'b': return {5, {0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b10001, 0b11110}};
            case 'c': return {5, {0b00000, 0b01110, 0b10001, 0b10000, 0b10000, 0b10001, 0b01110}};
            case 'd': return {5, {0b00001, 0b00001, 0b01111, 0b10001, 0b10001, 0b10001, 0b01111}};
            case 'e': return {5, {0b00000, 0b01110, 0b10001, 0b11111, 0b10000, 0b10000, 0b01110}};
            case 'f': return {5, {0b00110, 0b01001, 0b01000, 0b11100, 0b01000, 0b01000, 0b01000}};
            case 'g': return {5, {0b00000, 0b01111, 0b10001, 0b10001, 0b01111, 0b00001, 0b11110}};
            case 'h': return {5, {0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b10001, 0b10001}};
            case 'x': return {5, {0b00000, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b00000}};

            case '-': return {5, {0b00000, 0b00000, 0b00000, 0b01110, 0b00000, 0b00000, 0b00000}};
            case '/': return {5, {0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b00000, 0b00000}};
            case ':': return {3, {0b000, 0b010, 0b000, 0b000, 0b010, 0b000, 0b000}};
            case '|': return {3, {0b010, 0b010, 0b010, 0b010, 0b010, 0b010, 0b010}};
            case '<': return {4, {0b0001, 0b0010, 0b0100, 0b1000, 0b0100, 0b0010, 0b0001}};
            case '>': return {4, {0b1000, 0b0100, 0b0010, 0b0001, 0b0010, 0b0100, 0b1000}};
            case '*': return {5, {0b00100, 0b10101, 0b01110, 0b11111, 0b01110, 0b10101, 0b00100}};
            case '+': return {5, {0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000}};
            case '#': return {5, {0b01010, 0b11111, 0b01010, 0b01010, 0b11111, 0b01010, 0b01010}};
            case '=': return {5, {0b00000, 0b11111, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000}};
            case '.': return {3, {0b000, 0b000, 0b000, 0b000, 0b000, 0b110, 0b110}};
            case '?': return {5, {0b01110, 0b10001, 0b00010, 0b00100, 0b00100, 0b00000, 0b00100}};
            case ' ': return {3, {0, 0, 0, 0, 0, 0, 0}};

            default:
                return {5, {0, 0, 0, 0, 0, 0, 0}};
            }
        }

        int measure_text_width(const std::string& text, int scale)
        {
            int width = 0;
            for (char ch : text)
            {
                const Glyph glyph = glyph_for_char(ch);
                width += (glyph.width + 1) * scale;
            }
            if (!text.empty())
            {
                width -= scale;
            }
            return width;
        }

        void draw_glyph(SDL_Renderer* renderer, int x, int y, int scale, const Glyph& glyph, SDL_Color color)
        {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            for (int row = 0; row < 7; ++row)
            {
                const std::uint8_t bits = glyph.rows[static_cast<std::size_t>(row)];
                for (int col = 0; col < glyph.width; ++col)
                {
                    if ((bits >> (glyph.width - 1 - col)) & 1U)
                    {
                        SDL_Rect pixel{x + col * scale, y + row * scale, scale, scale};
                        SDL_RenderFillRect(renderer, &pixel);
                    }
                }
            }
        }

        void draw_text(SDL_Renderer* renderer,
                       int x,
                       int y,
                       int scale,
                       const std::string& text,
                       SDL_Color color)
        {
            SDL_Color useColor = color;
            useColor.a = 255;
            int cursorX = x;
            for (char ch : text)
            {
                Glyph glyph = glyph_for_char(ch);
                const bool emptyGlyph =
                    std::all_of(glyph.rows.begin(), glyph.rows.end(), [](std::uint8_t v) { return v == 0; });
                if (emptyGlyph && ch != ' ')
                {
                    glyph = glyph_for_char('?');
                }

                draw_glyph(renderer, cursorX, y, scale, glyph, useColor);
                cursorX += (glyph.width + 1) * scale;
            }
        }

        SDL_Texture* load_texture(SDL_Renderer* renderer, const std::string& path)
        {
            SDL_Texture* texture = IMG_LoadTexture(renderer, path.c_str());
            if (!texture)
            {
                std::cerr << "Failed to load texture: " << path << " - " << IMG_GetError() << '\n';
            }
            return texture;
        }

        std::string piece_texture_name(Piece piece)
        {
            switch (piece)
            {
            case Piece::WhitePawn:   return "wP.png";
            case Piece::WhiteKnight: return "wN.png";
            case Piece::WhiteBishop: return "wB.png";
            case Piece::WhiteRook:   return "wR.png";
            case Piece::WhiteQueen:  return "wQ.png";
            case Piece::WhiteKing:   return "wK.png";
            case Piece::BlackPawn:   return "bP.png";
            case Piece::BlackKnight: return "bN.png";
            case Piece::BlackBishop: return "bB.png";
            case Piece::BlackRook:   return "bR.png";
            case Piece::BlackQueen:  return "bQ.png";
            case Piece::BlackKing:   return "bK.png";
            case Piece::None:
            default:
                return {};
            }
        }

        int screen_to_square(int x, int y)
        {
            if (x < 0 || x >= BoardPixels || y < 0 || y >= BoardPixels)
            {
                return -1;
            }

            const int file = x / SquareSize;
            const int rankFromTop = y / SquareSize;
            const int rank = 7 - rankFromTop;

            return make_square(file, rank);
        }

        SDL_Rect square_rect(int square)
        {
            const int file = file_of(square);
            const int rank = rank_of(square);
            const int x = file * SquareSize;
            const int y = (7 - rank) * SquareSize;
            SDL_Rect rect{x, y, SquareSize, SquareSize};
            return rect;
        }

        bool apply_uci_move(Board& board, const std::string& uci)
        {
            const std::vector<Move> moves = board.generate_legal_moves();
            for (const Move& move : moves)
            {
                if (move.to_uci() == uci)
                {
                    board.make_move(move);
                    return true;
                }
            }

            std::cerr << "Failed to apply UCI move: " << uci << '\n';
            return false;
        }

        GameEndInfo detect_game_end(Board& board)
        {
            GameEndInfo info;

            const std::vector<Move> moves = board.generate_legal_moves();
            if (!moves.empty())
            {
                return info;
            }

            info.ended = true;
            const Color side = board.side_to_move();
            if (board.is_in_check(side))
            {
                info.termination = "checkmate";
                info.result = (side == Color::White) ? "0-1" : "1-0";
            }
            else
            {
                info.termination = "stalemate";
                info.result = "1/2-1/2";
            }

            return info;
        }

        void save_if_game_over(Board& board, GameState& gameState)
        {
            if (gameState.gameOver)
            {
                return;
            }

            GameEndInfo endInfo = detect_game_end(board);
            if (!endInfo.ended)
            {
                return;
            }

            gameState.gameOver = true;

            GameRecord record;
            record.startFen = gameState.startFen;
            record.moves = gameState.movesUci;
            record.finalFen = board.to_fen();
            record.result = endInfo.result;
            record.termination = endInfo.termination;
            record.engineDepth = gameState.engineDepth;
            record.engineTimeMs = gameState.engineTimeMs;

            history::save_game(record);
        }

        void reset_game(Board& board,
                        GameState& gameState,
                        int& selectedSquare,
                        std::vector<Move>& legalMovesForSelected)
        {
            board.load_fen(StartingFen);
            gameState.startFen = StartingFen;
            gameState.movesUci.clear();
            gameState.gameOver = false;
            gameState.engineDepth = DefaultEngineDepth;
            gameState.engineTimeMs = DefaultEngineTimeMs;
            selectedSquare = -1;
            legalMovesForSelected.clear();
        }

        std::string format_utc_brief(const std::string& utc)
        {
            if (utc.empty())
            {
                return "UNKNOWN";
            }

            std::string value = utc;
            if (!value.empty() && value.back() == 'Z')
            {
                value.pop_back();
            }
            for (char& ch : value)
            {
                if (ch == 'T')
                {
                    ch = ' ';
                }
            }
            if (value.size() > 16)
            {
                value = value.substr(0, 16);
            }
            return value;
        }

        void rebuild_replay_position(HistoryUIState& historyState, int targetPly)
        {
            historyState.replayBoard.load_fen(
                historyState.loaded.startFen.empty() ? StartingFen : historyState.loaded.startFen);

            const int maxPly =
                historyState.loadedValid
                    ? std::min(targetPly, static_cast<int>(historyState.loaded.moves.size()))
                    : 0;

            int applied = 0;
            for (int i = 0; i < maxPly; ++i)
            {
                if (!apply_uci_move(historyState.replayBoard, historyState.loaded.moves[i]))
                {
                    break;
                }
                ++applied;
            }

            historyState.ply = applied;
        }

        void load_history_entry(HistoryUIState& historyState, int index)
        {
            if (index < 0 || index >= static_cast<int>(historyState.games.size()))
            {
                historyState.loadedValid = false;
                historyState.loaded = GameRecord{};
                historyState.ply = 0;
                historyState.replayBoard.load_fen(StartingFen);
                return;
            }

            historyState.selectedIndex = index;
            historyState.loaded = history::load_game(historyState.games[static_cast<std::size_t>(index)].path);
            historyState.loadedValid = true;
            historyState.autoplay = false;
            historyState.ply = 0;
            historyState.statusText.clear();
            historyState.statusExpireMs = 0;
            historyState.showSan = true;
            historyState.moveListScroll = 0;

            historyState.sanMoves.clear();
            if (!historyState.loaded.moves.empty())
            {
                Board tmp;
                tmp.load_fen(
                    historyState.loaded.startFen.empty() ? StartingFen : historyState.loaded.startFen);

                historyState.sanMoves.reserve(historyState.loaded.moves.size());
                for (const std::string& uci : historyState.loaded.moves)
                {
                    const std::vector<Move> legal = tmp.generate_legal_moves();
                    Move matched{};
                    bool found = false;
                    for (const Move& m : legal)
                    {
                        if (m.to_uci() == uci)
                        {
                            matched = m;
                            found = true;
                            break;
                        }
                    }

                    if (found)
                    {
                        historyState.sanMoves.push_back(move_to_san(tmp, matched));
                        tmp.make_move(matched);
                    }
                    else
                    {
                        historyState.sanMoves.push_back(uci);
                        apply_uci_move(tmp, uci);
                    }
                }
            }

            rebuild_replay_position(historyState, 0);
        }

        void refresh_history(HistoryUIState& historyState)
        {
            historyState.games = history::list_games();
            historyState.scrollOffset = 0;
            historyState.moveListScroll = 0;
            historyState.autoplay = false;
            historyState.ply = 0;

            if (!historyState.games.empty())
            {
                load_history_entry(historyState, 0);
            }
            else
            {
                historyState.loadedValid = false;
                historyState.loaded = GameRecord{};
                historyState.replayBoard.load_fen(StartingFen);
            }
        }

        int total_plies(const HistoryUIState& historyState)
        {
            return historyState.loadedValid
                       ? static_cast<int>(historyState.loaded.moves.size())
                       : 0;
        }

        void clamp_scroll(HistoryUIState& historyState, int listHeight)
        {
            const int contentHeight = static_cast<int>(historyState.games.size()) * ListRowHeight;
            const int maxScroll = std::max(0, contentHeight - listHeight);
            if (historyState.scrollOffset < 0)
            {
                historyState.scrollOffset = 0;
            }
            else if (historyState.scrollOffset > maxScroll)
            {
                historyState.scrollOffset = maxScroll;
            }
        }

        void clamp_move_scroll(HistoryUIState& historyState, int listHeight)
        {
            const int movesRows = (total_plies(historyState) + 1) / 2;
            const int contentHeight = movesRows * ListRowHeight;
            const int maxScroll = std::max(0, contentHeight - listHeight);
            if (historyState.moveListScroll < 0)
            {
                historyState.moveListScroll = 0;
            }
            else if (historyState.moveListScroll > maxScroll)
            {
                historyState.moveListScroll = maxScroll;
            }
        }

        void clamp_play_move_scroll(PlayViewState& playState,
                                    const GameState& gameState,
                                    int listHeight)
        {
            const int movesRows = (static_cast<int>(gameState.movesUci.size()) + 1) / 2;
            const int contentHeight = movesRows * ListRowHeight;
            const int maxScroll = std::max(0, contentHeight - listHeight);
            if (playState.moveListScroll < 0)
            {
                playState.moveListScroll = 0;
            }
            else if (playState.moveListScroll > maxScroll)
            {
                playState.moveListScroll = maxScroll;
            }
        }

        std::string sanitize_filename(const std::string& name)
        {
            std::string out = name;
            for (char& c : out)
            {
                if (c == ':' || c == ' ' || c == '/')
                {
                    c = '-';
                }
            }
            return out;
        }

        std::filesystem::path export_dir()
        {
            std::filesystem::path dir;
            char* rawPath = SDL_GetPrefPath("serialcoder", "sdl2_chess_engine");
            if (rawPath)
            {
                dir = std::filesystem::path(rawPath) / "exports";
                SDL_free(rawPath);
            }
            else
            {
                dir = std::filesystem::temp_directory_path() / "sdl2_chess_engine" / "exports";
            }

            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (ec)
            {
                std::cerr << "Failed to create export directory " << dir << ": " << ec.message() << '\n';
            }
            return dir;
        }

        std::string pgn_date_from_utc(const std::string& utc)
        {
            if (utc.size() >= 10 &&
                std::isdigit(static_cast<unsigned char>(utc[0])) &&
                std::isdigit(static_cast<unsigned char>(utc[1])) &&
                std::isdigit(static_cast<unsigned char>(utc[2])) &&
                std::isdigit(static_cast<unsigned char>(utc[3])) &&
                utc[4] == '-' &&
                std::isdigit(static_cast<unsigned char>(utc[5])) &&
                std::isdigit(static_cast<unsigned char>(utc[6])) &&
                utc[7] == '-' &&
                std::isdigit(static_cast<unsigned char>(utc[8])) &&
                std::isdigit(static_cast<unsigned char>(utc[9])))
            {
                return utc.substr(0, 4) + "." + utc.substr(5, 2) + "." + utc.substr(8, 2);
            }
            return "????.??.??";
        }

        void set_status(HistoryUIState& historyState, const std::string& text)
        {
            historyState.statusText = text;
            historyState.statusExpireMs = SDL_GetTicks() + StatusDurationMs;
        }

        void set_status(PlayViewState& playState, const std::string& text)
        {
            playState.statusText = text;
            playState.statusExpireMs = SDL_GetTicks() + StatusDurationMs;
        }

        std::string build_pgn(const HistoryUIState& historyState)
        {
            if (!historyState.loadedValid)
            {
                return {};
            }

            const std::string result = historyState.loaded.result.empty() ? "*" : historyState.loaded.result;
            const std::string date = pgn_date_from_utc(historyState.loaded.utc);

            std::string pgn;
            pgn += "[Event \"SDL2 Chess\"]\n";
            pgn += "[Site \"Local\"]\n";
            pgn += "[Date \"" + date + "\"]\n";
            pgn += "[Round \"-\"]\n";
            pgn += "[White \"User\"]\n";
            pgn += "[Black \"Engine\"]\n";
            pgn += "[Result \"" + result + "\"]\n";

            if (historyState.loaded.startFen != StartingFen)
            {
                pgn += "[SetUp \"1\"]\n";
                pgn += "[FEN \"" + historyState.loaded.startFen + "\"]\n";
            }

            pgn += "\n";

            const std::vector<std::string>& sanList =
                !historyState.sanMoves.empty() ? historyState.sanMoves : historyState.loaded.moves;

            const int total = static_cast<int>(sanList.size());
            for (int i = 0; i < total; i += 2)
            {
                const int moveNumber = i / 2 + 1;
                pgn += std::to_string(moveNumber) + ". ";
                pgn += sanList[static_cast<std::size_t>(i)];
                if (i + 1 < total)
                {
                    pgn += " ";
                    pgn += sanList[static_cast<std::size_t>(i + 1)];
                }
                pgn += " ";
            }

            pgn += result;
            return pgn;
        }

        SDL_Color color_from_mod(SDL_Keymod mods)
        {
            if (mods & KMOD_CTRL)
            {
                return ArrowRed;
            }
            if (mods & KMOD_SHIFT)
            {
                return ArrowYellow;
            }
            if (mods & KMOD_ALT)
            {
                return ArrowBlue;
            }
            return ArrowGreen;
        }

        bool same_color(const SDL_Color& a, const SDL_Color& b)
        {
            return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
        }

        void toggle_arrow(Annotations& annotations, int from, int to, const SDL_Color& color)
        {
            auto it = std::find_if(
                annotations.arrows.begin(),
                annotations.arrows.end(),
                [&](const Arrow& a)
                {
                    return a.from == from && a.to == to && same_color(a.color, color);
                });

            if (it != annotations.arrows.end())
            {
                annotations.arrows.erase(it);
            }
            else
            {
                annotations.arrows.push_back(Arrow{from, to, color});
            }
        }

        void toggle_circle(Annotations& annotations, int square, const SDL_Color& color)
        {
            auto it = std::find_if(
                annotations.circles.begin(),
                annotations.circles.end(),
                [&](const Circle& c)
                {
                    return c.square == square && same_color(c.color, color);
                });

            if (it != annotations.circles.end())
            {
                annotations.circles.erase(it);
            }
            else
            {
                annotations.circles.push_back(Circle{square, color});
            }
        }

        void clear_annotations(Annotations& annotations)
        {
            annotations.arrows.clear();
            annotations.circles.clear();
            annotations.dragging = false;
            annotations.dragFromSquare = -1;
            annotations.dragToSquare = -1;
        }

        SDL_FPoint square_center(int square)
        {
            SDL_Rect rect = square_rect(square);
            SDL_FPoint p;
            p.x = static_cast<float>(rect.x + SquareSize / 2);
            p.y = static_cast<float>(rect.y + SquareSize / 2);
            return p;
        }

        void draw_filled_triangle(SDL_Renderer* renderer,
                                  const SDL_FPoint& p0,
                                  const SDL_FPoint& p1,
                                  const SDL_FPoint& p2,
                                  SDL_Color color)
        {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

            const float minX = std::floor(std::min({p0.x, p1.x, p2.x}));
            const float maxX = std::ceil(std::max({p0.x, p1.x, p2.x}));
            const float minY = std::floor(std::min({p0.y, p1.y, p2.y}));
            const float maxY = std::ceil(std::max({p0.y, p1.y, p2.y}));

            const auto edge = [](const SDL_FPoint& a, const SDL_FPoint& b, float x, float y)
            {
                return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
            };

            for (int y = static_cast<int>(minY); y <= static_cast<int>(maxY); ++y)
            {
                for (int x = static_cast<int>(minX); x <= static_cast<int>(maxX); ++x)
                {
                    const float w0 = edge(p1, p2, static_cast<float>(x), static_cast<float>(y));
                    const float w1 = edge(p2, p0, static_cast<float>(x), static_cast<float>(y));
                    const float w2 = edge(p0, p1, static_cast<float>(x), static_cast<float>(y));

                    if ((w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                        (w0 <= 0 && w1 <= 0 && w2 <= 0))
                    {
                        SDL_RenderDrawPoint(renderer, x, y);
                    }
                }
            }
        }

        void draw_thick_line(SDL_Renderer* renderer,
                             const SDL_FPoint& from,
                             const SDL_FPoint& to,
                             float thickness,
                             SDL_Color color)
        {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

            const float dx = to.x - from.x;
            const float dy = to.y - from.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0f)
            {
                return;
            }

            const float nx = dx / len;
            const float ny = dy / len;
            const float px = -ny;
            const float py = nx;

            const int half = static_cast<int>(std::max(1.0f, thickness / 2.0f));

            for (int i = -half; i <= half; ++i)
            {
                const float ox = px * static_cast<float>(i);
                const float oy = py * static_cast<float>(i);
                SDL_RenderDrawLine(
                    renderer,
                    static_cast<int>(std::round(from.x + ox)),
                    static_cast<int>(std::round(from.y + oy)),
                    static_cast<int>(std::round(to.x + ox)),
                    static_cast<int>(std::round(to.y + oy)));
            }
        }

        void draw_arrow(SDL_Renderer* renderer, int fromSquare, int toSquare, SDL_Color color)
        {
            if (fromSquare == toSquare || fromSquare == -1 || toSquare == -1)
            {
                return;
            }

            const SDL_FPoint from = square_center(fromSquare);
            const SDL_FPoint to = square_center(toSquare);

            const float dx = to.x - from.x;
            const float dy = to.y - from.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0f)
            {
                return;
            }

            const float nx = dx / len;
            const float ny = dy / len;

            const float thickness = std::max(2.0f, SquareSize * 0.10f);
            const float headLen = SquareSize * 0.35f;
            const float headWidth = SquareSize * 0.25f;

            SDL_FPoint tip{to.x, to.y};
            SDL_FPoint base{to.x - nx * headLen, to.y - ny * headLen};

            SDL_FPoint p2{base.x + (-ny) * (headWidth / 2.0f), base.y + nx * (headWidth / 2.0f)};
            SDL_FPoint p3{base.x - (-ny) * (headWidth / 2.0f), base.y - nx * (headWidth / 2.0f)};

            draw_thick_line(renderer, from, base, thickness, color);
            draw_filled_triangle(renderer, tip, p2, p3, color);
        }

        void draw_circle(SDL_Renderer* renderer, int square, SDL_Color color)
        {
            if (square < 0)
            {
                return;
            }

            SDL_FPoint c = square_center(square);
            const float radius = SquareSize * 0.38f;
            const float thickness = std::max(2.0f, SquareSize * 0.06f);
            const int steps = 64;

            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            for (float r = radius - thickness / 2.0f; r <= radius + thickness / 2.0f; r += 1.0f)
            {
                for (int i = 0; i < steps; ++i)
                {
                    const float theta = static_cast<float>(i) * 2.0f * Pi /
                                        static_cast<float>(steps);
                    const float x = c.x + std::cos(theta) * r;
                    const float y = c.y + std::sin(theta) * r;
                    SDL_RenderDrawPoint(renderer, static_cast<int>(std::round(x)), static_cast<int>(std::round(y)));
                }
            }
        }

        void render_annotations(SDL_Renderer* renderer, const Annotations& annotations)
        {
            for (const Arrow& arrow : annotations.arrows)
            {
                draw_arrow(renderer, arrow.from, arrow.to, arrow.color);
            }

            for (const Circle& circle : annotations.circles)
            {
                draw_circle(renderer, circle.square, circle.color);
            }
        }

        void rebuild_play_view(Board& viewBoard,
                               const GameState& gameState,
                               PlayViewState& playState,
                               int targetPly)
        {
            viewBoard.load_fen(gameState.startFen.empty() ? StartingFen : gameState.startFen);
            const int maxPly = std::min(targetPly, static_cast<int>(gameState.movesUci.size()));
            int applied = 0;
            for (int i = 0; i < maxPly; ++i)
            {
                if (!apply_uci_move(viewBoard, gameState.movesUci[static_cast<std::size_t>(i)]))
                {
                    set_status(playState, "Failed to apply move " + std::to_string(i + 1));
                    break;
                }
                ++applied;
            }
            playState.viewPly = applied;
        }

        void recompute_play_san(GameState& gameState)
        {
            gameState.sanMoves.clear();
            if (gameState.movesUci.empty())
            {
                return;
            }

            Board tmp;
            tmp.load_fen(gameState.startFen.empty() ? StartingFen : gameState.startFen);
            gameState.sanMoves.reserve(gameState.movesUci.size());

            for (const std::string& uci : gameState.movesUci)
            {
                const std::vector<Move> legal = tmp.generate_legal_moves();
                Move matched{};
                bool found = false;
                for (const Move& m : legal)
                {
                    if (m.to_uci() == uci)
                    {
                        matched = m;
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    gameState.sanMoves.push_back(move_to_san(tmp, matched));
                    tmp.make_move(matched);
                }
                else
                {
                    gameState.sanMoves.push_back(uci);
                    apply_uci_move(tmp, uci);
                }
            }
        }

        void draw_panel_background(SDL_Renderer* renderer)
        {
            SDL_Rect panelRect{BoardPixels, 0, PanelWidth, WindowHeight};
            fill_rect(renderer, panelRect, PanelBg);
        }

        void draw_button(SDL_Renderer* renderer,
                         const SDL_Rect& rect,
                         const std::string& label,
                         bool hovered,
                         bool pressed,
                         bool enabled)
        {
            SDL_Color bg = ButtonBg;
            if (!enabled)
            {
                bg = ButtonDisabled;
            }
            else if (pressed)
            {
                bg = ButtonPressed;
            }
            else if (hovered)
            {
                bg = ButtonHover;
            }

            fill_rect(renderer, rect, bg);

            SDL_Rect outline = rect;
            SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
            SDL_RenderDrawRect(renderer, &outline);

            const int textWidth = measure_text_width(label, TextScale);
            const int textHeight = 7 * TextScale;
            const int textX = rect.x + (rect.w - textWidth) / 2;
            const int textY = rect.y + (rect.h - textHeight) / 2;
            draw_text(renderer, textX, textY, TextScale, label, TextColor);
        }
    }

    void run(Board& board)
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
            return;
        }

        const int imgFlags = IMG_INIT_PNG;
        if ((IMG_Init(imgFlags) & imgFlags) != imgFlags)
        {
            std::cerr << "IMG_Init failed: " << IMG_GetError() << '\n';
            SDL_Quit();
            return;
        }

        SDL_Window* window = SDL_CreateWindow(
            "SDL2 Chess",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WindowWidth,
            WindowHeight,
            SDL_WINDOW_SHOWN);

        if (!window)
        {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
            IMG_Quit();
            SDL_Quit();
            return;
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        if (!renderer)
        {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
            SDL_DestroyWindow(window);
            IMG_Quit();
            SDL_Quit();
            return;
        }

        SDL_Texture* boardTexture = load_texture(renderer, "assets/boards/board.png");
        if (!boardTexture)
        {
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            IMG_Quit();
            SDL_Quit();
            return;
        }

        std::unordered_map<Piece, SDL_Texture*> pieceTextures;

        for (int p = static_cast<int>(Piece::WhitePawn); p <= static_cast<int>(Piece::BlackKing); ++p)
        {
            const Piece piece = static_cast<Piece>(p);
            const std::string name = piece_texture_name(piece);
            if (name.empty())
            {
                continue;
            }

            const std::string path = "assets/pieces/" + name;
            SDL_Texture* texture = load_texture(renderer, path);
            if (texture)
            {
                pieceTextures[piece] = texture;
            }
        }

        UIMode mode = UIMode::Play;
        GameState gameState;
        gameState.startFen = board.to_fen();

        HistoryUIState historyState;
        historyState.replayBoard.load_fen(gameState.startFen);

        Annotations playAnnotations;
        Annotations historyAnnotations;
        PlayViewState playViewState;
        playViewState.viewBoard.load_fen(gameState.startFen);
        recompute_play_san(gameState);
        rebuild_play_view(
            playViewState.viewBoard,
            gameState,
            playViewState,
            static_cast<int>(gameState.movesUci.size()));

        bool running = true;
        int selectedSquare = -1;
        std::vector<Move> legalMovesForSelected;
        int mouseX = 0;
        int mouseY = 0;
        bool mouseDown = false;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        while (running)
        {
            const int panelInnerX = BoardPixels + PanelPadding;
            const int panelInnerW = PanelWidth - 2 * PanelPadding;

            const int topButtons =
                (mode == UIMode::Play) ? 3 : 2; // Play: history/new/clear, History: back/clear
            const int listStartY = PanelPadding + topButtons * (ButtonHeight + ButtonSpacing);

            const int exportButtonsHeight = ButtonHeight;
            const int totalRem = WindowHeight - listStartY -
                                 ButtonSpacing - ButtonHeight - ButtonSpacing -
                                 exportButtonsHeight - ButtonSpacing - HistoryControlsHeight;
            int gameListHeight = totalRem / 2;
            int moveListHeight = totalRem - gameListHeight;
            if (mode == UIMode::Play)
            {
                gameListHeight = 0;
                moveListHeight = totalRem;
            }

            const SDL_Rect historyListRect{panelInnerX, listStartY, panelInnerW, gameListHeight};
            const int moveHeaderY = listStartY + gameListHeight + ButtonSpacing;
            const SDL_Rect moveHeaderRect{panelInnerX, moveHeaderY, panelInnerW, ButtonHeight};
            const SDL_Rect moveListRect{
                panelInnerX,
                moveHeaderY + ButtonHeight + ButtonSpacing,
                panelInnerW,
                moveListHeight};
            const int exportButtonsY = moveListRect.y + moveListRect.h + ButtonSpacing;

            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    running = false;
                }
                else if (event.type == SDL_MOUSEMOTION)
                {
                    mouseX = event.motion.x;
                    mouseY = event.motion.y;
                    Annotations& ann = (mode == UIMode::Play) ? playAnnotations : historyAnnotations;
                    if (ann.dragging)
                    {
                        ann.dragToSquare = screen_to_square(mouseX, mouseY);
                    }
                }
                else if (event.type == SDL_MOUSEWHEEL)
                {
                    if (mode == UIMode::History)
                    {
                        if (hit_test(historyListRect, mouseX, mouseY))
                        {
                            historyState.scrollOffset -= event.wheel.y * ListRowHeight;
                            clamp_scroll(historyState, historyListRect.h);
                        }
                        else if (hit_test(moveListRect, mouseX, mouseY))
                        {
                            historyState.moveListScroll -= event.wheel.y * ListRowHeight;
                            clamp_move_scroll(historyState, moveListRect.h);
                        }
                    }
                    else
                    {
                        if (hit_test(moveListRect, mouseX, mouseY))
                        {
                            playViewState.moveListScroll -= event.wheel.y * ListRowHeight;
                            clamp_play_move_scroll(playViewState, gameState, moveListRect.h);
                        }
                    }
                }
                else if (event.type == SDL_KEYDOWN)
                {
                    const SDL_Keycode key = event.key.keysym.sym;
                    if (key == SDLK_ESCAPE)
                    {
                        running = false;
                    }
                    else if (key == SDLK_h)
                    {
                        if (mode == UIMode::Play)
                        {
                            mode = UIMode::History;
                            historyState.autoplay = false;
                            selectedSquare = -1;
                            legalMovesForSelected.clear();
                            refresh_history(historyState);
                            clamp_scroll(historyState, historyListRect.h);
                            clamp_move_scroll(historyState, moveListRect.h);
                        }
                        else
                        {
                            mode = UIMode::Play;
                            historyState.autoplay = false;
                        }
                    }
                    else if (key == SDLK_x)
                    {
                        Annotations& ann = (mode == UIMode::Play) ? playAnnotations : historyAnnotations;
                        clear_annotations(ann);
                    }
                    else if (key == SDLK_n && mode == UIMode::Play)
                    {
                        reset_game(board, gameState, selectedSquare, legalMovesForSelected);
                        recompute_play_san(gameState);
                        rebuild_play_view(playViewState.viewBoard, gameState, playViewState, 0);
                    }
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
                {
                    mouseDown = true;
                    const int clickX = event.button.x;
                    const int clickY = event.button.y;

                    if (mode == UIMode::Play)
                    {
                        const bool liveView =
                            playViewState.viewPly == static_cast<int>(gameState.movesUci.size());

                        const SDL_Rect historyBtn{panelInnerX, PanelPadding, panelInnerW, ButtonHeight};
                        const SDL_Rect newBtn{
                            panelInnerX,
                            PanelPadding + ButtonHeight + ButtonSpacing,
                            panelInnerW,
                            ButtonHeight};
                        const SDL_Rect clearBtn{
                            panelInnerX,
                            PanelPadding + 2 * (ButtonHeight + ButtonSpacing),
                            panelInnerW,
                            ButtonHeight};

                        const int controlAreaY = WindowHeight - HistoryControlsHeight + PanelPadding;
                        const int buttonsY = controlAreaY + 7 * TextScale + 10;
                        const int controlWidth = panelInnerW;
                        const int buttonWidth = (controlWidth - ButtonSpacing * 4) / 5;

                        const SDL_Rect firstBtn{panelInnerX, buttonsY, buttonWidth, ButtonHeight};
                        const SDL_Rect prevBtn{panelInnerX + (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                        const SDL_Rect liveBtn{panelInnerX + 2 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                        const SDL_Rect nextBtn{panelInnerX + 3 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                        const SDL_Rect lastBtn{panelInnerX + 4 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};

                        if (hit_test(historyBtn, clickX, clickY))
                        {
                            mode = UIMode::History;
                            historyState.autoplay = false;
                            selectedSquare = -1;
                            legalMovesForSelected.clear();
                            refresh_history(historyState);
                            clamp_scroll(historyState, historyListRect.h);
                            clamp_move_scroll(historyState, moveListRect.h);
                        }
                        else if (hit_test(newBtn, clickX, clickY))
                        {
                            reset_game(board, gameState, selectedSquare, legalMovesForSelected);
                            recompute_play_san(gameState);
                            playViewState.viewPly = 0;
                            playViewState.moveListScroll = 0;
                            playViewState.statusText.clear();
                            playViewState.statusExpireMs = 0;
                            rebuild_play_view(playViewState.viewBoard, gameState, playViewState, 0);
                        }
                        else if (hit_test(clearBtn, clickX, clickY))
                        {
                            clear_annotations(playAnnotations);
                        }
                        else if (hit_test(moveListRect, clickX, clickY))
                        {
                            const int rowY = clickY - moveListRect.y + playViewState.moveListScroll;
                            const int rowIndex = rowY / ListRowHeight;
                            const int totalMoves = static_cast<int>(gameState.movesUci.size());
                            const int totalRows = (totalMoves + 1) / 2;
                            if (rowIndex >= 0 && rowIndex < totalRows)
                            {
                                const int basePly = rowIndex * 2;
                                const bool hasBlack = basePly + 1 < totalMoves;

                                int targetPly = basePly;

                                if (hasBlack)
                                {
                                    const int labelWidth =
                                        measure_text_width(std::to_string(rowIndex + 1) + ".", TextScale) +
                                        TextScale * 2;
                                    const std::string whiteText =
                                        (basePly < static_cast<int>(gameState.sanMoves.size()))
                                            ? gameState.sanMoves[static_cast<std::size_t>(basePly)]
                                            : (basePly < static_cast<int>(gameState.movesUci.size())
                                                   ? gameState.movesUci[static_cast<std::size_t>(basePly)]
                                                   : "");
                                    const int whiteWidth = measure_text_width(whiteText, TextScale);
                                    const int whiteEnd = moveListRect.x + 8 + labelWidth + whiteWidth;
                                    if (clickX >= whiteEnd + 4)
                                    {
                                        targetPly = basePly + 1;
                                    }
                                }

                                rebuild_play_view(playViewState.viewBoard, gameState, playViewState, targetPly + 1);
                                if (playViewState.viewPly != static_cast<int>(gameState.movesUci.size()))
                                {
                                    set_status(playViewState, "Browsing (moves disabled)");
                                }
                            }
                        }
                        else if (hit_test(firstBtn, clickX, clickY))
                        {
                            rebuild_play_view(playViewState.viewBoard, gameState, playViewState, 0);
                        }
                        else if (hit_test(prevBtn, clickX, clickY))
                        {
                            const int target = std::max(0, playViewState.viewPly - 1);
                            rebuild_play_view(playViewState.viewBoard, gameState, playViewState, target);
                            if (playViewState.viewPly != static_cast<int>(gameState.movesUci.size()))
                            {
                                set_status(playViewState, "Browsing (moves disabled)");
                            }
                        }
                        else if (hit_test(liveBtn, clickX, clickY))
                        {
                            rebuild_play_view(
                                playViewState.viewBoard,
                                gameState,
                                playViewState,
                                static_cast<int>(gameState.movesUci.size()));
                        }
                        else if (hit_test(nextBtn, clickX, clickY))
                        {
                            const int target =
                                std::min(static_cast<int>(gameState.movesUci.size()),
                                         playViewState.viewPly + 1);
                            rebuild_play_view(playViewState.viewBoard, gameState, playViewState, target);
                        }
                        else if (hit_test(lastBtn, clickX, clickY))
                        {
                            rebuild_play_view(
                                playViewState.viewBoard,
                                gameState,
                                playViewState,
                                static_cast<int>(gameState.movesUci.size()));
                        }
                        else if (clickX < BoardPixels && !gameState.gameOver)
                        {
                            if (!liveView)
                            {
                                // browsing; moves disabled
                                continue;
                            }

                            const int clickedSquare = screen_to_square(clickX, clickY);
                            if (clickedSquare != -1)
                            {
                                const Piece piece = board.piece_at(clickedSquare);
                                const bool isWhiteTurn = board.side_to_move() == Color::White;

                                if (selectedSquare == -1)
                                {
                                    if (piece != Piece::None &&
                                        ((isWhiteTurn && is_white_piece(piece)) ||
                                         (!isWhiteTurn && is_black_piece(piece))))
                                    {
                                        selectedSquare = clickedSquare;
                                        legalMovesForSelected.clear();

                                        const std::vector<Move> allMoves = board.generate_legal_moves();
                                        for (const Move& move : allMoves)
                                        {
                                            if (move.from == selectedSquare)
                                            {
                                                legalMovesForSelected.push_back(move);
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    if (clickedSquare == selectedSquare)
                                    {
                                        selectedSquare = -1;
                                        legalMovesForSelected.clear();
                                    }
                                    else
                                    {
                                        Move chosenMove{};
                                        bool foundMove = false;

                                        for (const Move& move : legalMovesForSelected)
                                        {
                                            if (move.to == clickedSquare)
                                            {
                                                if ((move.flags & MoveFlagPromotion) != 0U)
                                                {
                                                    if (move.promotionPiece == Piece::WhiteQueen ||
                                                        move.promotionPiece == Piece::BlackQueen)
                                                    {
                                                        chosenMove = move;
                                                        foundMove = true;
                                                        break;
                                                    }
                                                    if (!foundMove)
                                                    {
                                                        chosenMove = move;
                                                        foundMove = true;
                                                    }
                                                }
                                                else
                                                {
                                                    chosenMove = move;
                                                    foundMove = true;
                                                    break;
                                                }
                                            }
                                        }

                                        if (foundMove)
                                        {
                                            board.make_move(chosenMove);
                                            gameState.movesUci.push_back(chosenMove.to_uci());
                                            recompute_play_san(gameState);
                                            rebuild_play_view(
                                                playViewState.viewBoard,
                                                gameState,
                                                playViewState,
                                                static_cast<int>(gameState.movesUci.size()));
                                            selectedSquare = -1;
                                            legalMovesForSelected.clear();
                                            save_if_game_over(board, gameState);

                                            if (!gameState.gameOver &&
                                                board.side_to_move() == Color::Black)
                                            {
                                                int score = 0;
                                                std::int64_t nodes = 0;
                                                int depth = 0;

                                                const Move engineMove =
                                                    find_best_move(board,
                                                                   gameState.engineDepth,
                                                                   gameState.engineTimeMs,
                                                                   score,
                                                                   nodes,
                                                                   depth);

                                                if (!(engineMove.from == 0 &&
                                                      engineMove.to == 0 &&
                                                      engineMove.movingPiece == Piece{}))
                                                {
                                                    gameState.movesUci.push_back(engineMove.to_uci());
                                                    recompute_play_san(gameState);
                                                    rebuild_play_view(
                                                        playViewState.viewBoard,
                                                        gameState,
                                                        playViewState,
                                                        static_cast<int>(gameState.movesUci.size()));
                                                    board.make_move(engineMove);
                                                    save_if_game_over(board, gameState);
                                                }
                                            }
                                        }
                                        else
                                        {
                                            if (piece != Piece::None &&
                                                ((isWhiteTurn && is_white_piece(piece)) ||
                                                 (!isWhiteTurn && is_black_piece(piece))))
                                            {
                                                selectedSquare = clickedSquare;
                                                legalMovesForSelected.clear();

                                                const std::vector<Move> allMoves = board.generate_legal_moves();
                                                for (const Move& move : allMoves)
                                                {
                                                    if (move.from == selectedSquare)
                                                    {
                                                        legalMovesForSelected.push_back(move);
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                selectedSquare = -1;
                                                legalMovesForSelected.clear();
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        const SDL_Rect backBtn{panelInnerX, PanelPadding, panelInnerW, ButtonHeight};
                        const SDL_Rect clearBtn{
                            panelInnerX,
                            PanelPadding + ButtonHeight + ButtonSpacing,
                            panelInnerW,
                            ButtonHeight};

                        if (hit_test(backBtn, clickX, clickY))
                        {
                            mode = UIMode::Play;
                            historyState.autoplay = false;
                        }
                        else if (hit_test(clearBtn, clickX, clickY))
                        {
                            clear_annotations(historyAnnotations);
                        }
                        else if (hit_test(historyListRect, clickX, clickY))
                        {
                            const int rowY = clickY - historyListRect.y + historyState.scrollOffset;
                            const int rowIndex = rowY / ListRowHeight;
                            if (rowIndex >= 0 &&
                                rowIndex < static_cast<int>(historyState.games.size()))
                            {
                                load_history_entry(historyState, rowIndex);
                                clamp_move_scroll(historyState, moveListRect.h);
                            }
                        }
                        else if (hit_test(moveHeaderRect, clickX, clickY))
                        {
                            const int toggleWidth = ButtonHeight;
                            SDL_Rect toggleRect{moveHeaderRect.x + moveHeaderRect.w - toggleWidth,
                                                moveHeaderRect.y,
                                                toggleWidth,
                                                ButtonHeight};
                            if (hit_test(toggleRect, clickX, clickY))
                            {
                                historyState.showSan = !historyState.showSan;
                                set_status(historyState, historyState.showSan ? "Showing SAN" : "Showing UCI");
                            }
                        }
                        else if (hit_test(moveListRect, clickX, clickY) && historyState.loadedValid)
                        {
                            const int rowY = clickY - moveListRect.y + historyState.moveListScroll;
                            const int rowIndex = rowY / ListRowHeight;
                            const int totalRows = (total_plies(historyState) + 1) / 2;
                            if (rowIndex >= 0 && rowIndex < totalRows)
                            {
                                const int basePly = rowIndex * 2;
                                const bool hasBlack = basePly + 1 < total_plies(historyState);

                                int targetPly = basePly;

                                if (hasBlack)
                                {
                                    const int labelWidth =
                                        measure_text_width(std::to_string(rowIndex + 1) + ".", TextScale) +
                                        TextScale * 2;
                                    const int whiteWidth =
                                        measure_text_width(
                                            historyState.showSan
                                                ? historyState.sanMoves[static_cast<std::size_t>(basePly)]
                                                : historyState.loaded.moves[static_cast<std::size_t>(basePly)],
                                            TextScale);
                                    const int whiteEnd = moveListRect.x + 8 + labelWidth + whiteWidth;
                                    if (clickX >= whiteEnd + 4)
                                    {
                                        targetPly = basePly + 1;
                                    }
                                }

                                rebuild_replay_position(historyState, targetPly + 1);
                                historyState.autoplay = false;
                            }
                        }
                        else
                        {
                            const int exportWidth = (panelInnerW - ButtonSpacing * 2) / 3;
                            const SDL_Rect exportPgnBtn{
                                panelInnerX,
                                exportButtonsY,
                                exportWidth,
                                ButtonHeight};
                            const SDL_Rect copyPgnBtn{
                                panelInnerX + exportWidth + ButtonSpacing,
                                exportButtonsY,
                                exportWidth,
                                ButtonHeight};
                            const SDL_Rect copyFenBtn{
                                panelInnerX + 2 * (exportWidth + ButtonSpacing),
                                exportButtonsY,
                                exportWidth,
                                ButtonHeight};

                            const bool hasGame = historyState.loadedValid;

                            if (hasGame && hit_test(exportPgnBtn, clickX, clickY))
                            {
                                const std::string pgn = build_pgn(historyState);
                                const std::filesystem::path dir = export_dir();
                                const std::string utcSafe = sanitize_filename(historyState.loaded.utc.empty()
                                                                                    ? "unknown"
                                                                                    : historyState.loaded.utc);
                                const std::filesystem::path path = dir / ("game_" + utcSafe + ".pgn");
                                std::ofstream out(path);
                                if (out)
                                {
                                    out << pgn;
                                    set_status(historyState, "Saved " + path.filename().string());
                                }
                                else
                                {
                                    set_status(historyState, "Failed to save PGN");
                                }
                            }
                            else if (hasGame && hit_test(copyPgnBtn, clickX, clickY))
                            {
                                const std::string pgn = build_pgn(historyState);
                                if (!pgn.empty())
                                {
                                    SDL_SetClipboardText(pgn.c_str());
                                    set_status(historyState, "Copied PGN");
                                }
                            }
                            else if (hasGame && hit_test(copyFenBtn, clickX, clickY))
                            {
                                const std::string fen = historyState.replayBoard.to_fen();
                                SDL_SetClipboardText(fen.c_str());
                                set_status(historyState, "Copied FEN");
                            }
                            else
                            {
                                const int controlAreaY = WindowHeight - HistoryControlsHeight + PanelPadding;
                                const int buttonsY = controlAreaY + 7 * TextScale + 10;
                                const int controlWidth = panelInnerW;
                                const int buttonWidth = (controlWidth - ButtonSpacing * 4) / 5;

                                const SDL_Rect firstBtn{panelInnerX, buttonsY, buttonWidth, ButtonHeight};
                                const SDL_Rect prevBtn{panelInnerX + (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                                const SDL_Rect playBtn{panelInnerX + 2 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                                const SDL_Rect nextBtn{panelInnerX + 3 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                                const SDL_Rect lastBtn{panelInnerX + 4 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};

                                const int total = total_plies(historyState);

                                if (hasGame && hit_test(firstBtn, clickX, clickY))
                                {
                                    historyState.autoplay = false;
                                    rebuild_replay_position(historyState, 0);
                                }
                                else if (hasGame && hit_test(prevBtn, clickX, clickY))
                                {
                                    historyState.autoplay = false;
                                    const int target = std::max(0, historyState.ply - 1);
                                    rebuild_replay_position(historyState, target);
                                }
                                else if (hasGame && hit_test(playBtn, clickX, clickY))
                                {
                                    historyState.autoplay = !historyState.autoplay;
                                    historyState.lastAutoTick = SDL_GetTicks();
                                }
                                else if (hasGame && hit_test(nextBtn, clickX, clickY))
                                {
                                    historyState.autoplay = false;
                                    const int target = std::min(total, historyState.ply + 1);
                                    rebuild_replay_position(historyState, target);
                                }
                                else if (hasGame && hit_test(lastBtn, clickX, clickY))
                                {
                                    historyState.autoplay = false;
                                    rebuild_replay_position(historyState, total);
                                }
                            }
                        }
                    }
                }
                else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
                {
                    mouseDown = false;
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT)
                {
                    const int clickX = event.button.x;
                    const int clickY = event.button.y;
                    if (clickX < BoardPixels && clickY < BoardPixels)
                    {
                        const int sq = screen_to_square(clickX, clickY);
                        if (sq != -1)
                        {
                            Annotations& ann = (mode == UIMode::Play) ? playAnnotations : historyAnnotations;
                            ann.dragging = true;
                            ann.dragFromSquare = sq;
                            ann.dragToSquare = sq;
                            ann.dragColor = color_from_mod(SDL_GetModState());
                        }
                    }
                }
                else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)
                {
                    Annotations& ann = (mode == UIMode::Play) ? playAnnotations : historyAnnotations;
                    if (ann.dragging)
                    {
                        const int endSquare = screen_to_square(event.button.x, event.button.y);
                        if (endSquare != -1)
                        {
                            if (endSquare == ann.dragFromSquare)
                            {
                                toggle_circle(ann, endSquare, ann.dragColor);
                            }
                            else
                            {
                                toggle_arrow(ann, ann.dragFromSquare, endSquare, ann.dragColor);
                            }
                        }

                        ann.dragging = false;
                        ann.dragFromSquare = -1;
                        ann.dragToSquare = -1;
                    }
                }
            }

            if (mode == UIMode::History && historyState.autoplay && historyState.loadedValid)
            {
                const std::uint32_t now = SDL_GetTicks();
                if (now - historyState.lastAutoTick >= AutoPlayIntervalMs)
                {
                    historyState.lastAutoTick = now;
                    const int total = total_plies(historyState);
                    if (historyState.ply < total)
                    {
                        rebuild_replay_position(historyState, historyState.ply + 1);
                    }
                    else
                    {
                        historyState.autoplay = false;
                    }
                }
            }

            if (!historyState.statusText.empty() &&
                SDL_GetTicks() > historyState.statusExpireMs)
            {
                historyState.statusText.clear();
                historyState.statusExpireMs = 0;
            }

            if (!playViewState.statusText.empty() &&
                SDL_GetTicks() > playViewState.statusExpireMs)
            {
                playViewState.statusText.clear();
                playViewState.statusExpireMs = 0;
            }

            const Board& boardToRender =
                (mode == UIMode::History)
                    ? historyState.replayBoard
                    : ((playViewState.viewPly ==
                        static_cast<int>(gameState.movesUci.size()))
                           ? static_cast<const Board&>(board)
                           : static_cast<const Board&>(playViewState.viewBoard));

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            SDL_Rect boardRect{0, 0, BoardPixels, BoardPixels};
            SDL_RenderCopy(renderer, boardTexture, nullptr, &boardRect);

            const Annotations& annRender = (mode == UIMode::Play) ? playAnnotations : historyAnnotations;
            render_annotations(renderer, annRender);

            if (annRender.dragging &&
                annRender.dragFromSquare != -1 &&
                annRender.dragToSquare != -1 &&
                annRender.dragToSquare != annRender.dragFromSquare)
            {
                SDL_Color preview = annRender.dragColor;
                preview.a = static_cast<Uint8>(std::max(50, preview.a - 60));
                draw_arrow(renderer, annRender.dragFromSquare, annRender.dragToSquare, preview);
            }

            for (int square = 0; square < 64; ++square)
            {
                const Piece piece = boardToRender.piece_at(square);
                if (piece == Piece::None)
                {
                    continue;
                }

                auto it = pieceTextures.find(piece);
                if (it == pieceTextures.end())
                {
                    continue;
                }

                SDL_Rect rect = square_rect(square);
                SDL_RenderCopy(renderer, it->second, nullptr, &rect);
            }

            if (mode == UIMode::Play && selectedSquare != -1)
            {
                SDL_Rect selRect = square_rect(selectedSquare);
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 80);
                SDL_RenderFillRect(renderer, &selRect);

                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 80);
                for (const Move& move : legalMovesForSelected)
                {
                    SDL_Rect dstRect = square_rect(move.to);
                    SDL_RenderFillRect(renderer, &dstRect);
                }
            }

            draw_panel_background(renderer);

            if (mode == UIMode::Play)
            {
                const SDL_Rect historyBtn{panelInnerX, PanelPadding, panelInnerW, ButtonHeight};
                const SDL_Rect newBtn{
                    panelInnerX,
                    PanelPadding + ButtonHeight + ButtonSpacing,
                    panelInnerW,
                    ButtonHeight};
                const SDL_Rect clearBtn{
                    panelInnerX,
                    PanelPadding + 2 * (ButtonHeight + ButtonSpacing),
                    panelInnerW,
                    ButtonHeight};
                const int controlAreaY = WindowHeight - HistoryControlsHeight + PanelPadding;
                const int buttonsY = controlAreaY + 7 * TextScale + 10;
                const int controlWidth = panelInnerW;
                const int buttonWidth = (controlWidth - ButtonSpacing * 4) / 5;

                const SDL_Rect firstBtn{panelInnerX, buttonsY, buttonWidth, ButtonHeight};
                const SDL_Rect prevBtn{panelInnerX + (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                const SDL_Rect liveBtn{panelInnerX + 2 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                const SDL_Rect nextBtn{panelInnerX + 3 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                const SDL_Rect lastBtn{panelInnerX + 4 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};

                draw_button(
                    renderer,
                    historyBtn,
                    "HISTORY",
                    hit_test(historyBtn, mouseX, mouseY),
                    hit_test(historyBtn, mouseX, mouseY) && mouseDown,
                    true);
                draw_button(
                    renderer,
                    newBtn,
                    "NEW",
                    hit_test(newBtn, mouseX, mouseY),
                    hit_test(newBtn, mouseX, mouseY) && mouseDown,
                    true);
                draw_button(
                    renderer,
                    clearBtn,
                    "CLEAR",
                    hit_test(clearBtn, mouseX, mouseY),
                    hit_test(clearBtn, mouseX, mouseY) && mouseDown,
                    true);

                SDL_Rect moveListBg = moveListRect;
                fill_rect(renderer, moveListBg, PanelBg);
                SDL_SetRenderDrawColor(renderer, 25, 25, 30, 255);
                SDL_RenderDrawRect(renderer, &moveListBg);

                const int totalMoves = static_cast<int>(gameState.movesUci.size());
                const int totalRows = (totalMoves + 1) / 2;
                int moveStartIndex = 0;
                int moveOffsetY = moveListRect.y - (playViewState.moveListScroll % ListRowHeight);
                if (playViewState.moveListScroll > 0)
                {
                    moveStartIndex = playViewState.moveListScroll / ListRowHeight;
                }

                const int activeRow =
                    (playViewState.viewPly == 0) ? -1 : (playViewState.viewPly - 1) / 2;
                auto moveTextAt = [&](int moveIdx) -> std::string
                {
                    if (moveIdx < static_cast<int>(gameState.sanMoves.size()) &&
                        !gameState.sanMoves[static_cast<std::size_t>(moveIdx)].empty())
                    {
                        return gameState.sanMoves[static_cast<std::size_t>(moveIdx)];
                    }
                    if (moveIdx < static_cast<int>(gameState.movesUci.size()))
                    {
                        return gameState.movesUci[static_cast<std::size_t>(moveIdx)];
                    }
                    return {};
                };

                for (int idx = moveStartIndex;
                     idx < totalRows && moveOffsetY < moveListRect.y + moveListRect.h;
                     ++idx)
                {
                    SDL_Rect rowRect{moveListRect.x, moveOffsetY, moveListRect.w, ListRowHeight};
                    bool hovered = hit_test(rowRect, mouseX, mouseY);
                    bool selectedRow = idx == activeRow;
                    SDL_Color rowColor = (idx % 2 == 0) ? PanelBg : ListRowAlt;
                    if (selectedRow)
                    {
                        rowColor = ListRowSel;
                    }
                    else if (hovered)
                    {
                        rowColor = ButtonHover;
                    }
                    fill_rect(renderer, rowRect, rowColor);

                    const int whiteIndex = idx * 2;
                    const int blackIndex = whiteIndex + 1;
                    const bool hasWhite = whiteIndex < totalMoves;
                    const bool hasBlack = blackIndex < totalMoves;

                    const std::string moveNumber = std::to_string(idx + 1) + ".";
                    const int textY = rowRect.y + (ListRowHeight - 7 * TextScale) / 2;
                    int cursorX = rowRect.x + 8;
                    draw_text(renderer, cursorX, textY, TextScale, moveNumber, TextColor);
                    cursorX += measure_text_width(moveNumber, TextScale) + TextScale * 2;

                    if (hasWhite)
                    {
                        const bool active = playViewState.viewPly == whiteIndex + 1;
                        draw_text(renderer, cursorX, textY, TextScale, moveTextAt(whiteIndex), active ? ButtonPressed : TextColor);
                        cursorX += measure_text_width(moveTextAt(whiteIndex), TextScale) + TextScale * 2;
                    }

                    if (hasBlack)
                    {
                        const bool active = playViewState.viewPly == blackIndex + 1;
                        draw_text(renderer, cursorX, textY, TextScale, moveTextAt(blackIndex), active ? ButtonPressed : TextColor);
                    }

                    moveOffsetY += ListRowHeight;
                }

                SDL_Rect controlRect{panelInnerX, controlAreaY, panelInnerW, HistoryControlsHeight - 2 * PanelPadding};
                fill_rect(renderer, controlRect, PanelBg);

                const int totalPlies = static_cast<int>(gameState.movesUci.size());
                const bool liveView =
                    playViewState.viewPly == static_cast<int>(gameState.movesUci.size());
                const std::string statusLine =
                    "Viewing: " + std::to_string(playViewState.viewPly) + "/" +
                    std::to_string(totalPlies) + (liveView ? " LIVE" : "");
                draw_text(renderer, controlRect.x + 6, controlAreaY, TextScale, statusLine, TextColor);
                if (!liveView)
                {
                    draw_text(renderer,
                              controlRect.x + 6,
                              controlAreaY + 8 * TextScale,
                              TextScale,
                              "Browsing (moves disabled)",
                              TextColor);
                }
                if (!playViewState.statusText.empty())
                {
                    draw_text(renderer,
                              controlRect.x + 6,
                              controlAreaY + 16 * TextScale,
                              TextScale,
                              playViewState.statusText,
                              TextColor);
                }

                draw_button(
                    renderer,
                    firstBtn,
                    "|<",
                    hit_test(firstBtn, mouseX, mouseY),
                    hit_test(firstBtn, mouseX, mouseY) && mouseDown,
                    true);
                draw_button(
                    renderer,
                    prevBtn,
                    "<",
                    hit_test(prevBtn, mouseX, mouseY),
                    hit_test(prevBtn, mouseX, mouseY) && mouseDown,
                    true);
                draw_button(
                    renderer,
                    liveBtn,
                    "LIVE",
                    hit_test(liveBtn, mouseX, mouseY),
                    hit_test(liveBtn, mouseX, mouseY) && mouseDown,
                    true);
                draw_button(
                    renderer,
                    nextBtn,
                    ">",
                    hit_test(nextBtn, mouseX, mouseY),
                    hit_test(nextBtn, mouseX, mouseY) && mouseDown,
                    true);
                draw_button(
                    renderer,
                    lastBtn,
                    ">|",
                    hit_test(lastBtn, mouseX, mouseY),
                    hit_test(lastBtn, mouseX, mouseY) && mouseDown,
                    true);
            }
            else
            {
                const SDL_Rect backBtn{panelInnerX, PanelPadding, panelInnerW, ButtonHeight};
                const SDL_Rect clearBtn{
                    panelInnerX,
                    PanelPadding + ButtonHeight + ButtonSpacing,
                    panelInnerW,
                    ButtonHeight};
                draw_button(
                    renderer,
                    backBtn,
                    "BACK",
                    hit_test(backBtn, mouseX, mouseY),
                    hit_test(backBtn, mouseX, mouseY) && mouseDown,
                    true);
                draw_button(
                    renderer,
                    clearBtn,
                    "CLEAR",
                    hit_test(clearBtn, mouseX, mouseY),
                    hit_test(clearBtn, mouseX, mouseY) && mouseDown,
                    true);

                SDL_Rect listBg = historyListRect;
                fill_rect(renderer, listBg, PanelBg);
                SDL_SetRenderDrawColor(renderer, 25, 25, 30, 255);
                SDL_RenderDrawRect(renderer, &listBg);

                int startIndex = 0;
                int offsetY = historyListRect.y - (historyState.scrollOffset % ListRowHeight);
                if (historyState.scrollOffset > 0)
                {
                    startIndex = historyState.scrollOffset / ListRowHeight;
                }

                for (int idx = startIndex;
                     idx < static_cast<int>(historyState.games.size()) &&
                     offsetY < historyListRect.y + historyListRect.h;
                     ++idx)
                {
                    SDL_Rect rowRect{historyListRect.x, offsetY, historyListRect.w, ListRowHeight};
                    const bool selected = idx == historyState.selectedIndex;
                    const bool hovered = hit_test(rowRect, mouseX, mouseY);
                    SDL_Color rowColor = (idx % 2 == 0) ? PanelBg : ListRowAlt;
                    if (selected)
                    {
                        rowColor = ListRowSel;
                    }
                    else if (hovered)
                    {
                        rowColor = ButtonHover;
                    }

                    fill_rect(renderer, rowRect, rowColor);

                    const GameMeta& meta = historyState.games[static_cast<std::size_t>(idx)];
                    const std::string label = format_utc_brief(meta.utc) + " " + meta.result;
                    const int textY = rowRect.y + (ListRowHeight - 7 * TextScale) / 2;
                    draw_text(renderer, rowRect.x + 8, textY, TextScale, label, TextColor);

                    offsetY += ListRowHeight;
                }

                fill_rect(renderer, moveHeaderRect, PanelBg);
                draw_text(renderer, moveHeaderRect.x + 4, moveHeaderRect.y + (ButtonHeight - 7 * TextScale) / 2, TextScale, "MOVES", TextColor);
                const SDL_Rect toggleRect{moveHeaderRect.x + moveHeaderRect.w - ButtonHeight,
                                          moveHeaderRect.y,
                                          ButtonHeight,
                                          ButtonHeight};
                draw_button(
                    renderer,
                    toggleRect,
                    historyState.showSan ? "SAN" : "UCI",
                    hit_test(toggleRect, mouseX, mouseY),
                    hit_test(toggleRect, mouseX, mouseY) && mouseDown,
                    historyState.loadedValid);

                SDL_Rect moveListBg = moveListRect;
                fill_rect(renderer, moveListBg, PanelBg);
                SDL_SetRenderDrawColor(renderer, 25, 25, 30, 255);
                SDL_RenderDrawRect(renderer, &moveListBg);

                const int totalMoves = total_plies(historyState);
                const int totalRows = (totalMoves + 1) / 2;
                int moveStartIndex = 0;
                int moveOffsetY = moveListRect.y - (historyState.moveListScroll % ListRowHeight);
                if (historyState.moveListScroll > 0)
                {
                    moveStartIndex = historyState.moveListScroll / ListRowHeight;
                }

                const int activeRow = (historyState.ply == 0) ? -1 : (historyState.ply - 1) / 2;
                for (int idx = moveStartIndex;
                     idx < totalRows && moveOffsetY < moveListRect.y + moveListRect.h;
                     ++idx)
                {
                    SDL_Rect rowRect{moveListRect.x, moveOffsetY, moveListRect.w, ListRowHeight};
                    bool hovered = hit_test(rowRect, mouseX, mouseY);
                    bool selectedRow = idx == activeRow;
                    SDL_Color rowColor = (idx % 2 == 0) ? PanelBg : ListRowAlt;
                    if (selectedRow)
                    {
                        rowColor = ListRowSel;
                    }
                    else if (hovered)
                    {
                        rowColor = ButtonHover;
                    }
                    fill_rect(renderer, rowRect, rowColor);

                    const int whiteIndex = idx * 2;
                    const int blackIndex = whiteIndex + 1;
                    const bool hasWhite = whiteIndex < totalMoves;
                    const bool hasBlack = blackIndex < totalMoves;

                    const std::string moveNumber = std::to_string(idx + 1) + ".";
                    const int textY = rowRect.y + (ListRowHeight - 7 * TextScale) / 2;
                    int cursorX = rowRect.x + 8;
                    draw_text(renderer, cursorX, textY, TextScale, moveNumber, TextColor);
                    cursorX += measure_text_width(moveNumber, TextScale) + TextScale * 2;

                    auto moveTextAt = [&](int moveIdx) -> std::string
                    {
                        if (historyState.showSan && moveIdx < static_cast<int>(historyState.sanMoves.size()))
                        {
                            return historyState.sanMoves[static_cast<std::size_t>(moveIdx)];
                        }
                        if (moveIdx < static_cast<int>(historyState.loaded.moves.size()))
                        {
                            return historyState.loaded.moves[static_cast<std::size_t>(moveIdx)];
                        }
                        return {};
                    };

                    if (hasWhite)
                    {
                        const bool active = historyState.ply == whiteIndex + 1;
                        draw_text(renderer, cursorX, textY, TextScale, moveTextAt(whiteIndex), active ? ButtonPressed : TextColor);
                        cursorX += measure_text_width(moveTextAt(whiteIndex), TextScale) + TextScale * 2;
                    }

                    if (hasBlack)
                    {
                        const bool active = historyState.ply == blackIndex + 1;
                        draw_text(renderer, cursorX, textY, TextScale, moveTextAt(blackIndex), active ? ButtonPressed : TextColor);
                    }

                    moveOffsetY += ListRowHeight;
                }

                const int exportWidth = (panelInnerW - ButtonSpacing * 2) / 3;
                const SDL_Rect exportPgnBtn{
                    panelInnerX,
                    exportButtonsY,
                    exportWidth,
                    ButtonHeight};
                const SDL_Rect copyPgnBtn{
                    panelInnerX + exportWidth + ButtonSpacing,
                    exportButtonsY,
                    exportWidth,
                    ButtonHeight};
                const SDL_Rect copyFenBtn{
                    panelInnerX + 2 * (exportWidth + ButtonSpacing),
                    exportButtonsY,
                    exportWidth,
                    ButtonHeight};

                draw_button(
                    renderer,
                    exportPgnBtn,
                    "EXPORT PGN",
                    hit_test(exportPgnBtn, mouseX, mouseY),
                    hit_test(exportPgnBtn, mouseX, mouseY) && mouseDown,
                    historyState.loadedValid);
                draw_button(
                    renderer,
                    copyPgnBtn,
                    "COPY PGN",
                    hit_test(copyPgnBtn, mouseX, mouseY),
                    hit_test(copyPgnBtn, mouseX, mouseY) && mouseDown,
                    historyState.loadedValid);
                draw_button(
                    renderer,
                    copyFenBtn,
                    "COPY FEN",
                    hit_test(copyFenBtn, mouseX, mouseY),
                    hit_test(copyFenBtn, mouseX, mouseY) && mouseDown,
                    historyState.loadedValid);

                const int controlAreaY = WindowHeight - HistoryControlsHeight + PanelPadding;
                const int controlAreaH = HistoryControlsHeight - 2 * PanelPadding;
                SDL_Rect controlRect{panelInnerX, controlAreaY, panelInnerW, controlAreaH};
                fill_rect(renderer, controlRect, PanelBg);

                if (!historyState.statusText.empty())
                {
                    draw_text(
                        renderer,
                        controlRect.x + 6,
                        controlAreaY,
                        TextScale,
                        historyState.statusText,
                        TextColor);
                }

                const int plyTextWidth =
                    historyState.loadedValid
                        ? measure_text_width(
                              "PLY " + std::to_string(historyState.ply) + "/" +
                                  std::to_string(total_plies(historyState)),
                              TextScale)
                        : measure_text_width("NO GAMES", TextScale);
                const int plyTextX = controlRect.x + controlRect.w - plyTextWidth - 6;
                const int plyTextY = controlAreaY;

                if (historyState.loadedValid)
                {
                    draw_text(
                        renderer,
                        plyTextX,
                        plyTextY,
                        TextScale,
                        "PLY " + std::to_string(historyState.ply) + "/" +
                            std::to_string(total_plies(historyState)),
                        TextColor);
                }
                else
                {
                    draw_text(renderer, plyTextX, plyTextY, TextScale, "NO GAMES", TextColor);
                }

                const int buttonsY = controlAreaY + 7 * TextScale + 10;
                const int controlWidth = panelInnerW;
                const int buttonWidth = (controlWidth - ButtonSpacing * 4) / 5;

                const SDL_Rect firstBtn{panelInnerX, buttonsY, buttonWidth, ButtonHeight};
                const SDL_Rect prevBtn{panelInnerX + (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                const SDL_Rect playBtn{panelInnerX + 2 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                const SDL_Rect nextBtn{panelInnerX + 3 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};
                const SDL_Rect lastBtn{panelInnerX + 4 * (buttonWidth + ButtonSpacing), buttonsY, buttonWidth, ButtonHeight};

                const bool hasGame = historyState.loadedValid;

                draw_button(
                    renderer,
                    firstBtn,
                    "|<",
                    hit_test(firstBtn, mouseX, mouseY),
                    hit_test(firstBtn, mouseX, mouseY) && mouseDown,
                    hasGame);
                draw_button(
                    renderer,
                    prevBtn,
                    "<",
                    hit_test(prevBtn, mouseX, mouseY),
                    hit_test(prevBtn, mouseX, mouseY) && mouseDown,
                    hasGame);
                draw_button(
                    renderer,
                    playBtn,
                    historyState.autoplay ? "PAUSE" : "PLAY",
                    hit_test(playBtn, mouseX, mouseY),
                    hit_test(playBtn, mouseX, mouseY) && mouseDown,
                    hasGame);
                draw_button(
                    renderer,
                    nextBtn,
                    ">",
                    hit_test(nextBtn, mouseX, mouseY),
                    hit_test(nextBtn, mouseX, mouseY) && mouseDown,
                    hasGame);
                draw_button(
                    renderer,
                    lastBtn,
                    ">|",
                    hit_test(lastBtn, mouseX, mouseY),
                    hit_test(lastBtn, mouseX, mouseY) && mouseDown,
                    hasGame);
            }

            SDL_RenderPresent(renderer);
        }

        for (auto& entry : pieceTextures)
        {
            SDL_DestroyTexture(entry.second);
        }
        SDL_DestroyTexture(boardTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
    }
}
