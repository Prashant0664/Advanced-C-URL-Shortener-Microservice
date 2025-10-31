#pragma once

#include <string>

class Config {
public:
    static const std::string DB_HOST;
    static const std::string DB_USER;
    static const std::string DB_PASS;
    static const std::string DB_NAME;
    static const int DB_PORT;
    static const std::string BASE_URL;
    static const size_t MAX_URL_LENGTH;
    static const int LINK_EXPIRED_IN;
    static const std::string GOOGLE_CLIENT_ID;
    static const std::string GOOGLE_REDIRECT_URI;
    static const std::string GOOGLE_CLIENT_SECRET;
};
