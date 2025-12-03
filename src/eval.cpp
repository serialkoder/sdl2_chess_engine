#include "eval.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

#include "board.h"
#include "move.h"

namespace
{
    constexpr int PawnValue = 100;
    constexpr int KnightValue = 320;
    constexpr int BishopValue = 330;
    constexpr int RookValue = 500;
    constexpr int QueenValue = 900;
    constexpr int MaxPhase = 24;

    constexpr int PassedPawnBonusMg[8] = {0, 5, 10, 20, 35, 60, 100, 0};
    constexpr int PassedPawnBonusEg[8] = {0, 10, 20, 40, 70, 110, 170, 0};
    constexpr int IsolatedPenaltyMg = 15;
    constexpr int IsolatedPenaltyEg = 10;
    constexpr int DoubledPenaltyMg = 20;
    constexpr int DoubledPenaltyEg = 12;
    constexpr int BackwardPenaltyMg = 12;
    constexpr int BackwardPenaltyEg = 8;

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

    constexpr int kingTableEndgame[64] = {
        -50, -30, -30, -30, -30, -30, -30, -50,
        -30, -20, -10,   0,   0, -10, -20, -30,
        -30, -10,  20,  30,  30,  20, -10, -30,
        -30, -10,  30,  40,  40,  30, -10, -30,
        -30, -10,  30,  40,  40,  30, -10, -30,
        -30, -10,  20,  30,  30,  20, -10, -30,
        -30, -30,   0,   0,   0,   0, -30, -30,
        -50, -30, -30, -30, -30, -30, -30, -50
    };

    int mirror_square(int square)
    {
        const int file = file_of(square);
        const int rank = rank_of(square);
        const int mirroredRank = 7 - rank;
        return make_square(file, mirroredRank);
    }

    int piece_phase_value(Piece piece)
    {
        switch (piece)
        {
        case Piece::WhiteKnight:
        case Piece::BlackKnight: return 1;
        case Piece::WhiteBishop:
        case Piece::BlackBishop: return 1;
        case Piece::WhiteRook:
        case Piece::BlackRook: return 2;
        case Piece::WhiteQueen:
        case Piece::BlackQueen: return 4;
        case Piece::WhitePawn:
        case Piece::BlackPawn:
        case Piece::WhiteKing:
        case Piece::BlackKing:
        case Piece::None:
        default:
            return 0;
        }
    }

    struct PhaseScore
    {
        int mg{0};
        int eg{0};
    };

    struct SideEval
    {
        PhaseScore base{};
        std::array<int, 8> pawnFileCounts{};
        std::vector<int> pawnSquares;
        std::vector<int> knightSquares;
        std::vector<int> bishopSquares;
        std::vector<int> rookSquares;
        std::vector<int> queenSquares;
        int kingSquare{-1};
    };

    PhaseScore piece_square_score(Piece piece, int square, Color color)
    {
        const int idx = (color == Color::White) ? square : mirror_square(square);
        PhaseScore score{};

        switch (piece)
        {
        case Piece::WhitePawn:
        case Piece::BlackPawn:
            score.mg = PawnValue + pawnTable[idx];
            score.eg = PawnValue + pawnTable[idx];
            break;
        case Piece::WhiteKnight:
        case Piece::BlackKnight:
            score.mg = KnightValue + knightTable[idx];
            score.eg = KnightValue + knightTable[idx];
            break;
        case Piece::WhiteBishop:
        case Piece::BlackBishop:
            score.mg = BishopValue + bishopTable[idx];
            score.eg = BishopValue + bishopTable[idx];
            break;
        case Piece::WhiteRook:
        case Piece::BlackRook:
            score.mg = RookValue + rookTable[idx];
            score.eg = RookValue + rookTable[idx];
            break;
        case Piece::WhiteQueen:
        case Piece::BlackQueen:
            score.mg = QueenValue + queenTable[idx];
            score.eg = QueenValue + queenTable[idx];
            break;
        case Piece::WhiteKing:
        case Piece::BlackKing:
            score.mg = kingTableMidgame[idx];
            score.eg = kingTableEndgame[idx];
            break;
        case Piece::None:
        default:
            break;
        }

        return score;
    }

