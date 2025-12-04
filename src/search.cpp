#include "search.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <vector>

#include "board.h"
#include "eval.h"
#include "move.h"

namespace
{
    constexpr int MateValue = 30000;
    constexpr int MateThreshold = MateValue - 1024;
    constexpr int InfinityScore = std::numeric_limits<int>::max() / 16;
    constexpr int MaxSearchDepth = 64;
    constexpr std::size_t TTSize = 1ULL << 20;

    enum class NodeType : std::uint8_t
    {
        Exact,
        LowerBound,
        UpperBound
    };

    struct TTEntry
    {
        std::uint64_t key{0};
        int depth{0};
        int score{0};
        NodeType nodeType{NodeType::Exact};
        Move bestMove{};
        bool valid{false};
    };

    using TranspositionTable = std::array<TTEntry, TTSize>;
    TranspositionTable transpositionTable{};

    struct SearchContext
    {
        std::chrono::steady_clock::time_point startTime{};
        int timeLimitMs{0};
        bool stopped{false};
    };

    struct KillerMoves
    {
        Move primary{};
        Move secondary{};
    };

    std::array<KillerMoves, MaxSearchDepth> killerMoves{};
    int historyHeuristic[2][64][64]{};

    int piece_value(Piece piece)
    {
        switch (piece)
        {
        case Piece::WhitePawn:
        case Piece::BlackPawn: return 100;
        case Piece::WhiteKnight:
        case Piece::BlackKnight: return 320;
        case Piece::WhiteBishop:
        case Piece::BlackBishop: return 330;
        case Piece::WhiteRook:
        case Piece::BlackRook: return 500;
        case Piece::WhiteQueen:
        case Piece::BlackQueen: return 900;
        case Piece::WhiteKing:
        case Piece::BlackKing: return MateValue;
        case Piece::None:
        default:
            return 0;
        }
    }

    bool same_move(const Move& lhs, const Move& rhs)
    {
        return lhs.from == rhs.from &&
               lhs.to == rhs.to &&
               lhs.movingPiece == rhs.movingPiece &&
               lhs.promotionPiece == rhs.promotionPiece &&
               lhs.flags == rhs.flags;
    }

    bool is_capture(const Move& move)
    {
        return (move.flags & MoveFlagCapture) != 0U;
    }

    bool is_promotion(const Move& move)
    {
        return (move.flags & MoveFlagPromotion) != 0U;
    }

    int mvv_lva(const Move& move)
    {
        return piece_value(move.capturedPiece) * 10 - piece_value(move.movingPiece);
    }

    int compute_time_budget_ms(int timeLimitMs)
    {
        if (timeLimitMs <= 0)
        {
            return 0;
        }

        if (timeLimitMs < 5000)
        {
            return timeLimitMs;
        }

        const int assumedMovesToGo = 30;
        const int perMove = timeLimitMs / assumedMovesToGo;
        const int safetyMargin = timeLimitMs / 20;
        const int budget = std::max(perMove, 50);

        return std::min(timeLimitMs - safetyMargin, budget);
    }

    bool has_time_left(SearchContext& context)
    {
        if (context.timeLimitMs <= 0)
        {
            return true;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - context.startTime).count();

        if (elapsedMs >= context.timeLimitMs)
        {
            context.stopped = true;
            return false;
        }

        return true;
    }

    int to_tt_score(int score, int ply)
    {
        if (score >= MateThreshold)
        {
            return score + ply;
        }
        if (score <= -MateThreshold)
        {
            return score - ply;
        }
        return score;
    }

    int from_tt_score(int score, int ply)
    {
        if (score >= MateThreshold)
        {
            return score - ply;
        }
        if (score <= -MateThreshold)
        {
            return score + ply;
        }
        return score;
    }

    void store_tt(std::uint64_t key,
                  int depth,
                  int ply,
                  int score,
                  NodeType nodeType,
                  const Move& bestMove)
    {
        const std::size_t index = static_cast<std::size_t>(key % TTSize);
        TTEntry& entry = transpositionTable[index];

        if (!entry.valid || entry.key != key || depth >= entry.depth)
        {
            entry.key = key;
            entry.depth = depth;
            entry.score = to_tt_score(score, ply);
            entry.nodeType = nodeType;
            entry.bestMove = bestMove;
            entry.valid = true;
        }
    }

