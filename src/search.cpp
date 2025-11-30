#include "search.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

#include "board.h"
#include "eval.h"
#include "move.h"

namespace
{
    constexpr int CheckmateScore = 100000;
    constexpr int InfinityScore = std::numeric_limits<int>::max() / 4;

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
    };

    std::unordered_map<std::uint64_t, TTEntry> transpositionTable;

    struct SearchContext
    {
        std::chrono::steady_clock::time_point startTime{};
        int timeLimitMs{0};
        bool stopped{false};
    };

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

        const std::vector<Move> moves = board.generate_legal_moves();

        for (const Move& move : moves)
        {
            if (context.stopped)
            {
                break;
            }

            if (move.capturedPiece == Piece::None)
            {
                continue;
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
                    int ply)
    {
        if (!has_time_left(context))
        {
            return evaluate(board);
        }

        if (depth == 0)
        {
            return quiescence(board, alpha, beta, nodes, context, ply);
        }

        ++nodes;

        const int alphaOriginal = alpha;
        const std::uint64_t key = board.zobrist_key();

        Move ttMove{};

        const auto it = transpositionTable.find(key);
        if (it != transpositionTable.end())
        {
            const TTEntry& entry = it->second;
            if (entry.depth >= depth)
            {
                const int ttScore = entry.score;

                switch (entry.nodeType)
                {
                case NodeType::Exact:
                    return ttScore;
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
                    return ttScore;
                }
            }

            ttMove = entry.bestMove;
        }

        std::vector<Move> moves = board.generate_legal_moves();

        if (moves.empty())
        {
            if (board.is_in_check(board.side_to_move()))
            {
                return -CheckmateScore + ply;
            }
            return 0;
        }

        if (!(ttMove.from == 0 && ttMove.to == 0 && ttMove.movingPiece == Piece{}))
        {
            const auto ttIt = std::find_if(
                moves.begin(),
                moves.end(),
                [&ttMove](const Move& m)
                {
                    return m.from == ttMove.from &&
                           m.to == ttMove.to &&
                           m.movingPiece == ttMove.movingPiece;
                });

            if (ttIt != moves.end() && ttIt != moves.begin())
            {
                std::swap(moves.front(), *ttIt);
            }
        }

        int bestScore = -InfinityScore;
        Move bestMove{};

        for (const Move& move : moves)
        {
            if (context.stopped)
            {
                break;
            }

            board.make_move(move);
            const int score = -search_impl(board, depth - 1, -beta, -alpha, nodes, context, ply + 1);
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
                    break;
                }
            }
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

        TTEntry newEntry;
        newEntry.key = key;
        newEntry.depth = depth;
        newEntry.score = bestScore;
        newEntry.nodeType = nodeType;
        newEntry.bestMove = bestMove;
        transpositionTable[key] = newEntry;

        return bestScore;
    }
}

int search(Board& board, int depth, int alpha, int beta, std::int64_t& nodes)
{
    SearchContext context;
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = 0;
    context.stopped = false;

    return search_impl(board, depth, alpha, beta, nodes, context, 0);
}

Move find_best_move(Board& board,
                    int maxDepth,
                    int timeLimitMs,
                    int& outScore,
                    std::int64_t& outNodes,
                    int& outDepth)
{
    transpositionTable.clear();

    SearchContext context;
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = timeLimitMs;
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

        for (const Move& move : rootMoves)
        {
            if (!has_time_left(context))
            {
                break;
            }

            board.make_move(move);
            const int score = -search_impl(board, depth - 1, -beta, -alpha, nodes, context, 1);
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
