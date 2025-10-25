#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <optional>

// --- New MySQL X DevAPI Headers ---
#include <mysqlx/xdevapi.h>

// --- Configuration Header ---
#include "Config.h"

// --- DTO Headers ---
#include "Modals/UserDTO.h"
#include "Modals/SessionDTO.h" // Contains struct Session
#include "Modals/ShortenedLink.h"
#include "Modals/QuotaDTO.h"
#include "Modals/GlobalSettingDTO.h"


class UrlShortenerDB {
private:
    // This is explicitly qualified as the MySQL database session class
    std::unique_ptr<mysqlx::Session> session;  
    std::optional<mysqlx::Schema> schema;
    bool isConnected = false;

    // Helper method signature (using fixed ABI)
    std::unique_ptr<mysqlx::RowResult> executeStatement(
        const std::string& sql, 
        const std::vector<mysqlx::abi2::Value>& params
    );

public:
    UrlShortenerDB();
    ~UrlShortenerDB();

    bool connect();
    bool setupDatabase();

    // --- Time/Date Helpers (NEW) ---
    static std::string getTodayDate();
    static std::string getCurrentTimestamp();
    static std::string getFutureTimestamp(int days);

    // --- User & Session Methods ---
    bool createUser(const User& user); 
    std::unique_ptr<User> findUserByGoogleId(const std::string& google_id);
    
    // Explicitly refer to the DTO struct using the global scope operator (::)
    bool createSession(const ::Session& sessionObj);
    std::unique_ptr<::Session> findSessionByToken(const std::string& token);
    std::unique_ptr<User> findUserByEmail(const std::string& email); // for google sign in automated
    // NEW: Token Expiration / Logout
    bool deleteSession(const std::string& token); 

    // --- Link Creation & Retrieval ---
    bool createLink(const ShortenedLink& link);
    std::unique_ptr<ShortenedLink> getLinkByShortCode(const std::string& code);
    
    // NEW: Link Analytics (Click Tracking)
    bool incrementLinkClicks(unsigned int link_id); 
    bool incrementEndpointStat(const std::string& endpoint, const std::string& method, const std::string& createdBy);
    
    // NEW: Link Management Dashboard (Read All Links by User)
    std::unique_ptr<std::vector<ShortenedLink>> getLinksByUserId(unsigned int user_id);

    // --- Quota Management ---
    bool isQuotaLimitEnabled();
    bool checkAndUpdateGuestQuota(const std::string& guest_identifier, const std::string& today_date);

    // global settings
    std::string getConfig(std::string key);
    bool setLinkFavorite(const int&userId, const std::string&code, const bool&isFav);
    bool deleteLink(const int&id, const std::string&code);




};
