#pragma once

#include <intercept.hpp>

#include <fstream>

// Class that lets SQF interract with files
// Author: Sparker 20.02.2019

using namespace intercept;
using namespace std;


class FileInterface
{
private:
	// Registered SQF functions
	registered_sqf_function m_SQF_ofstream_new;
	registered_sqf_function m_SQF_ofstream_write;
	registered_sqf_function m_SQF_ofstream_write2;

	// Code that handles SQF function calls
	// Must be static to register it
	static game_value ofstream_new(uintptr_t state, game_value_parameter parameter);
	static game_value ofstream_write(uintptr_t state, game_value_parameter left, game_value_parameter right);

	// Array with open file handles
	struct openedFile_s {
		
		/*openedFile_s(ofstream* in_ofstreamPtr, string in_fileName) :
			ofstreamPtr(in_ofstreamPtr),
			fileName(in_fileName)
		{};*/

		ofstream* ofstreamPtr;
		string fileName;
	};
	vector<openedFile_s> m_files;

public:
	void preStart();

	// Construct/destruct
	FileInterface();
	~FileInterface();
};