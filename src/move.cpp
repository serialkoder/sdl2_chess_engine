#include "move.h"

#include <cassert>
#include <cctype>

#include "board.h"

namespace
{
    char promotion_char(Piece piece)
    {
        switch (piece)
        {
        case Piece::WhiteQueen:
        case Piece::BlackQueen: return 'q';
        case Piece::WhiteRook:
        case Piece::BlackRook: return 'r';
        case Piece::WhiteBishop:
        case Piece::BlackBishop: return 'b';
        case Piece::WhiteKnight:
        case Piece::BlackKnight: return 'n';
        default: return '\0';
        }
    }
}

Move::Move(int fromSquare,
           int toSquare,
           Piece moving,
           Piece captured,
           Piece promotion,
           std::uint8_t flagsValue)
    : from(fromSquare),
      to(toSquare),
      movingPiece(moving),
      capturedPiece(captured),
      promotionPiece(promotion),
      flags(flagsValue)
{
}

std::string Move::to_uci() const
{
    std::string result;
    result.reserve(5);
    result += square_to_string(from);
    result += square_to_string(to);

    if ((flags & MoveFlagPromotion) != 0U)
    {
        const char promo = promotion_char(promotionPiece);
        if (promo != '\0')
        {
            result += promo;
        }
    }

    return result;
}

int file_of(int square)
{
    assert(square >= 0 && square < 64);
    return square % 8;
}

int rank_of(int square)
{
    assert(square >= 0 && square < 64);
    return square / 8;
}

int make_square(int file, int rank)
{
    assert(file >= 0 && file < 8);
    assert(rank >= 0 && rank < 8);
    return rank * 8 + file;
}

std::string square_to_string(int square)
{
    if (square < 0 || square >= 64)
    {
        return {};
    }

    const char fileChar = static_cast<char>('a' + file_of(square));
    const char rankChar = static_cast<char>('1' + rank_of(square));

    std::string result;
    result += fileChar;
    result += rankChar;
    return result;
}

int square_from_string(const std::string& name)
{
    if (name.size() != 2)
    {
        return -1;
    }

    const char fileChar = static_cast<char>(std::tolower(static_cast<unsigned char>(name[0])));
    const char rankChar = name[1];

    if (fileChar < 'a' || fileChar > 'h' || rankChar < '1' || rankChar > '8')
    {
        return -1;
    }

    const int file = fileChar - 'a';
    const int rank = rankChar - '1';
    return make_square(file, rank);
}
