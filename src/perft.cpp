#include <cstdint>
#include <iostream>
#include <vector>

#include "board.h"
#include "move.h"

std::uint64_t perft(Board& board, int depth)
{
    if (depth == 0)
    {
        return 1;
    }

    std::uint64_t nodes = 0;
    const std::vector<Move> moves = board.generate_legal_moves();

    for (const Move& move : moves)
    {
        board.make_move(move);
        nodes += perft(board, depth - 1);
        board.undo_move();
    }

    return nodes;
}

int main()
{
    Board board;

    std::cout << "Perft for standard starting position:\n";

    const int depths[] = {1, 2, 3, 4};
    const std::uint64_t expected[] = {20ULL, 400ULL, 8902ULL, 197281ULL};

    for (std::size_t i = 0; i < 4; ++i)
    {
        const int depth = depths[i];
        std::uint64_t nodes = perft(board, depth);
        std::cout << "Depth " << depth << ": " << nodes
                  << " (expected " << expected[i] << ")\n";
    }

    return 0;
}

