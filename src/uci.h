#pragma once

#include <string>

class Board;

namespace uci
{
    struct EngineInfo
    {
        std::string name;
        std::string author;
    };

    void run(Board& board, const EngineInfo& info);
}