    bool probe_tt(std::uint64_t key,
                  int depth,
                  int alpha,
                  int beta,
                  int ply,
                  Move& outMove,
                  int& outScore)
    {
        const std::size_t index = static_cast<std::size_t>(key % TTSize);
        const TTEntry& entry = transpositionTable[index];

        if (!entry.valid || entry.key != key)
        {
            return false;
        }

        outMove = entry.bestMove;
        const int ttScore = from_tt_score(entry.score, ply);

        if (entry.depth >= depth)
        {
            switch (entry.nodeType)
            {
            case NodeType::Exact:
                outScore = ttScore;
                return true;
            case NodeType::LowerBound:
                if (ttScore > alpha)
                {
                    alpha = ttScore;
                }
                break;
            case NodeType::UpperBound:
                if (ttScore < beta)
                {
                    beta = ttScore;
                }
                break;
            }

            if (alpha >= beta)
            {
                outScore = ttScore;
                return true;
            }
        }

        return false;
    }

    bool is_passed_pawn_push(const Board& board, const Move& move, Color mover)
    {
        const Piece pawnPiece = (mover == Color::White) ? Piece::WhitePawn : Piece::BlackPawn;
        if (move.movingPiece != pawnPiece)
        {
            return false;
        }

        if (is_capture(move))
        {
            return false;
        }

        const int targetSquare = move.to;
        const int targetFile = file_of(targetSquare);
        const int targetRank = rank_of(targetSquare);
        const int direction = (mover == Color::White) ? 1 : -1;

        for (int rank = targetRank + direction; rank >= 0 && rank < 8; rank += direction)
        {
            for (int df = -1; df <= 1; ++df)
            {
                const int file = targetFile + df;
                if (file < 0 || file > 7)
                {
                    continue;
                }

                const int sq = make_square(file, rank);
                const Piece target = board.piece_at(sq);
                if (mover == Color::White)
                {
                    if (target == Piece::BlackPawn)
                    {
                        return false;
                    }
                }
                else
                {
                    if (target == Piece::WhitePawn)
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    bool has_non_pawn_material(const Board& board, Color side)
    {
        const Piece pawn = (side == Color::White) ? Piece::WhitePawn : Piece::BlackPawn;
        const Piece king = (side == Color::White) ? Piece::WhiteKing : Piece::BlackKing;

        for (int square = 0; square < 64; ++square)
        {
            const Piece piece = board.piece_at(square);
            if (piece == pawn || piece == king || piece == Piece::None)
            {
                continue;
            }

            if ((side == Color::White && is_white_piece(piece)) ||
                (side == Color::Black && is_black_piece(piece)))
            {
                return true;
            }
        }

        return false;
    }

    void score_and_sort_moves(const Move& ttMove,
                              int ply,
                              Color mover,
                              std::vector<Move>& moves)
    {
        const int colorIndex = (mover == Color::White) ? 0 : 1;
        const KillerMoves emptyKillers{};
        const KillerMoves& killers = (ply < MaxSearchDepth) ? killerMoves[ply] : emptyKillers;

        std::vector<std::pair<int, Move>> scored;
        scored.reserve(moves.size());

        for (const Move& move : moves)
        {
            int score = 0;

            if (same_move(move, ttMove))
            {
                score = 1'000'000;
            }
            else if (is_capture(move))
            {
                score = 900'000 + mvv_lva(move);
                if (is_promotion(move))
                {
                    score += piece_value(move.promotionPiece);
                }
            }
            else if (is_promotion(move))
            {
                score = 850'000 + piece_value(move.promotionPiece);
            }
            else if (same_move(move, killers.primary))
            {
                score = 800'000;
            }
            else if (same_move(move, killers.secondary))
            {
                score = 795'000;
            }
            else
            {
                score = historyHeuristic[colorIndex][move.from][move.to];
            }

            scored.emplace_back(score, move);
        }

        std::stable_sort(
            scored.begin(),
            scored.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.first > rhs.first;
            });

        for (std::size_t i = 0; i < moves.size(); ++i)
        {
            moves[i] = scored[i].second;
        }
    }

    int quiescence(Board& board,
                   int alpha,
                   int beta,
                   std::int64_t& nodes,
                   SearchContext& context,
                   int ply)
    {
        if (!has_time_left(context))
        {
            return evaluate(board);
        }

        ++nodes;

        int standPat = evaluate(board);

        if (standPat >= beta)
        {
            return beta;
        }
        if (standPat > alpha)
        {
            alpha = standPat;
        }

        std::vector<Move> moves = board.generate_legal_moves();

        std::vector<Move> captures;
        captures.reserve(moves.size());

        for (const Move& move : moves)
        {
            if (is_capture(move))
            {
                captures.push_back(move);
            }
        }

        score_and_sort_moves(Move{}, ply, board.side_to_move(), captures);

        for (const Move& move : captures)
        {
            if (context.stopped)
            {
                break;
            }

            board.make_move(move);
            const int score = -quiescence(board, -beta, -alpha, nodes, context, ply + 1);
            board.undo_move();

            if (context.stopped)
            {
                return alpha;
            }

            if (score >= beta)
            {
                return beta;
            }
            if (score > alpha)
            {
                alpha = score;
            }
        }

        return alpha;
    }

    int search_impl(Board& board,
                    int depth,
                    int alpha,
                    int beta,
                    std::int64_t& nodes,
                    SearchContext& context,
                    int ply,
                    const Move& previousMove)
    {
        if (!has_time_left(context))
        {
            return evaluate(board);
        }

        if (depth <= 0)
        {
            return quiescence(board, alpha, beta, nodes, context, ply);
        }

        ++nodes;

        const int alphaOriginal = alpha;
        const std::uint64_t key = board.zobrist_key();
        const Color mover = board.side_to_move();
        const bool inCheck = board.is_in_check(mover);

        Move ttMove{};
        int ttScore = 0;
        if (probe_tt(key, depth, alpha, beta, ply, ttMove, ttScore))
        {
            return ttScore;
        }

        if (!inCheck && depth >= 3 && has_non_pawn_material(board, mover))
        {
            board.make_null_move();
            const int nullScore =
                -search_impl(board, depth - 3, -beta, -beta + 1, nodes, context, ply + 1, Move{});
            board.undo_null_move();

            if (context.stopped)
            {
                return alpha;
            }

            if (nullScore >= beta)
            {
                return beta;
            }
        }

        std::vector<Move> moves = board.generate_legal_moves();

        if (moves.empty())
        {
            if (inCheck)
            {
                return -MateValue + ply;
            }
            return 0;
        }

        score_and_sort_moves(ttMove, ply, mover, moves);

        int bestScore = -InfinityScore;
        Move bestMove{};
        int moveIndex = 0;

        for (const Move& move : moves)
        {
            if (context.stopped)
            {
                break;
            }

            board.make_move(move);

            const bool gaveCheck = board.is_in_check(board.side_to_move());
            const bool passedPawnPush = is_passed_pawn_push(board, move, mover);
            const bool recapture =
                is_capture(move) && previousMove.to == move.to && previousMove.to != previousMove.from;

            const int extension = (gaveCheck || passedPawnPush || recapture) ? 1 : 0;
            int nextDepth = depth - 1 + extension;

            if (!is_capture(move) &&
                !is_promotion(move) &&
                depth >= 3 &&
                moveIndex >= 4 &&
                !gaveCheck &&
                !recapture &&
                !same_move(move, ttMove))
            {
                --nextDepth;
            }

            if (nextDepth < 0)
            {
                nextDepth = 0;
            }

            const int score =
                -search_impl(board, nextDepth, -beta, -alpha, nodes, context, ply + 1, move);

            board.undo_move();

            if (context.stopped)
            {
                return alpha;
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestMove = move;
            }
            if (score > alpha)
            {
                alpha = score;
                if (alpha >= beta)
                {
                    if (!is_capture(move) && !is_promotion(move) && ply < MaxSearchDepth)
                    {
                        KillerMoves& killers = killerMoves[ply];
                        if (!same_move(move, killers.primary))
                        {
                            killers.secondary = killers.primary;
                            killers.primary = move;
                        }

                        const int colorIndex = (mover == Color::White) ? 0 : 1;
                        historyHeuristic[colorIndex][move.from][move.to] += depth * depth;
                    }

                    break;
                }
            }

            ++moveIndex;
        }

        NodeType nodeType = NodeType::Exact;

        if (bestScore <= alphaOriginal)
        {
            nodeType = NodeType::UpperBound;
        }
        else if (bestScore >= beta)
        {
            nodeType = NodeType::LowerBound;
        }

        store_tt(key, depth, ply, bestScore, nodeType, bestMove);

        return bestScore;
    }
}

int search(Board& board, int depth, int alpha, int beta, std::int64_t& nodes)
{
    SearchContext context;
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = 0;
    context.stopped = false;

    return search_impl(board, depth, alpha, beta, nodes, context, 0, Move{});
}

Move find_best_move(Board& board,
                    int maxDepth,
                    int timeLimitMs,
                    int& outScore,
                    std::int64_t& outNodes,
                    int& outDepth,
                    bool useAbsoluteTime)
{
    transpositionTable.fill(TTEntry{});
    killerMoves.fill(KillerMoves{});

    for (auto& colorTable : historyHeuristic)
    {
        for (auto& fromTable : colorTable)
        {
            std::fill(std::begin(fromTable), std::end(fromTable), 0);
        }
    }

    SearchContext context;
    context.startTime = std::chrono::steady_clock::now();
    const int clampedTime = (timeLimitMs > 0) ? timeLimitMs : 0;
    context.timeLimitMs = useAbsoluteTime ? clampedTime : compute_time_budget_ms(clampedTime);
    context.stopped = false;

    std::int64_t nodes = 0;

    std::vector<Move> rootMoves = board.generate_legal_moves();
    if (rootMoves.empty())
    {
        outScore = 0;
        outNodes = 0;
        outDepth = 0;
        return Move{};
    }

    Move globalBestMove = rootMoves.front();
    int globalBestScore = -InfinityScore;
    int bestDepthReached = 0;

    for (int depth = 1; depth <= maxDepth; ++depth)
    {
        int alpha = -InfinityScore;
        int beta = InfinityScore;

        Move bestMoveThisDepth{};
        int bestScoreThisDepth = -InfinityScore;

        const auto iterStart = std::chrono::steady_clock::now();
        const std::int64_t nodesBefore = nodes;

        score_and_sort_moves(globalBestMove, 0, board.side_to_move(), rootMoves);

        for (const Move& move : rootMoves)
        {
            if (!has_time_left(context))
            {
                break;
            }

            board.make_move(move);
            const int score =
                -search_impl(board, depth - 1, -beta, -alpha, nodes, context, 1, move);
            board.undo_move();

            if (context.stopped)
            {
                break;
            }

            if (score > bestScoreThisDepth)
            {
                bestScoreThisDepth = score;
                bestMoveThisDepth = move;
            }
            if (score > alpha)
            {
                alpha = score;
            }
        }

        const auto iterEnd = std::chrono::steady_clock::now();
        const auto elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(iterEnd - iterStart).count();
        const std::int64_t nodesThisIter = nodes - nodesBefore;
        const double seconds = elapsedMs > 0 ? static_cast<double>(elapsedMs) / 1000.0 : 0.001;
        const auto nps = static_cast<std::int64_t>(nodesThisIter / seconds);

        if (!context.stopped && bestScoreThisDepth != -InfinityScore)
        {
            globalBestMove = bestMoveThisDepth;
            globalBestScore = bestScoreThisDepth;
            bestDepthReached = depth;

            std::cout << "info depth " << depth
                      << " score " << bestScoreThisDepth
                      << " nodes " << nodes
                      << " nps " << nps
                      << " pv " << bestMoveThisDepth.to_uci()
                      << '\n';
        }

        if (context.stopped)
        {
            break;
        }
    }

    outScore = (globalBestScore == -InfinityScore) ? 0 : globalBestScore;
    outNodes = nodes;
    outDepth = bestDepthReached;

    return globalBestMove;
}
