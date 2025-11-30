#include "eval.h"

#include "board.h"
#include "move.h"

namespace
{
    constexpr int PawnValue = 100;
    constexpr int KnightValue = 320;
    constexpr int BishopValue = 330;
    constexpr int RookValue = 500;
    constexpr int QueenValue = 900;

    constexpr int pawnTable[64] = {
         0,  0,  0,  0,  0,  0,  0,  0,
        10, 15, 15, 20, 20, 15, 15, 10,
         5, 10, 15, 25, 25, 15, 10,  5,
         0,  5, 10, 20, 20, 10,  5,  0,
         0,  5, 10, 15, 15, 10,  5,  0,
         0,  5,  5, 10, 10,  5,  5,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0
    };

    constexpr int knightTable[64] = {
        -50, -40, -30, -30, -30, -30, -40, -50,
        -40, -20,   0,   5,   5,   0, -20, -40,
        -30,   5,  10,  15,  15,  10,   5, -30,
        -30,   0,  15,  20,  20,  15,   0, -30,
        -30,   5,  15,  20,  20,  15,   5, -30,
        -30,   0,  10,  15,  15,  10,   0, -30,
        -40, -20,   0,   0,   0,   0, -20, -40,
        -50, -40, -30, -30, -30, -30, -40, -50
    };

    constexpr int bishopTable[64] = {
        -20, -10, -10, -10, -10, -10, -10, -20,
        -10,   0,   0,   0,   0,   0,   0, -10,
        -10,   0,   5,  10,  10,   5,   0, -10,
        -10,   5,   5,  10,  10,   5,   5, -10,
        -10,   0,  10,  10,  10,  10,   0, -10,
        -10,  10,  10,  10,  10,  10,  10, -10,
        -10,   5,   0,   0,   0,   0,   5, -10,
        -20, -10, -10, -10, -10, -10, -10, -20
    };

    constexpr int rookTable[64] = {
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
        10,  10,  10,  15,  15,  10,  10,  10,
         0,   0,   0,   0,   0,   0,   0,   0
    };

    constexpr int queenTable[64] = {
        -20, -10, -10,  -5,  -5, -10, -10, -20,
        -10,   0,   0,   0,   0,   0,   0, -10,
        -10,   0,   5,   5,   5,   5,   0, -10,
         -5,   0,   5,   5,   5,   5,   0,  -5,
          0,   0,   5,   5,   5,   5,   0,  -5,
        -10,   5,   5,   5,   5,   5,   0, -10,
        -10,   0,   5,   0,   0,   0,   0, -10,
        -20, -10, -10,  -5,  -5, -10, -10, -20
    };

    constexpr int kingTableMidgame[64] = {
        -30, -40, -40, -50, -50, -40, -40, -30,
        -30, -40, -40, -50, -50, -40, -40, -30,
        -30, -40, -40, -50, -50, -40, -40, -30,
        -30, -40, -40, -50, -50, -40, -40, -30,
        -20, -30, -30, -40, -40, -30, -30, -20,
        -10, -20, -20, -20, -20, -20, -20, -10,
         20,  20,   0,   0,   0,   0,  20,  20,
         20,  30,  10,   0,   0,  10,  30,  20
    };

    int mirror_square(int square)
    {
        const int file = file_of(square);
        const int rank = rank_of(square);
        const int mirroredRank = 7 - rank;
        return make_square(file, mirroredRank);
    }
}

int evaluate(const Board& board)
{
    int whiteScore = 0;
    int blackScore = 0;

    for (int square = 0; square < 64; ++square)
    {
        const Piece piece = board.piece_at(square);
        if (piece == Piece::None)
        {
            continue;
        }

        const int mirrored = mirror_square(square);

        switch (piece)
        {
        case Piece::WhitePawn:
            whiteScore += PawnValue + pawnTable[square];
            break;
        case Piece::BlackPawn:
            blackScore += PawnValue + pawnTable[mirrored];
            break;

        case Piece::WhiteKnight:
            whiteScore += KnightValue + knightTable[square];
            break;
        case Piece::BlackKnight:
            blackScore += KnightValue + knightTable[mirrored];
            break;

        case Piece::WhiteBishop:
            whiteScore += BishopValue + bishopTable[square];
            break;
        case Piece::BlackBishop:
            blackScore += BishopValue + bishopTable[mirrored];
            break;

        case Piece::WhiteRook:
            whiteScore += RookValue + rookTable[square];
            break;
        case Piece::BlackRook:
            blackScore += RookValue + rookTable[mirrored];
            break;

        case Piece::WhiteQueen:
            whiteScore += QueenValue + queenTable[square];
            break;
        case Piece::BlackQueen:
            blackScore += QueenValue + queenTable[mirrored];
            break;

        case Piece::WhiteKing:
            whiteScore += kingTableMidgame[square];
            break;
        case Piece::BlackKing:
            blackScore += kingTableMidgame[mirrored];
            break;

        case Piece::None:
        default:
            break;
        }
    }

    const int scoreFromWhite = whiteScore - blackScore;
    return board.side_to_move() == Color::White ? scoreFromWhite : -scoreFromWhite;
}
