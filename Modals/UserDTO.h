#pragma once

#include <string>

struct User {
    unsigned int id = 0;
    std::string google_id;
    std::string email;
    std::string name;
    std::string created_at;
    std::string updated_at;
};
