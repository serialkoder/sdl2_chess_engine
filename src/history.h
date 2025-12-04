#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct GameRecord
{
    std::string utc;
    std::string result{"*"};
    std::string termination{"unknown"};
    std::string startFen;
    std::string finalFen;
    std::vector<std::string> moves;
    int engineDepth{0};
    int engineTimeMs{0};
};

struct GameMeta
{
    std::filesystem::path path;
    std::string utc;
    std::string result;
    std::string termination;
    std::size_t moveCount{0};
};

namespace history
{
    std::filesystem::path history_dir();

    void save_game(GameRecord record);

    std::vector<GameMeta> list_games();

    GameRecord load_game(const std::filesystem::path& path);
}
