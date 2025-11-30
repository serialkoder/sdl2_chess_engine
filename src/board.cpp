#include "board.h"

#include <algorithm>
#include <random>
#include <sstream>

#include "move.h"

namespace
{
    constexpr std::uint8_t CastleWhiteKing = 1 << 0;
    constexpr std::uint8_t CastleWhiteQueen = 1 << 1;
    constexpr std::uint8_t CastleBlackKing = 1 << 2;
    constexpr std::uint8_t CastleBlackQueen = 1 << 3;

    std::uint64_t zobristPieces[13][64]{};
    std::uint64_t zobristCastling[16]{};
    std::uint64_t zobristEnPassant[8]{};
    std::uint64_t zobristSideToMove{0};
    bool zobristInitialized = false;

    void init_zobrist()
    {
        if (zobristInitialized)
        {
            return;
        }

        std::mt19937_64 rng(0x9e3779b97f4a7c15ULL);

        for (auto& pieceArray : zobristPieces)
        {
            for (auto& key : pieceArray)
            {
                key = rng();
            }
        }

        for (auto& key : zobristCastling)
        {
            key = rng();
        }

        for (auto& key : zobristEnPassant)
        {
            key = rng();
        }

        zobristSideToMove = rng();
        zobristInitialized = true;
    }

    Color opposite_color(Color color)
    {
        return color == Color::White ? Color::Black : Color::White;
    }

    Piece piece_from_char(char symbol)
    {
        switch (symbol)
        {
        case 'P': return Piece::WhitePawn;
        case 'N': return Piece::WhiteKnight;
        case 'B': return Piece::WhiteBishop;
        case 'R': return Piece::WhiteRook;
        case 'Q': return Piece::WhiteQueen;
        case 'K': return Piece::WhiteKing;
        case 'p': return Piece::BlackPawn;
        case 'n': return Piece::BlackKnight;
        case 'b': return Piece::BlackBishop;
        case 'r': return Piece::BlackRook;
        case 'q': return Piece::BlackQueen;
        case 'k': return Piece::BlackKing;
        default:  return Piece::None;
        }
    }

    char char_from_piece(Piece piece)
    {
        switch (piece)
        {
        case Piece::WhitePawn:   return 'P';
        case Piece::WhiteKnight: return 'N';
        case Piece::WhiteBishop: return 'B';
        case Piece::WhiteRook:   return 'R';
        case Piece::WhiteQueen:  return 'Q';
        case Piece::WhiteKing:   return 'K';
        case Piece::BlackPawn:   return 'p';
        case Piece::BlackKnight: return 'n';
        case Piece::BlackBishop: return 'b';
        case Piece::BlackRook:   return 'r';
        case Piece::BlackQueen:  return 'q';
        case Piece::BlackKing:   return 'k';
        case Piece::None:
        default:
            return ' ';
        }
    }

    bool is_pawn(Piece piece)
    {
        return piece == Piece::WhitePawn || piece == Piece::BlackPawn;
    }

    bool is_king(Piece piece)
    {
        return piece == Piece::WhiteKing || piece == Piece::BlackKing;
    }
}

