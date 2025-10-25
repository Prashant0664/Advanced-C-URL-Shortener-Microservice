#pragma once

#include <string>

struct GuestQuota {
    unsigned int id = 0;
    std::string guest_identifier;
    std::string quota_date; // SQL DATE type represented as string (YYYY-MM-DD)
    unsigned char links_created = 0; 
};
