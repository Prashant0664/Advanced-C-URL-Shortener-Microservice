#pragma once

#include <string>

struct Session {
    unsigned int id = 0;
    unsigned int user_id;
    std::string session_token;
    std::string expires_at;
    std::string created_at;
};