Board::Board()
{
    init_zobrist();
    load_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Board::load_fen(const std::string& fen)
{
    init_zobrist();

    std::istringstream stream(fen);
    std::string placement;
    std::string side;
    std::string castling;
    std::string enPassant;
    int halfmove = 0;
    int fullmove = 1;

    stream >> placement >> side >> castling >> enPassant >> halfmove >> fullmove;

    squares_.fill(Piece::None);
    state_ = {};

    int rank = 7;
    int file = 0;

    for (char symbol : placement)
    {
        if (symbol == '/')
        {
            --rank;
            file = 0;
        }
        else if (symbol >= '1' && symbol <= '8')
        {
            file += symbol - '0';
        }
        else
        {
            Piece piece = piece_from_char(symbol);
            const int square = rank * 8 + file;
            if (square >= 0 && square < 64)
            {
                squares_[static_cast<std::size_t>(square)] = piece;
            }
            ++file;
        }
    }

    state_.sideToMove = (side == "b") ? Color::Black : Color::White;

    state_.castlingRights = 0;
    if (castling.find('K') != std::string::npos) state_.castlingRights |= CastleWhiteKing;
    if (castling.find('Q') != std::string::npos) state_.castlingRights |= CastleWhiteQueen;
    if (castling.find('k') != std::string::npos) state_.castlingRights |= CastleBlackKing;
    if (castling.find('q') != std::string::npos) state_.castlingRights |= CastleBlackQueen;

    state_.enPassantSquare = -1;
    if (enPassant != "-" && !enPassant.empty())
    {
        state_.enPassantSquare = square_from_string(enPassant);
    }

    state_.halfmoveClock = halfmove;
    state_.fullmoveNumber = fullmove;

    zobristKey_ = compute_zobrist();
    history_.clear();
}

std::string Board::to_fen() const
{
    std::ostringstream stream;

    for (int rank = 7; rank >= 0; --rank)
    {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file)
        {
            const int square = rank * 8 + file;
            const Piece piece = squares_[static_cast<std::size_t>(square)];
            if (piece == Piece::None)
            {
                ++emptyCount;
            }
            else
            {
                if (emptyCount > 0)
                {
                    stream << emptyCount;
                    emptyCount = 0;
                }
                stream << char_from_piece(piece);
            }
        }
        if (emptyCount > 0)
        {
            stream << emptyCount;
        }
        if (rank > 0)
        {
            stream << '/';
        }
    }

    stream << ' ';
    stream << (state_.sideToMove == Color::White ? 'w' : 'b');
    stream << ' ';

    if (state_.castlingRights == 0)
    {
        stream << '-';
    }
    else
    {
        if (state_.castlingRights & CastleWhiteKing) stream << 'K';
        if (state_.castlingRights & CastleWhiteQueen) stream << 'Q';
        if (state_.castlingRights & CastleBlackKing) stream << 'k';
        if (state_.castlingRights & CastleBlackQueen) stream << 'q';
    }

    stream << ' ';
    if (state_.enPassantSquare == -1)
    {
        stream << '-';
    }
    else
    {
        stream << square_to_string(state_.enPassantSquare);
    }

    stream << ' ';
    stream << state_.halfmoveClock;
    stream << ' ';
    stream << state_.fullmoveNumber;

    return stream.str();
}

std::vector<Move> Board::generate_legal_moves() const
{
    std::vector<Move> legalMoves;
    const std::vector<Move> pseudoMoves = generate_pseudo_legal_moves();

    legalMoves.reserve(pseudoMoves.size());

    for (const Move& move : pseudoMoves)
    {
        Board copy = *this;
        const Color movingSide = copy.side_to_move();
        copy.make_move(move);
        if (!copy.is_in_check(movingSide))
        {
            legalMoves.push_back(move);
        }
    }

    return legalMoves;
}

