#pragma once

#include <string>

class Board;
struct Move;

std::string move_to_san(Board& positionBeforeMove, const Move& move);
