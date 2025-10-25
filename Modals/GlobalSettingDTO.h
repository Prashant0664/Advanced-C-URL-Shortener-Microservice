#pragma once

#include <string>

struct GlobalSetting {
    unsigned int id = 0;
    std::string setting_key;
    std::string setting_value;
    std::string description;
};
