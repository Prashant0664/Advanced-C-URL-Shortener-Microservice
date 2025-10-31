#include "Config.h"
#include <cstdlib>
#include <string>
#include <iostream>
using namespace std;

// Helper lambda to safely read environment variables
// Provides a hardcoded default only for non-sensitive local testing setup.
static std::string getEnv(const char* name, const std::string& defaultValue = "") {
    const char* value = std::getenv(name);
    if (value && value[0] != '\0') {
        return std::string(value);
    }
    return defaultValue;
}

// --- Definitions of Static Member Variables ---

// Database Credentials (MUST be set via Environment Variables in Production)
const std::string Config::DB_HOST = getEnv("DB_HOST", "127.0.0.1"); 
const std::string Config::DB_USER = getEnv("DB_USER", "root");
const std::string Config::DB_PASS = getEnv("DB_PASS", "Prashant"); // Using the hardcoded value as a local fallback
const std::string Config::DB_NAME = getEnv("DB_NAME", "test_url"); 
const int Config::DB_PORT = std::stoi(getEnv("DB_PORT", "33060")); 

// Application URLs and Secrets
const std::string Config::BASE_URL = getEnv("BASE_URL", "http://localhost:9080/");

// Google OAuth Secrets (MUST be set via Environment Variables)
const std::string Config::GOOGLE_CLIENT_ID = getEnv("GOOGLE_CLIENT_ID", "DEF_PLACEHOLDER");
const std::string Config::GOOGLE_CLIENT_SECRET = getEnv("GOOGLE_CLIENT_SECRET", "ABC_PLACEHOLDER");
const std::string Config::GOOGLE_REDIRECT_URI = getEnv("GOOGLE_REDIRECT_URI", "http://localhost:9080/auth/google/callback");

// Non-sensitive Configuration
const std::size_t Config::MAX_URL_LENGTH = 2048;
const int Config::LINK_EXPIRED_IN = 30; // Defined as the default in Config.h, but kept here for completeness
