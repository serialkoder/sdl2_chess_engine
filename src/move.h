#pragma once

#include <cstdint>
#include <string>

enum class Piece : std::uint8_t;

enum MoveFlags : std::uint8_t
{
    MoveFlagNone = 0,
    MoveFlagCapture = 1 << 0,
    MoveFlagDoublePawnPush = 1 << 1,
    MoveFlagEnPassant = 1 << 2,
    MoveFlagCastleKingSide = 1 << 3,
    MoveFlagCastleQueenSide = 1 << 4,
    MoveFlagPromotion = 1 << 5
};

struct Move
{
    int from{0};
    int to{0};
    Piece movingPiece{};
    Piece capturedPiece{};
    Piece promotionPiece{};
    std::uint8_t flags{0};

    Move() = default;

    Move(int fromSquare,
         int toSquare,
         Piece moving,
         Piece captured = Piece{},
         Piece promotion = Piece{},
         std::uint8_t flagsValue = 0);

    [[nodiscard]] std::string to_uci() const;
};

int file_of(int square);
int rank_of(int square);
int make_square(int file, int rank);
std::string square_to_string(int square);
int square_from_string(const std::string& name);