void Board::make_move(const Move& move)
{
    Undo undo;
    undo.state = state_;
    undo.zobristKey = zobristKey_;
    undo.capturedPiece = move.capturedPiece;
    undo.move = move;
    history_.push_back(undo);

    const Color movingSide = state_.sideToMove;

    if (movingSide == Color::Black)
    {
        ++state_.fullmoveNumber;
    }

    if (is_pawn(move.movingPiece) || move.capturedPiece != Piece::None)
    {
        state_.halfmoveClock = 0;
    }
    else
    {
        ++state_.halfmoveClock;
    }

    state_.enPassantSquare = -1;

    const int from = move.from;
    const int to = move.to;

    Piece movingPiece = move.movingPiece;

    if (move.flags & MoveFlagEnPassant)
    {
        if (movingSide == Color::White)
        {
            const int captureSquare = to - 8;
            if (captureSquare >= 0 && captureSquare < 64)
            {
                squares_[static_cast<std::size_t>(captureSquare)] = Piece::None;
            }
        }
        else
        {
            const int captureSquare = to + 8;
            if (captureSquare >= 0 && captureSquare < 64)
            {
                squares_[static_cast<std::size_t>(captureSquare)] = Piece::None;
            }
        }
    }

    if (move.flags & MoveFlagCastleKingSide)
    {
        if (movingSide == Color::White)
        {
            const int rookFrom = make_square(7, 0);
            const int rookTo = make_square(5, 0);
            squares_[static_cast<std::size_t>(rookTo)] = squares_[static_cast<std::size_t>(rookFrom)];
            squares_[static_cast<std::size_t>(rookFrom)] = Piece::None;
        }
        else
        {
            const int rookFrom = make_square(7, 7);
            const int rookTo = make_square(5, 7);
            squares_[static_cast<std::size_t>(rookTo)] = squares_[static_cast<std::size_t>(rookFrom)];
            squares_[static_cast<std::size_t>(rookFrom)] = Piece::None;
        }
    }
    else if (move.flags & MoveFlagCastleQueenSide)
    {
        if (movingSide == Color::White)
        {
            const int rookFrom = make_square(0, 0);
            const int rookTo = make_square(3, 0);
            squares_[static_cast<std::size_t>(rookTo)] = squares_[static_cast<std::size_t>(rookFrom)];
            squares_[static_cast<std::size_t>(rookFrom)] = Piece::None;
        }
        else
        {
            const int rookFrom = make_square(0, 7);
            const int rookTo = make_square(3, 7);
            squares_[static_cast<std::size_t>(rookTo)] = squares_[static_cast<std::size_t>(rookFrom)];
            squares_[static_cast<std::size_t>(rookFrom)] = Piece::None;
        }
    }

    squares_[static_cast<std::size_t>(from)] = Piece::None;
    Piece placedPiece = movingPiece;
    if (move.flags & MoveFlagPromotion)
    {
        placedPiece = move.promotionPiece;
    }
    squares_[static_cast<std::size_t>(to)] = placedPiece;

    const int fromFile = file_of(from);
    const int fromRank = rank_of(from);

    if (movingPiece == Piece::WhiteKing)
    {
        state_.castlingRights &= static_cast<std::uint8_t>(~(CastleWhiteKing | CastleWhiteQueen));
    }
    else if (movingPiece == Piece::BlackKing)
    {
        state_.castlingRights &= static_cast<std::uint8_t>(~(CastleBlackKing | CastleBlackQueen));
    }

    if (movingPiece == Piece::WhiteRook)
    {
        if (fromFile == 0 && fromRank == 0)
        {
            state_.castlingRights &= static_cast<std::uint8_t>(~CastleWhiteQueen);
        }
        else if (fromFile == 7 && fromRank == 0)
        {
            state_.castlingRights &= static_cast<std::uint8_t>(~CastleWhiteKing);
        }
    }
    else if (movingPiece == Piece::BlackRook)
    {
        if (fromFile == 0 && fromRank == 7)
        {
            state_.castlingRights &= static_cast<std::uint8_t>(~CastleBlackQueen);
        }
        else if (fromFile == 7 && fromRank == 7)
        {
            state_.castlingRights &= static_cast<std::uint8_t>(~CastleBlackKing);
        }
    }

    if (move.capturedPiece == Piece::WhiteRook)
    {
        const int toFile = file_of(to);
        const int toRank = rank_of(to);
        if (toFile == 0 && toRank == 0)
        {
            state_.castlingRights &= static_cast<std::uint8_t>(~CastleWhiteQueen);
        }
        else if (toFile == 7 && toRank == 0)
        {
            state_.castlingRights &= static_cast<std::uint8_t>(~CastleWhiteKing);
        }
    }
    else if (move.capturedPiece == Piece::BlackRook)
    {
        const int toFile = file_of(to);
        const int toRank = rank_of(to);
        if (toFile == 0 && toRank == 7)
        {
            state_.castlingRights &= static_cast<std::uint8_t>(~CastleBlackQueen);
        }
        else if (toFile == 7 && toRank == 7)
        {
            state_.castlingRights &= static_cast<std::uint8_t>(~CastleBlackKing);
        }
    }

    if (is_pawn(movingPiece) && (move.flags & MoveFlagDoublePawnPush))
    {
        if (movingSide == Color::White)
        {
            state_.enPassantSquare = from + 8;
        }
        else
        {
            state_.enPassantSquare = from - 8;
        }
    }

    state_.sideToMove = opposite_color(state_.sideToMove);

    zobristKey_ = compute_zobrist();
}

