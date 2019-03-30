#include "FileInterface.h"
#include <cstdio>
#include <stdint.h>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <memory>
#include <unordered_map>

#include <intercept.hpp>

#ifdef _DEBUG
#include <conio.h>
#endif

using namespace std;

FileInterface g_FileInterface;

using file_map = std::unordered_map<std::string, std::ofstream>;

file_map g_files;

file_map::iterator open_file(std::string fileName)
{
    // lowercase it
    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);

    auto fItr = g_files.find(fileName);
    if (fItr == g_files.end())
    {
        fItr = g_files.emplace(std::make_pair(fileName, std::ofstream{ std::string("Logs/") + fileName, ios_base::out | ios_base::ate })).first;
    }

    return fItr;
}

// Creates a new ofstream, adds it to the array
game_value FileInterface::ofstream_new(game_state& state, game_value_parameter parameter)
{
    string fileName{ parameter };
#ifdef _DEBUG
    _cprintf("ofstream_new: %s\n", fileName.c_str());
#endif
    return game_value(open_file(fileName)->first);
}

game_value FileInterface::ofstream_write(game_state& state, game_value_parameter left, game_value_parameter right)
{
    string fileName(left);
    string textToWrite(right);

    auto fileItr = open_file(fileName);
    // Time to write stuff

    // First output the current time
    time_t t = time(0);
    tm lt;
    localtime_s(&lt, &t);
    char timeStr[16];
    sprintf_s(timeStr, "%02u:%02u:%02u ", lt.tm_hour, lt.tm_min, lt.tm_sec);
    fileItr->second << timeStr << textToWrite << "\n";

#ifdef _DEBUG
    _cprintf("ofstream_write: %s %s\n", fileName.c_str(), textToWrite.c_str());
#endif

    return game_value(fileItr->first);
}

// Writes to the opened file

void FileInterface::preStart()
{
    #ifdef _DEBUG
    _cprintf("PRE START ... ");
    #endif

    // Register SQF commands

    //register_sqf_command(std::string_view name, std::string_view description, WrapperFunctionUnary function_, game_data_type return_arg_type, game_data_type right_arg_type) {
    //	return functions.register_sqf_function_unary(name, description, function_, return_arg_type, right_arg_type);

    m_SQF_ofstream_new = client::host::register_sqf_command("ofstream_new"sv, "Opens a file in output text mode"sv, ofstream_new, game_data_type::STRING, game_data_type::STRING);
    m_SQF_ofstream_write = client::host::register_sqf_command("ofstream_write"sv, "Writes to a previously opened file"sv, ofstream_write,
        game_data_type::STRING, // Return
        game_data_type::STRING, // Left
        game_data_type::STRING); // Right
    m_SQF_ofstream_write2 = client::host::register_sqf_command("<<"sv, "Writes to a previously opened file, same as ofstream_write"sv, ofstream_write,
        game_data_type::STRING, // Return
        game_data_type::STRING, // Left
        game_data_type::STRING); // Right

    #ifdef _DEBUG
    _cprintf("DONE\n");
    #endif
}