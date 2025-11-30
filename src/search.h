#pragma once

#include <cstdint>

class Board;
struct Move;

int search(Board& board, int depth, int alpha, int beta, std::int64_t& nodes);

Move find_best_move(Board& board,
                    int maxDepth,
                    int timeLimitMs,
                    int& outScore,
                    std::int64_t& outNodes,
                    int& outDepth);