void Board::undo_move()
{
    if (history_.empty())
    {
        return;
    }

    const Undo undo = history_.back();
    history_.pop_back();

    const Move& move = undo.move;
    const Piece movingPiece = move.movingPiece;

    const Color movingSide = opposite_color(state_.sideToMove);

    const int from = move.from;
    const int to = move.to;

    if (move.flags & MoveFlagCastleKingSide)
    {
        if (movingSide == Color::White)
        {
            const int rookFrom = make_square(7, 0);
            const int rookTo = make_square(5, 0);
            squares_[static_cast<std::size_t>(rookFrom)] = squares_[static_cast<std::size_t>(rookTo)];
            squares_[static_cast<std::size_t>(rookTo)] = Piece::None;
        }
        else
        {
            const int rookFrom = make_square(7, 7);
            const int rookTo = make_square(5, 7);
            squares_[static_cast<std::size_t>(rookFrom)] = squares_[static_cast<std::size_t>(rookTo)];
            squares_[static_cast<std::size_t>(rookTo)] = Piece::None;
        }
    }
    else if (move.flags & MoveFlagCastleQueenSide)
    {
        if (movingSide == Color::White)
        {
            const int rookFrom = make_square(0, 0);
            const int rookTo = make_square(3, 0);
            squares_[static_cast<std::size_t>(rookFrom)] = squares_[static_cast<std::size_t>(rookTo)];
            squares_[static_cast<std::size_t>(rookTo)] = Piece::None;
        }
        else
        {
            const int rookFrom = make_square(0, 7);
            const int rookTo = make_square(3, 7);
            squares_[static_cast<std::size_t>(rookFrom)] = squares_[static_cast<std::size_t>(rookTo)];
            squares_[static_cast<std::size_t>(rookTo)] = Piece::None;
        }
    }

    if (move.flags & MoveFlagEnPassant)
    {
        squares_[static_cast<std::size_t>(from)] = movingPiece;
        squares_[static_cast<std::size_t>(to)] = Piece::None;

        if (movingSide == Color::White)
        {
            const int captureSquare = to - 8;
            squares_[static_cast<std::size_t>(captureSquare)] = undo.capturedPiece;
        }
        else
        {
            const int captureSquare = to + 8;
            squares_[static_cast<std::size_t>(captureSquare)] = undo.capturedPiece;
        }
    }
    else
    {
        squares_[static_cast<std::size_t>(from)] = movingPiece;
        if (move.flags & MoveFlagPromotion)
        {
            squares_[static_cast<std::size_t>(to)] = undo.capturedPiece;
        }
        else
        {
            squares_[static_cast<std::size_t>(to)] = undo.capturedPiece;
        }
    }

    state_ = undo.state;
    zobristKey_ = undo.zobristKey;
}

Color Board::side_to_move() const noexcept
{
    return state_.sideToMove;
}

Piece Board::piece_at(int square) const noexcept
{
    if (square < 0 || square >= 64)
    {
        return Piece::None;
    }
    return squares_[static_cast<std::size_t>(square)];
}

void Board::set_piece_at(int square, Piece piece)
{
    if (square < 0 || square >= 64)
    {
        return;
    }
    squares_[static_cast<std::size_t>(square)] = piece;
}

std::uint64_t Board::zobrist_key() const noexcept
{
    return zobristKey_;
}

bool Board::is_in_check(Color side) const
{
    const int kingSquare = find_king_square(side);
    if (kingSquare == -1)
    {
        return false;
    }

    return is_square_attacked(kingSquare, opposite_color(side));
}

std::uint64_t Board::compute_zobrist() const
{
    std::uint64_t key = 0;

    for (int square = 0; square < 64; ++square)
    {
        const Piece piece = squares_[static_cast<std::size_t>(square)];
        if (piece != Piece::None)
        {
            const int pieceIndex = static_cast<int>(piece);
            key ^= zobristPieces[pieceIndex][square];
        }
    }

    key ^= zobristCastling[state_.castlingRights & 0x0F];

    if (state_.enPassantSquare != -1)
    {
        const int epFile = file_of(state_.enPassantSquare);
        if (epFile >= 0 && epFile < 8)
        {
            key ^= zobristEnPassant[epFile];
        }
    }

    if (state_.sideToMove == Color::Black)
    {
        key ^= zobristSideToMove;
    }

    return key;
}

