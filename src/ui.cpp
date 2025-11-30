#include "ui.h"

#include <SDL.h>
#include <SDL_image.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "board.h"
#include "move.h"
#include "search.h"

namespace ui
{
    namespace
    {
        constexpr int SquareSize = 80;
        constexpr int BoardPixels = SquareSize * 8;
        constexpr int WindowWidth = BoardPixels;
        constexpr int WindowHeight = BoardPixels;

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

        bool running = true;
        int selectedSquare = -1;
        std::vector<Move> legalMovesForSelected;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    running = false;
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                {
                    running = false;
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
                {
                    const int mouseX = event.button.x;
                    const int mouseY = event.button.y;
                    const int clickedSquare = screen_to_square(mouseX, mouseY);

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
                                    selectedSquare = -1;
                                    legalMovesForSelected.clear();

                                    if (board.side_to_move() == Color::Black)
                                    {
                                        int score = 0;
                                        std::int64_t nodes = 0;
                                        int depth = 0;
                                        const int engineDepth = 4;
                                        const int engineTimeMs = 2000;

                                        const Move engineMove =
                                            find_best_move(board, engineDepth, engineTimeMs, score, nodes, depth);

                                        if (!(engineMove.from == 0 &&
                                              engineMove.to == 0 &&
                                              engineMove.movingPiece == Piece{}))
                                        {
                                            board.make_move(engineMove);
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

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            SDL_Rect boardRect{0, 0, BoardPixels, BoardPixels};
            SDL_RenderCopy(renderer, boardTexture, nullptr, &boardRect);

            for (int square = 0; square < 64; ++square)
            {
                const Piece piece = board.piece_at(square);
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

            if (selectedSquare != -1)
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

