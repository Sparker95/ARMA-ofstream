#include "FileInterface.h"
#include <cstdio>
#include <stdint.h>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <intercept.hpp>

#ifdef _DEBUG
#include <conio.h>
#endif

using namespace std;
using string = std::string;

static FileInterface* g_instance = nullptr;

// Internal function (not SQF)
std::ofstream* FileInterface::open_file(const std::string& fileName)
{
	string profileName{ intercept::sqf::profile_name() };

#ifdef _DEBUG
	_cprintf("open_file: %s\n", fileName.c_str());
#endif

	string key = profileName + fileName;
    auto fItr = g_instance->g_files.find(key);
    if (fItr == g_instance->g_files.end())
    {
#ifdef _DEBUG
		_cprintf("  file was not open before\n");
#endif

		// Also create a directory Logs/profileName
		string folderPathStr("Logs/");
		folderPathStr += profileName;
		std::filesystem::path folderPath(folderPathStr);
		std::filesystem::create_directories(folderPathStr);

		string fileOpenPath = folderPathStr; // Logs/profileName/fileName
		fileOpenPath.append("/");
		fileOpenPath.append(fileName);
        fItr = g_instance->g_files.emplace(key, std::ofstream{ fileOpenPath, ios_base::out | ios_base::ate }).first;
    }
    return &fItr->second;
}

// Internal function (not SQF)
std::ofstream* FileInterface::clear_file(const std::string& fileName)
{
	string profileName{ intercept::sqf::profile_name() };

	string key = profileName + fileName;
	auto fItr = g_instance->g_files.find(key);
	if (fItr != g_instance->g_files.end())
	{
		fItr->second.close();
		g_instance->g_files.erase(fItr);
	}

	// Also create a directory Logs/profileName
	string folderPathStr("Logs/");
	folderPathStr += profileName;
	std::filesystem::path folderPath(folderPathStr);
	std::filesystem::create_directories(folderPathStr);

	string fileOpenPath = folderPathStr; // Logs/profileName/fileName
	fileOpenPath.append("/");
	fileOpenPath.append(fileName);
	fItr = g_instance->g_files.emplace(key, std::ofstream{ fileOpenPath, ios_base::out | ios_base::trunc }).first;
	return &fItr->second;
}

// Creates a new ofstream, adds it to the array
// Makes no sense any more, since open_file will be called each time. I guess we keep it for compatibility
game_value FileInterface::ofstream_new(game_state& state, game_value_parameter parameter)
{
    string fileName{ parameter };
	std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);

#ifdef _DEBUG
    _cprintf("ofstream_new: %s\n", fileName.c_str());
#endif
    open_file(fileName);

    return fileName;
}

game_value FileInterface::ofstream_clear(game_state& state, game_value_parameter parameter)
{
	string fileName{ parameter };
	std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);

#ifdef _DEBUG
	_cprintf("ofstream_clear: %s\n", fileName.c_str());
#endif
	clear_file(fileName);
	return fileName;
}

game_value FileInterface::ofstream_write(game_state& state, game_value_parameter left, game_value_parameter right)
{
    string fileName{ left };
	std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
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

game_value FileInterface::ofstream_dump(game_state& state, game_value_parameter left, game_value_parameter right)
{
	string fileName{ left };
	std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
	string textToWrite{ right };

#ifdef _DEBUG
	_cprintf("ofstream_dump: %s %s\n", fileName.c_str(), textToWrite.c_str());
#endif

	// Get the file handle, opening it if required
	auto fileItr = open_file(fileName);
	(*fileItr) << textToWrite;
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
        "ofstream_new", "Opens a file in output text mode (not required, files are opened on write anyway)",
        ofstream_new,
        game_data_type::STRING, game_data_type::STRING);
	m_SQF_ofstream_clear = client::host::register_sqf_command(
		"ofstream_clear", "Clears a file contents",
		ofstream_clear,
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

	m_SQF_ofstream_dump = client::host::register_sqf_command(
		"ofstream_dump", "Dumps string to a file with no formatting or timestamp",
		ofstream_dump,
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
