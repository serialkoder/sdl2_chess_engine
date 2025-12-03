#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "move.h"

enum class Color : std::uint8_t
{
    White,
    Black
};

enum class Piece : std::uint8_t
{
    None = 0,
    WhitePawn,
    WhiteKnight,
    WhiteBishop,
    WhiteRook,
    WhiteQueen,
    WhiteKing,
    BlackPawn,
    BlackKnight,
    BlackBishop,
    BlackRook,
    BlackQueen,
    BlackKing
};

inline bool is_white_piece(Piece piece) noexcept
{
    return piece >= Piece::WhitePawn && piece <= Piece::WhiteKing;
}

inline bool is_black_piece(Piece piece) noexcept
{
    return piece >= Piece::BlackPawn && piece <= Piece::BlackKing;
}

inline bool is_empty_piece(Piece piece) noexcept
{
    return piece == Piece::None;
}

struct BoardState
{
    Color sideToMove{Color::White};
    std::uint8_t castlingRights{0};
    int enPassantSquare{-1};
    int halfmoveClock{0};
    int fullmoveNumber{1};
};

class Board
{
public:
    Board();

    void load_fen(const std::string& fen);
    [[nodiscard]] std::string to_fen() const;

    [[nodiscard]] std::vector<Move> generate_legal_moves() const;

    void make_move(const Move& move);
    void undo_move();
    void make_null_move();
    void undo_null_move();

    [[nodiscard]] Color side_to_move() const noexcept;
    [[nodiscard]] int fullmove_number() const noexcept;
    [[nodiscard]] Piece piece_at(int square) const noexcept;
    void set_piece_at(int square, Piece piece);

    [[nodiscard]] std::uint64_t zobrist_key() const noexcept;

    [[nodiscard]] bool is_in_check(Color side) const;

private:
    struct Undo
    {
        BoardState state{};
        std::uint64_t zobristKey{0};
        Piece capturedPiece{Piece::None};
        Move move;
    };

    std::array<Piece, 64> squares_{};
    BoardState state_{};
    std::uint64_t zobristKey_{0};
    std::vector<Undo> history_{};

    [[nodiscard]] std::uint64_t compute_zobrist() const;
    [[nodiscard]] std::vector<Move> generate_pseudo_legal_moves() const;
    [[nodiscard]] bool is_square_attacked(int square, Color bySide) const;
    [[nodiscard]] int find_king_square(Color side) const;
};
