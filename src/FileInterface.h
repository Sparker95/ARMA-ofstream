#pragma once

#include <intercept.hpp>


// Class that lets SQF interract with files
// Author: Sparker 20.02.2019

using namespace intercept;

class FileInterface
{
private:
    // Registered SQF functions
    registered_sqf_function m_SQF_ofstream_new;
    registered_sqf_function m_SQF_ofstream_write;
    registered_sqf_function m_SQF_ofstream_write2;

    // Code that handles SQF function calls
    // Must be static to register it
    static game_value ofstream_new(game_state& state, game_value_parameter parameter);
    static game_value ofstream_write(game_state& state, game_value_parameter left, game_value_parameter right);

public:
    void preStart();
};