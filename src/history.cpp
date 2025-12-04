#include "history.h"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
    std::string current_utc_timestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};

#if defined(_WIN32)
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif

        char buffer[32];
        if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm) == 0)
        {
            return {};
        }

        return std::string(buffer);
    }

    std::string sanitize_for_filename(const std::string& timestamp)
    {
        std::string sanitized = timestamp;
        for (char& ch : sanitized)
        {
            if (ch == ':')
            {
                ch = '-';
            }
        }
        return sanitized;
    }

    std::string line_value(const std::string& line, const std::string& key)
    {
        const std::size_t prefixSize = key.size();
        if (line.size() <= prefixSize)
        {
            return {};
        }
        return line.substr(prefixSize);
    }
}

std::filesystem::path history::history_dir()
{
    std::filesystem::path dir;

    char* rawPath = SDL_GetPrefPath("serialcoder", "sdl2_chess_engine");
    if (rawPath)
    {
        dir = std::filesystem::path(rawPath) / "games";
        SDL_free(rawPath);
    }
    else
    {
        dir = std::filesystem::temp_directory_path() / "sdl2_chess_engine" / "games";
        std::cerr << "SDL_GetPrefPath failed; using temp directory " << dir << '\n';
    }

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec)
    {
        std::cerr << "Failed to create history directory " << dir << ": " << ec.message() << '\n';
    }

    return dir;
}

void history::save_game(GameRecord record)
{
    if (record.utc.empty())
    {
        record.utc = current_utc_timestamp();
    }

    const std::string fileTimestamp = sanitize_for_filename(record.utc);

    const std::filesystem::path dir = history_dir();
    const std::filesystem::path path = dir / ("game_" + fileTimestamp + ".uci");

    std::ofstream out(path);
    if (!out)
    {
        std::cerr << "Failed to open game file for writing: " << path << '\n';
        return;
    }

    out << "utc " << record.utc << '\n';
    out << "result " << record.result << '\n';
    out << "termination " << record.termination << '\n';
    out << "startfen " << record.startFen << '\n';

    out << "moves";
    for (const std::string& move : record.moves)
    {
        out << ' ' << move;
    }
    out << '\n';

    out << "finalfen " << record.finalFen << '\n';
    out << "engineDepth " << record.engineDepth << '\n';
    out << "engineTimeMs " << record.engineTimeMs << '\n';

    out.close();
    if (!out)
    {
        std::cerr << "Failed to write game file: " << path << '\n';
    }
}

std::vector<GameMeta> history::list_games()
{
    std::vector<GameMeta> games;
    const std::filesystem::path dir = history_dir();

    if (!std::filesystem::exists(dir))
    {
        return games;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        if (entry.path().extension() != ".uci")
        {
            continue;
        }

        GameRecord record = load_game(entry.path());
        GameMeta meta;
        meta.path = entry.path();
        meta.utc = record.utc;
        meta.result = record.result;
        meta.termination = record.termination;
        meta.moveCount = record.moves.size();
        games.push_back(std::move(meta));
    }

    std::sort(
        games.begin(),
        games.end(),
        [](const GameMeta& lhs, const GameMeta& rhs)
        {
            std::error_code ec1;
            std::error_code ec2;
            const auto t1 = std::filesystem::last_write_time(lhs.path, ec1);
            const auto t2 = std::filesystem::last_write_time(rhs.path, ec2);
            if (ec1 || ec2)
            {
                return lhs.path > rhs.path;
            }
            return t1 > t2;
        });

    return games;
}

GameRecord history::load_game(const std::filesystem::path& path)
{
    GameRecord record;

    std::ifstream in(path);
    if (!in)
    {
        std::cerr << "Failed to open game file: " << path << '\n';
        return record;
    }

    std::string line;
    while (std::getline(in, line))
    {
        if (line.rfind("utc ", 0) == 0)
        {
            record.utc = line_value(line, "utc ");
        }
        else if (line.rfind("result ", 0) == 0)
        {
            record.result = line_value(line, "result ");
        }
        else if (line.rfind("termination ", 0) == 0)
        {
            record.termination = line_value(line, "termination ");
        }
        else if (line.rfind("startfen ", 0) == 0)
        {
            record.startFen = line_value(line, "startfen ");
        }
        else if (line.rfind("moves ", 0) == 0)
        {
            const std::string movesStr = line_value(line, "moves ");
            std::istringstream movesStream(movesStr);
            std::string move;
            while (movesStream >> move)
            {
                record.moves.push_back(move);
            }
        }
        else if (line.rfind("finalfen ", 0) == 0)
        {
            record.finalFen = line_value(line, "finalfen ");
        }
        else if (line.rfind("engineDepth ", 0) == 0)
        {
            const std::string value = line_value(line, "engineDepth ");
            try
            {
                record.engineDepth = std::stoi(value);
            }
            catch (...)
            {
                record.engineDepth = 0;
            }
        }
        else if (line.rfind("engineTimeMs ", 0) == 0)
        {
            const std::string value = line_value(line, "engineTimeMs ");
            try
            {
                record.engineTimeMs = std::stoi(value);
            }
            catch (...)
            {
                record.engineTimeMs = 0;
            }
        }
    }

    return record;
}
