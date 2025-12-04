#include "uci.h"

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "board.h"
#include "move.h"
#include "search.h"

namespace
{
    const std::string StartPositionFen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    std::vector<std::string> tokenize(const std::string& line)
    {
        std::istringstream stream(line);
        std::vector<std::string> tokens;
        std::string token;
        while (stream >> token)
        {
            tokens.push_back(token);
        }
        return tokens;
    }

    std::string to_lower_copy(const std::string& text)
    {
        std::string lowered;
        lowered.reserve(text.size());
        for (char ch : text)
        {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return lowered;
    }

    int parse_int(const std::string& text)
    {
        try
        {
            return std::stoi(text);
        }
        catch (...)
        {
            return -1;
        }
    }

    void reset_board(Board& board)
    {
        board.load_fen(StartPositionFen);
    }

    bool apply_uci_move(Board& board, const std::string& moveText)
    {
        const std::string lowered = to_lower_copy(moveText);
        const std::vector<Move> legalMoves = board.generate_legal_moves();

        for (const Move& move : legalMoves)
        {
            if (move.to_uci() == lowered)
            {
                board.make_move(move);
                return true;
            }
        }

        return false;
    }

    void handle_position(Board& board, const std::vector<std::string>& tokens)
    {
        if (tokens.size() < 2)
        {
            return;
        }

        std::size_t index = 1;
        const std::string first = to_lower_copy(tokens[index]);

        if (first == "startpos")
        {
            reset_board(board);
            ++index;
        }
        else if (first == "fen")
        {
            if (index + 6 >= tokens.size())
            {
                return;
            }

            std::ostringstream fen;
            fen << tokens[index + 1] << ' '
                << tokens[index + 2] << ' '
                << tokens[index + 3] << ' '
                << tokens[index + 4] << ' '
                << tokens[index + 5] << ' '
                << tokens[index + 6];

            board.load_fen(fen.str());
            index += 7;
        }
        else
        {
            return;
        }

        if (index < tokens.size() && to_lower_copy(tokens[index]) == "moves")
        {
            ++index;
            for (; index < tokens.size(); ++index)
            {
                if (!apply_uci_move(board, tokens[index]))
                {
                    break;
                }
            }
        }
    }

    std::string best_move_string(const Move& move)
    {
        if (move.movingPiece == Piece::None &&
            move.from == 0 &&
            move.to == 0)
        {
            return "0000";
        }
        return move.to_uci();
    }

    void handle_go(Board& board, const std::vector<std::string>& tokens)
    {
        int depth = -1;
        int movetime = 0;

        for (std::size_t i = 1; i < tokens.size(); ++i)
        {
            const std::string tokenLower = to_lower_copy(tokens[i]);

            if (tokenLower == "depth" && i + 1 < tokens.size())
            {
                const int parsed = parse_int(tokens[i + 1]);
                if (parsed > 0)
                {
                    depth = parsed;
                }
                ++i;
            }
            else if (tokenLower == "movetime" && i + 1 < tokens.size())
            {
                const int parsed = parse_int(tokens[i + 1]);
                if (parsed > 0)
                {
                    movetime = parsed;
                }
                ++i;
            }
        }

        const int fallbackDepth = (movetime > 0) ? 64 : 6;
        const int maxDepth = (depth > 0) ? depth : fallbackDepth;

        int score = 0;
        std::int64_t nodes = 0;
        int searchedDepth = 0;

        const Move bestMove = find_best_move(board,
                                             maxDepth,
                                             movetime,
                                             score,
                                             nodes,
                                             searchedDepth,
                                             movetime > 0);

        std::cout << "bestmove " << best_move_string(bestMove) << '\n';
        std::cout.flush();
    }
}

namespace uci
{
    void run(Board& board, const EngineInfo& info)
    {
        std::ios::sync_with_stdio(false);
        std::cin.tie(nullptr);

        std::string line;
        while (std::getline(std::cin, line))
        {
            const std::vector<std::string> tokens = tokenize(line);
            if (tokens.empty())
            {
                continue;
            }

            const std::string command = to_lower_copy(tokens.front());

            if (command == "uci")
            {
                std::cout << "id name " << info.name << '\n';
                std::cout << "id author " << info.author << '\n';
                std::cout << "uciok\n";
                std::cout.flush();
            }
            else if (command == "isready")
            {
                std::cout << "readyok\n";
                std::cout.flush();
            }
            else if (command == "ucinewgame")
            {
                reset_board(board);
            }
            else if (command == "position")
            {
                handle_position(board, tokens);
            }
            else if (command == "go")
            {
                handle_go(board, tokens);
            }
            else if (command == "stop")
            {
                // Synchronous search; nothing to stop here.
            }
            else if (command == "quit")
            {
                break;
            }
        }
    }
}

