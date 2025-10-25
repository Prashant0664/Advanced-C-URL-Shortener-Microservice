#pragma once

#include <string>
#include <memory> 
#include <optional> // Using optional for cleaner clicks/analytics integration

struct ShortenedLink {
    unsigned int id = 0;
    std::string original_url;
    std::string short_code;
    std::unique_ptr<unsigned int> user_id; // Handles NULL (Authenticated Link Creation)
    std::string guest_identifier;
    std::string expires_at;               // Handles Link Expiration
    unsigned int clicks = 0;              // Handles Link Analytics
    std::string created_at;
    std::string updated_at;               // Added for consistency with DB update field
};