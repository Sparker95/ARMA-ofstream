#include "FileInterface.h"
#include <cstdio>
#include <stdint.h>
#include <ctime>
#include <algorithm>

#include <intercept.hpp>

#ifdef _DEBUG
#include <conio.h>
#endif

using namespace std;

static FileInterface* g_instance = nullptr;

std::ofstream* FileInterface::open_file(std::string fileName)
{
    // lowercase it
    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);

    auto fItr = g_instance->g_files.find(fileName);
    if (fItr == g_instance->g_files.end())
    {
        fItr = g_instance->g_files.emplace(fileName, std::ofstream{ std::string("Logs/") + fileName, ios_base::out | ios_base::ate }).first;
    }
    return &fItr->second;
}

// Creates a new ofstream, adds it to the array
game_value FileInterface::ofstream_new(game_state& state, game_value_parameter parameter)
{
    string fileName{ parameter };

#ifdef _DEBUG
    _cprintf("ofstream_new: %s\n", fileName.c_str());
#endif
    open_file(fileName);

    return fileName;
}

game_value FileInterface::ofstream_write(game_state& state, game_value_parameter left, game_value_parameter right)
{
    string fileName{ left };
    string textToWrite{ right };

#ifdef _DEBUG
    _cprintf("ofstream_write: %s %s\n", fileName.c_str(), textToWrite.c_str());
#endif

    // Get the file handle, opening it if required
    auto fileItr = open_file(fileName);

    // First output the current time
    time_t t = time(0);
    tm lt;
    localtime_s(&lt, &t);
    char timeStr[16];
    sprintf_s(timeStr, "%02u:%02u:%02u ", lt.tm_hour, lt.tm_min, lt.tm_sec);
    (*fileItr) << timeStr << textToWrite << "\n";
    fileItr->flush();

    return fileName;
}

// Writes to the opened file
// Registered SQF functions
FileInterface::FileInterface()
{
    g_instance = this;

#ifdef _DEBUG
    _cprintf("PRE START ... ");
#endif

    // Register SQF commands

    //register_sqf_command(std::string_view name, std::string_view description, WrapperFunctionUnary function_, game_data_type return_arg_type, game_data_type right_arg_type) {
    //	return functions.register_sqf_function_unary(name, description, function_, return_arg_type, right_arg_type);
    using namespace intercept;

    m_SQF_ofstream_new = client::host::register_sqf_command(
        "ofstream_new", "Opens a file in output text mode",
        ofstream_new,
        game_data_type::STRING, game_data_type::STRING);
    m_SQF_ofstream_write = client::host::register_sqf_command(
        "ofstream_write", "Writes to a file",
        ofstream_write,
        game_data_type::STRING, // Return
        game_data_type::STRING, // Left
        game_data_type::STRING); // Right
    m_SQF_ofstream_write2 = client::host::register_sqf_command(
        "<<", "Writes to a file, same as ofstream_write",
        ofstream_write,
        game_data_type::STRING, // Return
        game_data_type::STRING, // Left
        game_data_type::STRING); // Right

#ifdef _DEBUG
    _cprintf("DONE\n");
#endif
}

FileInterface::~FileInterface()
{
    g_instance = nullptr;
}