    void accumulate_piece(Piece piece, int square, SideEval& side, int& phase)
    {
        const Color color = is_white_piece(piece) ? Color::White : Color::Black;
        const PhaseScore pst = piece_square_score(piece, square, color);

        side.base.mg += pst.mg;
        side.base.eg += pst.eg;
        phase += piece_phase_value(piece);

        switch (piece)
        {
        case Piece::WhitePawn:
        case Piece::BlackPawn:
        {
            const int file = file_of(square);
            ++side.pawnFileCounts[static_cast<std::size_t>(file)];
            side.pawnSquares.push_back(square);
            break;
        }
        case Piece::WhiteKnight:
        case Piece::BlackKnight:
            side.knightSquares.push_back(square);
            break;
        case Piece::WhiteBishop:
        case Piece::BlackBishop:
            side.bishopSquares.push_back(square);
            break;
        case Piece::WhiteRook:
        case Piece::BlackRook:
            side.rookSquares.push_back(square);
            break;
        case Piece::WhiteQueen:
        case Piece::BlackQueen:
            side.queenSquares.push_back(square);
            break;
        case Piece::WhiteKing:
        case Piece::BlackKing:
            side.kingSquare = square;
            break;
        case Piece::None:
        default:
            break;
        }
    }

    bool is_passed_pawn(const Board& board, int square, Color side)
    {
        const int file = file_of(square);
        const int rank = rank_of(square);
        const int direction = (side == Color::White) ? 1 : -1;
        const Piece enemyPawn = (side == Color::White) ? Piece::BlackPawn : Piece::WhitePawn;

        for (int r = rank + direction; r >= 0 && r < 8; r += direction)
        {
            for (int df = -1; df <= 1; ++df)
            {
                const int f = file + df;
                if (f < 0 || f > 7)
                {
                    continue;
                }

                const int target = make_square(f, r);
                if (board.piece_at(target) == enemyPawn)
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool is_isolated_pawn(const SideEval& side, int file)
    {
        const bool left = (file > 0) && side.pawnFileCounts[static_cast<std::size_t>(file - 1)] > 0;
        const bool right = (file < 7) && side.pawnFileCounts[static_cast<std::size_t>(file + 1)] > 0;
        return !left && !right;
    }

    bool is_backward_pawn(const Board& board,
                          int square,
                          Color side,
                          const SideEval& opponent)
    {
        const int file = file_of(square);
        const int rank = rank_of(square);
        const int direction = (side == Color::White) ? 1 : -1;
        const int targetRank = rank + direction;

        if (targetRank < 0 || targetRank > 7)
        {
            return false;
        }

        const int aheadSquare = make_square(file, targetRank);
        if (!is_empty_piece(board.piece_at(aheadSquare)))
        {
            return false;
        }

        for (int df : {-1, 1})
        {
            const int adjFile = file + df;
            if (adjFile < 0 || adjFile > 7)
            {
                continue;
            }

            for (int r = rank - direction; r >= 0 && r < 8; r -= direction)
            {
                const int sq = make_square(adjFile, r);
                const Piece p = board.piece_at(sq);
                if ((side == Color::White && p == Piece::WhitePawn) ||
                    (side == Color::Black && p == Piece::BlackPawn))
                {
                    return false;
                }
            }
        }

        const Piece enemyPawn = (side == Color::White) ? Piece::BlackPawn : Piece::WhitePawn;
        for (int df : {-1, 1})
        {
            const int adjFile = file + df;
            if (adjFile < 0 || adjFile > 7)
            {
                continue;
            }

            const int sq = make_square(adjFile, targetRank);
            if (board.piece_at(sq) == enemyPawn)
            {
                return true;
            }
        }

        return opponent.pawnFileCounts[static_cast<std::size_t>(file)] > 0;
    }

    PhaseScore pawn_structure_score(const Board& board,
                                    Color side,
                                    const SideEval& us,
                                    const SideEval& them)
    {
        PhaseScore score{};

        for (int file = 0; file < 8; ++file)
        {
            const int count = us.pawnFileCounts[static_cast<std::size_t>(file)];
            if (count > 1)
            {
                score.mg -= (count - 1) * DoubledPenaltyMg;
                score.eg -= (count - 1) * DoubledPenaltyEg;
            }
        }

        for (int square : us.pawnSquares)
        {
            const int file = file_of(square);
            const int relRank = (side == Color::White) ? rank_of(square) : 7 - rank_of(square);

            if (relRank >= 0 && relRank < 8 && is_passed_pawn(board, square, side))
            {
                score.mg += PassedPawnBonusMg[relRank];
                score.eg += PassedPawnBonusEg[relRank];
            }

            const bool isolated = is_isolated_pawn(us, file);
            if (isolated)
            {
                score.mg -= IsolatedPenaltyMg;
                score.eg -= IsolatedPenaltyEg;
            }
            else if (is_backward_pawn(board, square, side, them))
            {
                score.mg -= BackwardPenaltyMg;
                score.eg -= BackwardPenaltyEg;
            }
        }

        return score;
    }

    PhaseScore king_safety_score(const Board& board,
                                 Color side,
                                 const SideEval& us,
                                 const SideEval& them,
                                 int fullmoveNumber)
    {
        PhaseScore score{};
        if (us.kingSquare == -1)
        {
            return score;
        }

        const int kingFile = file_of(us.kingSquare);
        const int kingRank = rank_of(us.kingSquare);
        const int direction = (side == Color::White) ? 1 : -1;

        int pawnShield = 0;
        for (int df = -1; df <= 1; ++df)
        {
            const int file = kingFile + df;
            if (file < 0 || file > 7)
            {
                continue;
            }

            for (int step = 1; step <= 2; ++step)
            {
                const int rank = kingRank + direction * step;
                if (rank < 0 || rank > 7)
                {
                    continue;
                }

                const Piece p = board.piece_at(make_square(file, rank));
                if ((side == Color::White && p == Piece::WhitePawn) ||
                    (side == Color::Black && p == Piece::BlackPawn))
                {
                    ++pawnShield;
                    break;
                }
            }
        }

        const int missingShield = std::max(0, 3 - pawnShield);
        score.mg -= missingShield * 12;

        for (int df = -1; df <= 1; ++df)
        {
            const int file = kingFile + df;
            if (file < 0 || file > 7)
            {
                continue;
            }

            const bool friendlyPawns = us.pawnFileCounts[static_cast<std::size_t>(file)] > 0;
            const bool enemyPawns = them.pawnFileCounts[static_cast<std::size_t>(file)] > 0;

            if (!friendlyPawns && !enemyPawns)
            {
                score.mg -= 20;
            }
            else if (!friendlyPawns)
            {
                score.mg -= 12;
            }
        }

        const bool kingCastled =
            (side == Color::White &&
             (us.kingSquare == make_square(6, 0) || us.kingSquare == make_square(2, 0))) ||
            (side == Color::Black &&
             (us.kingSquare == make_square(6, 7) || us.kingSquare == make_square(2, 7)));

        if (kingCastled)
        {
            score.mg += 16;
        }
        else if (fullmoveNumber > 10)
        {
            if ((side == Color::White && kingRank == 0) ||
                (side == Color::Black && kingRank == 7))
            {
                score.mg -= 18;
            }
        }

        const auto add_threat_penalty =
            [&](const std::vector<int>& squares, int penalty)
            {
                for (int sq : squares)
                {
                    const int df = std::abs(file_of(sq) - kingFile);
                    const int dr = std::abs(rank_of(sq) - kingRank);
                    if (std::max(df, dr) <= 2)
                    {
                        score.mg -= penalty;
                    }
                }
            };

        add_threat_penalty(them.knightSquares, 6);
        add_threat_penalty(them.bishopSquares, 5);
        add_threat_penalty(them.rookSquares, 7);
        add_threat_penalty(them.queenSquares, 9);

        return score;
    }

    PhaseScore activity_score(Color side, const SideEval& us, const SideEval& them)
    {
        PhaseScore score{};

        for (int square : us.knightSquares)
        {
            const int file = file_of(square);
            const int relRank = (side == Color::White) ? rank_of(square) : 7 - rank_of(square);

            if (relRank > 1)
            {
                score.mg += 6;
            }

            if (file >= 2 && file <= 5 && relRank >= 2 && relRank <= 5)
            {
                score.mg += 8;
                score.eg += 4;
            }

            if (file == 0 || file == 7)
            {
                score.mg -= 8;
            }
        }

        for (int square : us.bishopSquares)
        {
            const int relRank = (side == Color::White) ? rank_of(square) : 7 - rank_of(square);
            if (relRank > 0)
            {
                score.mg += 5;
            }
        }

        for (int square : us.rookSquares)
        {
            const int file = file_of(square);
            const int relRank = (side == Color::White) ? rank_of(square) : 7 - rank_of(square);
            const bool friendlyPawns = us.pawnFileCounts[static_cast<std::size_t>(file)] > 0;
            const bool enemyPawns = them.pawnFileCounts[static_cast<std::size_t>(file)] > 0;

            if (!friendlyPawns && !enemyPawns)
            {
                score.mg += 20;
                score.eg += 12;
            }
            else if (!friendlyPawns)
            {
                score.mg += 12;
                score.eg += 6;
            }

            if (relRank == 6)
            {
                score.mg += 8;
                score.eg += 6;
            }
        }

        for (int square : us.queenSquares)
        {
            const int relRank = (side == Color::White) ? rank_of(square) : 7 - rank_of(square);
            if (relRank >= 5)
            {
                score.mg += 4;
            }
        }

        return score;
    }
}

int evaluate(const Board& board)
{
    SideEval white{};
    SideEval black{};
    int phase = 0;

    for (int square = 0; square < 64; ++square)
    {
        const Piece piece = board.piece_at(square);
        if (piece == Piece::None)
        {
            continue;
        }

        SideEval& side = is_white_piece(piece) ? white : black;
        accumulate_piece(piece, square, side, phase);
    }

    const int fullmoveNumber = board.fullmove_number();

    const PhaseScore whitePawn = pawn_structure_score(board, Color::White, white, black);
    const PhaseScore blackPawn = pawn_structure_score(board, Color::Black, black, white);

    const PhaseScore whiteKing = king_safety_score(board, Color::White, white, black, fullmoveNumber);
    const PhaseScore blackKing = king_safety_score(board, Color::Black, black, white, fullmoveNumber);

    const PhaseScore whiteActivity = activity_score(Color::White, white, black);
    const PhaseScore blackActivity = activity_score(Color::Black, black, white);

    int mgScore = white.base.mg + whitePawn.mg + whiteKing.mg + whiteActivity.mg -
                  (black.base.mg + blackPawn.mg + blackKing.mg + blackActivity.mg);
    int egScore = white.base.eg + whitePawn.eg + whiteKing.eg + whiteActivity.eg -
                  (black.base.eg + blackPawn.eg + blackKing.eg + blackActivity.eg);

    const int phaseClamped = std::clamp(phase, 0, MaxPhase);
    const int blended = (mgScore * phaseClamped + egScore * (MaxPhase - phaseClamped)) / MaxPhase;

    return board.side_to_move() == Color::White ? blended : -blended;
}