std::vector<Move> Board::generate_pseudo_legal_moves() const
{
    std::vector<Move> moves;

    const Color us = state_.sideToMove;
    const Color them = opposite_color(us);

    moves.reserve(64);

    for (int square = 0; square < 64; ++square)
    {
        const Piece piece = squares_[static_cast<std::size_t>(square)];
        if (piece == Piece::None)
        {
            continue;
        }

        if (us == Color::White && !is_white_piece(piece))
        {
            continue;
        }
        if (us == Color::Black && !is_black_piece(piece))
        {
            continue;
        }

        const int file = file_of(square);
        const int rank = rank_of(square);

        if (piece == Piece::WhitePawn || piece == Piece::BlackPawn)
        {
            const int direction = (piece == Piece::WhitePawn) ? 1 : -1;
            const int startRank = (piece == Piece::WhitePawn) ? 1 : 6;
            const int promotionRank = (piece == Piece::WhitePawn) ? 6 : 1;

            const int forwardRank = rank + direction;
            if (forwardRank >= 0 && forwardRank < 8)
            {
                const int forwardSquare = make_square(file, forwardRank);
                if (piece_at(forwardSquare) == Piece::None)
                {
                    if (rank == promotionRank)
                    {
                        const Piece promotions[4] = {
                            (piece == Piece::WhitePawn) ? Piece::WhiteQueen : Piece::BlackQueen,
                            (piece == Piece::WhitePawn) ? Piece::WhiteRook : Piece::BlackRook,
                            (piece == Piece::WhitePawn) ? Piece::WhiteBishop : Piece::BlackBishop,
                            (piece == Piece::WhitePawn) ? Piece::WhiteKnight : Piece::BlackKnight};

                        for (Piece promoPiece : promotions)
                        {
                            Move move(square,
                                      forwardSquare,
                                      piece,
                                      Piece::None,
                                      promoPiece,
                                      static_cast<std::uint8_t>(MoveFlagPromotion));
                            moves.push_back(move);
                        }
                    }
                    else
                    {
                        Move move(square, forwardSquare, piece);
                        moves.push_back(move);

                        if (rank == startRank)
                        {
                            const int doubleRank = rank + 2 * direction;
                            const int doubleSquare = make_square(file, doubleRank);
                            if (piece_at(doubleSquare) == Piece::None)
                            {
                                Move doubleMove(square,
                                                doubleSquare,
                                                piece,
                                                Piece::None,
                                                Piece::None,
                                                static_cast<std::uint8_t>(MoveFlagDoublePawnPush));
                                moves.push_back(doubleMove);
                            }
                        }
                    }
                }
            }

            const int captureRank = rank + direction;
            if (captureRank >= 0 && captureRank < 8)
            {
                for (int df : {-1, 1})
                {
                    const int captureFile = file + df;
                    if (captureFile < 0 || captureFile > 7)
                    {
                        continue;
                    }
                    const int targetSquare = make_square(captureFile, captureRank);
                    const Piece targetPiece = piece_at(targetSquare);

                    const bool isEnemy =
                        (them == Color::White && is_white_piece(targetPiece)) ||
                        (them == Color::Black && is_black_piece(targetPiece));

                    if (!is_empty_piece(targetPiece) && isEnemy)
                    {
                        const int promotionRankCapture = promotionRank + direction;
                        const bool isPromotionCapture =
                            (direction == 1 && captureRank == 7) ||
                            (direction == -1 && captureRank == 0);

                        if (isPromotionCapture)
                        {
                            const Piece promotions[4] = {
                                (piece == Piece::WhitePawn) ? Piece::WhiteQueen : Piece::BlackQueen,
                                (piece == Piece::WhitePawn) ? Piece::WhiteRook : Piece::BlackRook,
                                (piece == Piece::WhitePawn) ? Piece::WhiteBishop : Piece::BlackBishop,
                                (piece == Piece::WhitePawn) ? Piece::WhiteKnight : Piece::BlackKnight};

                            for (Piece promoPiece : promotions)
                            {
                                Move move(square,
                                          targetSquare,
                                          piece,
                                          targetPiece,
                                          promoPiece,
                                          static_cast<std::uint8_t>(MoveFlagCapture | MoveFlagPromotion));
                                moves.push_back(move);
                            }
                        }
                        else
                        {
                            Move move(square,
                                      targetSquare,
                                      piece,
                                      targetPiece,
                                      Piece::None,
                                      static_cast<std::uint8_t>(MoveFlagCapture));
                            moves.push_back(move);
                        }
                    }

                    if (state_.enPassantSquare == targetSquare)
                    {
                        Move move(square,
                                  targetSquare,
                                  piece,
                                  (piece == Piece::WhitePawn) ? Piece::BlackPawn : Piece::WhitePawn,
                                  Piece::None,
                                  static_cast<std::uint8_t>(MoveFlagEnPassant | MoveFlagCapture));
                        moves.push_back(move);
                    }
                }
            }
        }
        else if (piece == Piece::WhiteKnight || piece == Piece::BlackKnight)
        {
            constexpr int knightDeltas[8][2] = {
                {1, 2},  {2, 1},  {2, -1}, {1, -2},
                {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};

            for (const auto& delta : knightDeltas)
            {
                const int targetFile = file + delta[0];
                const int targetRank = rank + delta[1];
                if (targetFile < 0 || targetFile > 7 || targetRank < 0 || targetRank > 7)
                {
                    continue;
                }
                const int targetSquare = make_square(targetFile, targetRank);
                const Piece targetPiece = piece_at(targetSquare);
                if (is_empty_piece(targetPiece))
                {
                    moves.emplace_back(square, targetSquare, piece);
                }
                else
                {
                    const bool isEnemy =
                        (them == Color::White && is_white_piece(targetPiece)) ||
                        (them == Color::Black && is_black_piece(targetPiece));
                    if (isEnemy)
                    {
                        moves.emplace_back(square,
                                           targetSquare,
                                           piece,
                                           targetPiece,
                                           Piece::None,
                                           static_cast<std::uint8_t>(MoveFlagCapture));
                    }
                }
            }
        }
        else if (piece == Piece::WhiteBishop || piece == Piece::BlackBishop ||
                 piece == Piece::WhiteRook || piece == Piece::BlackRook ||
                 piece == Piece::WhiteQueen || piece == Piece::BlackQueen)
        {
            constexpr int bishopDirections[4][2] = {
                {1, 1},  {1, -1}, {-1, 1}, {-1, -1}};
            constexpr int rookDirections[4][2] = {
                {1, 0},  {-1, 0}, {0, 1},  {0, -1}};

            const bool isBishopLike =
                piece == Piece::WhiteBishop || piece == Piece::BlackBishop ||
                piece == Piece::WhiteQueen || piece == Piece::BlackQueen;
            const bool isRookLike =
                piece == Piece::WhiteRook || piece == Piece::BlackRook ||
                piece == Piece::WhiteQueen || piece == Piece::BlackQueen;

            if (isBishopLike)
            {
                for (const auto& direction : bishopDirections)
                {
                    int currentFile = file + direction[0];
                    int currentRank = rank + direction[1];
                    while (currentFile >= 0 && currentFile < 8 &&
                           currentRank >= 0 && currentRank < 8)
                    {
                        const int targetSquare = make_square(currentFile, currentRank);
                        const Piece targetPiece = piece_at(targetSquare);
                        if (is_empty_piece(targetPiece))
                        {
                            moves.emplace_back(square, targetSquare, piece);
                        }
                        else
                        {
                            const bool isEnemy =
                                (them == Color::White && is_white_piece(targetPiece)) ||
                                (them == Color::Black && is_black_piece(targetPiece));
                            if (isEnemy)
                            {
                                moves.emplace_back(square,
                                                   targetSquare,
                                                   piece,
                                                   targetPiece,
                                                   Piece::None,
                                                   static_cast<std::uint8_t>(MoveFlagCapture));
                            }
                            break;
                        }
                        currentFile += direction[0];
                        currentRank += direction[1];
                    }
                }
            }

            if (isRookLike)
            {
                for (const auto& direction : rookDirections)
                {
                    int currentFile = file + direction[0];
                    int currentRank = rank + direction[1];
                    while (currentFile >= 0 && currentFile < 8 &&
                           currentRank >= 0 && currentRank < 8)
                    {
                        const int targetSquare = make_square(currentFile, currentRank);
                        const Piece targetPiece = piece_at(targetSquare);
                        if (is_empty_piece(targetPiece))
                        {
                            moves.emplace_back(square, targetSquare, piece);
                        }
                        else
                        {
                            const bool isEnemy =
                                (them == Color::White && is_white_piece(targetPiece)) ||
                                (them == Color::Black && is_black_piece(targetPiece));
                            if (isEnemy)
                            {
                                moves.emplace_back(square,
                                                   targetSquare,
                                                   piece,
                                                   targetPiece,
                                                   Piece::None,
                                                   static_cast<std::uint8_t>(MoveFlagCapture));
                            }
                            break;
                        }
                        currentFile += direction[0];
                        currentRank += direction[1];
                    }
                }
            }
        }
        else if (piece == Piece::WhiteKing || piece == Piece::BlackKing)
        {
            constexpr int kingDeltas[8][2] = {
                {1, 0},  {1, 1},  {0, 1},  {-1, 1},
                {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};

            for (const auto& delta : kingDeltas)
            {
                const int targetFile = file + delta[0];
                const int targetRank = rank + delta[1];
                if (targetFile < 0 || targetFile > 7 || targetRank < 0 || targetRank > 7)
                {
                    continue;
                }
                const int targetSquare = make_square(targetFile, targetRank);
                const Piece targetPiece = piece_at(targetSquare);

                if (is_empty_piece(targetPiece))
                {
                    moves.emplace_back(square, targetSquare, piece);
                }
                else
                {
                    const bool isEnemy =
                        (them == Color::White && is_white_piece(targetPiece)) ||
                        (them == Color::Black && is_black_piece(targetPiece));
                    if (isEnemy)
                    {
                        moves.emplace_back(square,
                                           targetSquare,
                                           piece,
                                           targetPiece,
                                           Piece::None,
                                           static_cast<std::uint8_t>(MoveFlagCapture));
                    }
                }
            }

            const bool isWhite = piece == Piece::WhiteKing;
            const int rankHome = isWhite ? 0 : 7;
            const int kingFile = 4;

            if (isWhite)
            {
                if (state_.castlingRights & CastleWhiteKing)
                {
                    const int fSquare = make_square(5, rankHome);
                    const int gSquare = make_square(6, rankHome);
                    if (piece_at(fSquare) == Piece::None && piece_at(gSquare) == Piece::None)
                    {
                        if (!is_square_attacked(make_square(4, rankHome), them) &&
                            !is_square_attacked(fSquare, them) &&
                            !is_square_attacked(gSquare, them))
                        {
                            moves.emplace_back(square,
                                               gSquare,
                                               piece,
                                               Piece::None,
                                               Piece::None,
                                               static_cast<std::uint8_t>(MoveFlagCastleKingSide));
                        }
                    }
                }

                if (state_.castlingRights & CastleWhiteQueen)
                {
                    const int dSquare = make_square(3, rankHome);
                    const int cSquare = make_square(2, rankHome);
                    const int bSquare = make_square(1, rankHome);
                    if (piece_at(dSquare) == Piece::None &&
                        piece_at(cSquare) == Piece::None &&
                        piece_at(bSquare) == Piece::None)
                    {
                        if (!is_square_attacked(make_square(4, rankHome), them) &&
                            !is_square_attacked(dSquare, them) &&
                            !is_square_attacked(cSquare, them))
                        {
                            moves.emplace_back(square,
                                               cSquare,
                                               piece,
                                               Piece::None,
                                               Piece::None,
                                               static_cast<std::uint8_t>(MoveFlagCastleQueenSide));
                        }
                    }
                }
            }
            else
            {
                if (state_.castlingRights & CastleBlackKing)
                {
                    const int fSquare = make_square(5, rankHome);
                    const int gSquare = make_square(6, rankHome);
                    if (piece_at(fSquare) == Piece::None && piece_at(gSquare) == Piece::None)
                    {
                        if (!is_square_attacked(make_square(4, rankHome), them) &&
                            !is_square_attacked(fSquare, them) &&
                            !is_square_attacked(gSquare, them))
                        {
                            moves.emplace_back(square,
                                               gSquare,
                                               piece,
                                               Piece::None,
                                               Piece::None,
                                               static_cast<std::uint8_t>(MoveFlagCastleKingSide));
                        }
                    }
                }

                if (state_.castlingRights & CastleBlackQueen)
                {
                    const int dSquare = make_square(3, rankHome);
                    const int cSquare = make_square(2, rankHome);
                    const int bSquare = make_square(1, rankHome);
                    if (piece_at(dSquare) == Piece::None &&
                        piece_at(cSquare) == Piece::None &&
                        piece_at(bSquare) == Piece::None)
                    {
                        if (!is_square_attacked(make_square(4, rankHome), them) &&
                            !is_square_attacked(dSquare, them) &&
                            !is_square_attacked(cSquare, them))
                        {
                            moves.emplace_back(square,
                                               cSquare,
                                               piece,
                                               Piece::None,
                                               Piece::None,
                                               static_cast<std::uint8_t>(MoveFlagCastleQueenSide));
                        }
                    }
                }
            }
        }
    }

    return moves;
}

bool Board::is_square_attacked(int square, Color bySide) const
{
    const int file = file_of(square);
    const int rank = rank_of(square);

    const int pawnDirection = (bySide == Color::White) ? -1 : 1;
    const int pawnRank = rank + pawnDirection;
    if (pawnRank >= 0 && pawnRank < 8)
    {
        for (int df : {-1, 1})
        {
            const int pawnFile = file + df;
            if (pawnFile < 0 || pawnFile > 7)
            {
                continue;
            }
            const int pawnSquare = make_square(pawnFile, pawnRank);
            const Piece pawnPiece = piece_at(pawnSquare);
            if (bySide == Color::White && pawnPiece == Piece::WhitePawn)
            {
                return true;
            }
            if (bySide == Color::Black && pawnPiece == Piece::BlackPawn)
            {
                return true;
            }
        }
    }

    constexpr int knightDeltas[8][2] = {
        {1, 2},  {2, 1},  {2, -1}, {1, -2},
        {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};

    for (const auto& delta : knightDeltas)
    {
        const int knightFile = file + delta[0];
        const int knightRank = rank + delta[1];
        if (knightFile < 0 || knightFile > 7 || knightRank < 0 || knightRank > 7)
        {
            continue;
        }
        const int knightSquare = make_square(knightFile, knightRank);
        const Piece knightPiece = piece_at(knightSquare);
        if (bySide == Color::White && knightPiece == Piece::WhiteKnight)
        {
            return true;
        }
        if (bySide == Color::Black && knightPiece == Piece::BlackKnight)
        {
            return true;
        }
    }

    constexpr int bishopDirections[4][2] = {
        {1, 1},  {1, -1}, {-1, 1}, {-1, -1}};
    constexpr int rookDirections[4][2] = {
        {1, 0},  {-1, 0}, {0, 1},  {0, -1}};

    for (const auto& direction : bishopDirections)
    {
        int currentFile = file + direction[0];
        int currentRank = rank + direction[1];
        while (currentFile >= 0 && currentFile < 8 &&
               currentRank >= 0 && currentRank < 8)
        {
            const int targetSquare = make_square(currentFile, currentRank);
            const Piece targetPiece = piece_at(targetSquare);
            if (!is_empty_piece(targetPiece))
            {
                if (bySide == Color::White &&
                    (targetPiece == Piece::WhiteBishop || targetPiece == Piece::WhiteQueen))
                {
                    return true;
                }
                if (bySide == Color::Black &&
                    (targetPiece == Piece::BlackBishop || targetPiece == Piece::BlackQueen))
                {
                    return true;
                }
                break;
            }
            currentFile += direction[0];
            currentRank += direction[1];
        }
    }

    for (const auto& direction : rookDirections)
    {
        int currentFile = file + direction[0];
        int currentRank = rank + direction[1];
        while (currentFile >= 0 && currentFile < 8 &&
               currentRank >= 0 && currentRank < 8)
        {
            const int targetSquare = make_square(currentFile, currentRank);
            const Piece targetPiece = piece_at(targetSquare);
            if (!is_empty_piece(targetPiece))
            {
                if (bySide == Color::White &&
                    (targetPiece == Piece::WhiteRook || targetPiece == Piece::WhiteQueen))
                {
                    return true;
                }
                if (bySide == Color::Black &&
                    (targetPiece == Piece::BlackRook || targetPiece == Piece::BlackQueen))
                {
                    return true;
                }
                break;
            }
            currentFile += direction[0];
            currentRank += direction[1];
        }
    }

    for (int dr = -1; dr <= 1; ++dr)
    {
        for (int df = -1; df <= 1; ++df)
        {
            if (dr == 0 && df == 0)
            {
                continue;
            }
            const int kingFile = file + df;
            const int kingRank = rank + dr;
            if (kingFile < 0 || kingFile > 7 || kingRank < 0 || kingRank > 7)
            {
                continue;
            }
            const int kingSquare = make_square(kingFile, kingRank);
            const Piece kingPiece = piece_at(kingSquare);
            if (bySide == Color::White && kingPiece == Piece::WhiteKing)
            {
                return true;
            }
            if (bySide == Color::Black && kingPiece == Piece::BlackKing)
            {
                return true;
            }
        }
    }

    return false;
}

int Board::find_king_square(Color side) const
{
    const Piece kingPiece = (side == Color::White) ? Piece::WhiteKing : Piece::BlackKing;
    for (int square = 0; square < 64; ++square)
    {
        if (squares_[static_cast<std::size_t>(square)] == kingPiece)
        {
            return square;
        }
    }
    return -1;
}
