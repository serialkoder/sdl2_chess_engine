#include "ui.h"

#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "board.h"
#include "history.h"
#include "move.h"
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

        enum class UIMode
        {
            Play,
            History
        };

        struct GameState
        {
            std::string startFen{StartingFen};
            std::vector<std::string> movesUci;
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
            GameRecord loaded{};
            bool loadedValid{false};
            int ply{0};
            bool autoplay{false};
            std::uint32_t lastAutoTick{0};
            Board replayBoard;
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

            case '-': return {5, {0b00000, 0b00000, 0b00000, 0b01110, 0b00000, 0b00000, 0b00000}};
            case '/': return {5, {0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b00000, 0b00000}};
            case ':': return {3, {0b000, 0b010, 0b000, 0b000, 0b010, 0b000, 0b000}};
            case '|': return {3, {0b010, 0b010, 0b010, 0b010, 0b010, 0b010, 0b010}};
            case '<': return {4, {0b0001, 0b0010, 0b0100, 0b1000, 0b0100, 0b0010, 0b0001}};
            case '>': return {4, {0b1000, 0b0100, 0b0010, 0b0001, 0b0010, 0b0100, 0b1000}};
            case '*': return {5, {0b00100, 0b10101, 0b01110, 0b11111, 0b01110, 0b10101, 0b00100}};
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
            int cursorX = x;
            for (char ch : text)
            {
                const Glyph glyph = glyph_for_char(ch);
                draw_glyph(renderer, cursorX, y, scale, glyph, color);
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
            rebuild_replay_position(historyState, 0);
        }

        void refresh_history(HistoryUIState& historyState)
        {
            historyState.games = history::list_games();
            historyState.scrollOffset = 0;
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

        bool running = true;
        int selectedSquare = -1;
        std::vector<Move> legalMovesForSelected;
        int mouseX = 0;
        int mouseY = 0;
        bool mouseDown = false;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        const int panelInnerX = BoardPixels + PanelPadding;
        const int panelInnerW = PanelWidth - 2 * PanelPadding;
        const int historyListY = PanelPadding + ButtonHeight + ButtonSpacing;
        const int historyListHeight = WindowHeight - historyListY - HistoryControlsHeight - ButtonSpacing;
        const SDL_Rect historyListRect{panelInnerX, historyListY, panelInnerW, historyListHeight};

        while (running)
        {
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
                }
                else if (event.type == SDL_MOUSEWHEEL)
                {
                    if (mode == UIMode::History)
                    {
                        historyState.scrollOffset -= event.wheel.y * ListRowHeight;
                        clamp_scroll(historyState, historyListRect.h);
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
                        }
                        else
                        {
                            mode = UIMode::Play;
                            historyState.autoplay = false;
                        }
                    }
                    else if (key == SDLK_n && mode == UIMode::Play)
                    {
                        reset_game(board, gameState, selectedSquare, legalMovesForSelected);
                    }
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
                {
                    mouseDown = true;
                    const int clickX = event.button.x;
                    const int clickY = event.button.y;

                    if (mode == UIMode::Play)
                    {
                        const SDL_Rect historyBtn{panelInnerX, PanelPadding, panelInnerW, ButtonHeight};
                        const SDL_Rect newBtn{
                            panelInnerX,
                            PanelPadding + ButtonHeight + ButtonSpacing,
                            panelInnerW,
                            ButtonHeight};

                        if (hit_test(historyBtn, clickX, clickY))
                        {
                            mode = UIMode::History;
                            historyState.autoplay = false;
                            selectedSquare = -1;
                            legalMovesForSelected.clear();
                            refresh_history(historyState);
                            clamp_scroll(historyState, historyListRect.h);
                        }
                        else if (hit_test(newBtn, clickX, clickY))
                        {
                            reset_game(board, gameState, selectedSquare, legalMovesForSelected);
                        }
                        else if (clickX < BoardPixels && !gameState.gameOver)
                        {
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
                        if (hit_test(backBtn, clickX, clickY))
                        {
                            mode = UIMode::Play;
                            historyState.autoplay = false;
                        }
                        else if (hit_test(historyListRect, clickX, clickY))
                        {
                            const int rowY = clickY - historyListRect.y + historyState.scrollOffset;
                            const int rowIndex = rowY / ListRowHeight;
                            if (rowIndex >= 0 &&
                                rowIndex < static_cast<int>(historyState.games.size()))
                            {
                                load_history_entry(historyState, rowIndex);
                            }
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

                            const bool hasGame = historyState.loadedValid;
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
                else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
                {
                    mouseDown = false;
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

            const Board& boardToRender =
                (mode == UIMode::History) ? historyState.replayBoard : board;

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            SDL_Rect boardRect{0, 0, BoardPixels, BoardPixels};
            SDL_RenderCopy(renderer, boardTexture, nullptr, &boardRect);

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
            }
            else
            {
                const SDL_Rect backBtn{panelInnerX, PanelPadding, panelInnerW, ButtonHeight};
                draw_button(
                    renderer,
                    backBtn,
                    "BACK",
                    hit_test(backBtn, mouseX, mouseY),
                    hit_test(backBtn, mouseX, mouseY) && mouseDown,
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

                const int controlAreaY = WindowHeight - HistoryControlsHeight + PanelPadding;
                const int controlAreaH = HistoryControlsHeight - 2 * PanelPadding;
                SDL_Rect controlRect{panelInnerX, controlAreaY, panelInnerW, controlAreaH};
                fill_rect(renderer, controlRect, PanelBg);

                const int plyTextWidth =
                    historyState.loadedValid
                        ? measure_text_width(
                              "PLY " + std::to_string(historyState.ply) + "/" +
                                  std::to_string(total_plies(historyState)),
                              TextScale)
                        : measure_text_width("NO GAMES", TextScale);
                const int plyTextX = controlRect.x + (controlRect.w - plyTextWidth) / 2;
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
