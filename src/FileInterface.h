#pragma once

#include <intercept.hpp>

#include <fstream>
#include <memory>
#include <unordered_map>

// Class that lets SQF interract with files
// Author: Sparker 20.02.2019

class FileInterface
{
public:
    FileInterface();
    ~FileInterface();

private:

    static std::ofstream* open_file(std::string fileName);
	static std::ofstream* clear_file(std::string fileName);

    static game_value ofstream_new(game_state& state, game_value_parameter parameter);
	static game_value ofstream_clear(game_state& state, game_value_parameter parameter);

    static game_value ofstream_write(game_state& state, game_value_parameter left, game_value_parameter right);
	static game_value ofstream_dump(game_state& state, game_value_parameter left, game_value_parameter right);

    // Registered SQF functions
	registered_sqf_function m_SQF_ofstream_new;
	registered_sqf_function m_SQF_ofstream_clear;
	registered_sqf_function m_SQF_ofstream_write;
    registered_sqf_function m_SQF_ofstream_write2;
	registered_sqf_function m_SQF_ofstream_dump;

    using file_map = std::unordered_map<std::string, std::ofstream>;
    file_map g_files;
};