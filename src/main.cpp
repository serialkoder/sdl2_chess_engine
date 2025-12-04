#include "board.h"
#include "uci.h"
#include "ui.h"

#include <string>

int main(int argc, char* argv[])
{
    Board board;

    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--uci")
        {
            const uci::EngineInfo info{"SDL2 Chess Engine", "serialcoder"};
            uci::run(board, info);
            return 0;
        }
    }

    ui::run(board);
    return 0;
}
