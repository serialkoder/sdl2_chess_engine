#include "notation.h"

#include <algorithm>
#include <string>
#include <vector>

#include "board.h"
#include "move.h"

namespace
{
    bool is_castling(const Move& move, const Board& board)
    {
        const Piece king = (board.side_to_move() == Color::White) ? Piece::WhiteKing : Piece::BlackKing;
        if (move.movingPiece != king)
        {
            return false;
        }

        const int fromFile = file_of(move.from);
        const int toFile = file_of(move.to);
        return std::abs(toFile - fromFile) == 2;
    }

    std::string piece_letter(Piece piece)
    {
        switch (piece)
        {
        case Piece::WhiteKing:
        case Piece::BlackKing: return "K";
        case Piece::WhiteQueen:
        case Piece::BlackQueen: return "Q";
        case Piece::WhiteRook:
        case Piece::BlackRook: return "R";
        case Piece::WhiteBishop:
        case Piece::BlackBishop: return "B";
        case Piece::WhiteKnight:
        case Piece::BlackKnight: return "N";
        default:
            return "";
        }
    }

    bool same_piece_type(Piece a, Piece b)
    {
        const bool white = is_white_piece(a) && is_white_piece(b);
        const bool black = is_black_piece(a) && is_black_piece(b);
        if (!white && !black)
        {
            return false;
        }

        const Piece typeA = static_cast<Piece>(static_cast<int>(a) % 6);
        const Piece typeB = static_cast<Piece>(static_cast<int>(b) % 6);
        return typeA == typeB;
    }

    bool causes_check(Board& board, const Move& move)
    {
        Board copy = board;
        copy.make_move(move);
        return copy.is_in_check(copy.side_to_move());
    }

    bool causes_mate(Board& board, const Move& move)
    {
        Board copy = board;
        copy.make_move(move);
        if (!copy.is_in_check(copy.side_to_move()))
        {
            return false;
        }
        const auto replies = copy.generate_legal_moves();
        return replies.empty();
    }
}

std::string move_to_san(Board& positionBeforeMove, const Move& move)
{
    if (is_castling(move, positionBeforeMove))
    {
        const int toFile = file_of(move.to);
        return (toFile == 6) ? "O-O" : "O-O-O";
    }

    const bool isPawn = move.movingPiece == Piece::WhitePawn || move.movingPiece == Piece::BlackPawn;
    const bool isCapture = (move.flags & MoveFlagCapture) != 0U;

    std::string san;

    if (!isPawn)
    {
        san += piece_letter(move.movingPiece);
    }

    // Disambiguation
    if (!isPawn)
    {
        const auto legal = positionBeforeMove.generate_legal_moves();
        std::vector<Move> candidates;
        for (const auto& m : legal)
        {
            if (m.to == move.to && same_piece_type(m.movingPiece, move.movingPiece) &&
                m.from != move.from)
            {
                candidates.push_back(m);
            }
        }

        if (!candidates.empty())
        {
            const int fromFile = file_of(move.from);
            const int fromRank = rank_of(move.from);

            bool fileUnique = true;
            bool rankUnique = true;

            for (const auto& c : candidates)
            {
                if (file_of(c.from) == fromFile)
                {
                    fileUnique = false;
                }
                if (rank_of(c.from) == fromRank)
                {
                    rankUnique = false;
                }
            }

            if (fileUnique)
            {
                san += static_cast<char>('a' + fromFile);
            }
            else if (rankUnique)
            {
                san += static_cast<char>('1' + fromRank);
            }
            else
            {
                san += static_cast<char>('a' + fromFile);
                san += static_cast<char>('1' + fromRank);
            }
        }
    }

    if (isCapture)
    {
        if (isPawn && san.empty())
        {
            san += static_cast<char>('a' + file_of(move.from));
        }
        san += 'x';
    }

    san += square_to_string(move.to);

    if ((move.flags & MoveFlagPromotion) != 0U)
    {
        san += '=';
        san += piece_letter(move.promotionPiece);
    }

    if (causes_mate(positionBeforeMove, move))
    {
        san += '#';
    }
    else if (causes_check(positionBeforeMove, move))
    {
        san += '+';
    }

    return san;
}
