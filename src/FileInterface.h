#pragma once

#include <intercept.hpp>

// Class that lets SQF interract with files
// Author: Sparker 20.02.2019

class FileInterface
{
public:
    FileInterface();
    ~FileInterface();

private:
    using file_map = std::unordered_map<std::string, std::ofstream>;

    static std::ofstream* open_file(std::string fileName);
    static game_value ofstream_new(game_state& state, game_value_parameter parameter);
    static game_value ofstream_write(game_state& state, game_value_parameter left, game_value_parameter right);

    // Registered SQF functions
    registered_sqf_function m_SQF_ofstream_new;
    registered_sqf_function m_SQF_ofstream_write;
    registered_sqf_function m_SQF_ofstream_write2;

    file_map g_files;
};