#include "FileInterface.h"
#include <cstdio>
#include <stdint.h>
#include <ctime>

#include <intercept.hpp>

#ifdef _DEBUG
#include <conio.h>
#endif

FileInterface g_FileInterface;

FileInterface::FileInterface() :
	m_files()
{

};

FileInterface::~FileInterface()
{
	// Close all the files
	for (auto i : m_files) {
		i.ofstreamPtr->close();
	}
};

// Creates a new ofstream, adds it to the array
game_value FileInterface::ofstream_new(uintptr_t state, game_value_parameter parameter)
{
	string fileName(parameter);

	#ifdef _DEBUG
	_cprintf("ofstream_new: %s\n", fileName.data());
	#endif

	openedFile_s* openedFilePtr = 0;

	// Check if a file with such name is already open
	for (unsigned int i = 0; i < g_FileInterface.m_files.size(); i++) {
		if (g_FileInterface.m_files[i].fileName == fileName) {
			openedFilePtr = &g_FileInterface.m_files[i];
			#ifdef _DEBUG
			_cprintf("File is already open\n");
			#endif
			break;
		}
	}

	// Create a new ofstream if such file wasn't found in the array
	if (openedFilePtr == 0) {
		// Add the ofstream to the array
		openedFile_s openedFile;
		openedFile.ofstreamPtr = new ofstream(string("Logs/") + fileName, ios_base::out | ios_base::ate); // Allocate in heap
		openedFile.fileName = fileName; // Store the file name
		// Add to the array
		g_FileInterface.m_files.push_back(openedFile);
		openedFilePtr = &g_FileInterface.m_files[g_FileInterface.m_files.size() - 1];
	}

	// Return a hex pointer to the struct
	const unsigned int nChars = sizeof(void*) * 2;
	char hexStr[nChars + 1]; // +1 for the null character
	uint64_t returnPtr_64 = (uint64_t)openedFilePtr;
	sprintf(hexStr, "%llX", returnPtr_64);

	#ifdef _DEBUG
	_cprintf("Struct pointer: %0llX\n", openedFilePtr);
	_cprintf("Returning value: %s\n", hexStr);
	#endif

	return game_value(hexStr);
}

game_value FileInterface::ofstream_write(uintptr_t state, game_value_parameter left, game_value_parameter right)
{
	string structPtrStr(left);
	string textToWrite(right);


	#ifdef _DEBUG
	_cprintf("ofstream_write: %s, %s\n", structPtrStr.data(), textToWrite.data());
	#endif

	// Convert the left argument to struct pointer
	uint64_t structPtr_64 = 0;
	sscanf(structPtrStr.data(), "%llX", &structPtr_64);
	openedFile_s* structPtr = (openedFile_s*)structPtr_64;

	#ifdef _DEBUG
	_cprintf("Restored pointer value from string: %llX\n", structPtr_64);
	#endif

	// Try to find this pointer in the array
	bool found = false;
	for (unsigned int i = 0; i < g_FileInterface.m_files.size(); i++) {
		if (&g_FileInterface.m_files[i] == structPtr) {
			found = true;
			break;
		}
	}

	if (found) {
#ifdef _DEBUG
		_cprintf("Success: pointer was found\n", structPtrStr.data(), textToWrite.data());
#endif
		// Time to write stuff

		// First output the current time
		time_t t = time(0);
		tm* lt = localtime(&t);
		char timeStr[16];
		sprintf(timeStr, "%02u:%02u:%02u ", lt->tm_hour, lt->tm_min, lt->tm_sec);

		structPtr->ofstreamPtr->write(timeStr, 9);
		structPtr->ofstreamPtr->write(textToWrite.data(), textToWrite.size());
		structPtr->ofstreamPtr->write("\n", 1);

		//char testText[] = "Test text";
		//structPtr->ofstreamPtr->write(testText, sizeof(testText)-1);

		structPtr->ofstreamPtr->flush();

		// Return the stringified pointer
		const unsigned int nChars = sizeof(void*) * 2;
		char hexStr[nChars + 1]; // +1 for the null character
		uint64_t returnPtr_64 = (uint64_t)structPtr;
		sprintf(hexStr, "%llX", returnPtr_64);
		return game_value(hexStr);
	} else {
		#ifdef _DEBUG
		_cprintf("Error: pointer not found\n", structPtrStr.data(), textToWrite.data());
		#endif

		return game_value("ERROR");
	}
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

	#ifdef _DEBUG
	_cprintf("DONE\n");
	#endif
}