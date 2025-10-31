#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <mutex>
#include <queue>
#include <condition_variable>

#include <mysqlx/xdevapi.h>

#include "Config.h"

// --- DTO Headers ---
#include "Modals/UserDTO.h"
#include "Modals/SessionDTO.h"
#include "Modals/ShortenedLink.h"
#include "Modals/QuotaDTO.h"
#include "Modals/GlobalSettingDTO.h"


class UrlShortenerDB {
private:
    std::queue<std::unique_ptr<mysqlx::Session>> connectionPool;
    std::mutex poolMutex;
    std::condition_variable poolCv;
    int poolSize = 10; 
    
    std::unique_ptr<mysqlx::Session> getConnection();
    bool isConnected = false;

    std::unique_ptr<mysqlx::RowResult> executeStatement(
        const std::string& sql, 
        const std::vector<mysqlx::abi2::Value>& params
    );

public:
    UrlShortenerDB();
    ~UrlShortenerDB();

    bool connect();
    bool setupDatabase();

    // --- Time/Date Helpers ---
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
    
    // Token Expiration / Logout
    bool deleteSession(const std::string& token); 

    // --- Link Creation & Retrieval ---
    bool createLink(const ShortenedLink& link);
    std::unique_ptr<ShortenedLink> getLinkByShortCode(const std::string& code);
    
    // Link Analytics (Click Tracking)
    bool incrementLinkClicks(unsigned int link_id); 
    bool incrementEndpointStat(const std::string& endpoint, const std::string& method, const std::string& createdBy);
    
    // Link Management Dashboard (Read All Links by User)
    std::unique_ptr<std::vector<ShortenedLink>> getLinksByUserId(unsigned int user_id);

    // --- Quota Management ---
    bool isQuotaLimitEnabled();
    bool checkAndUpdateGuestQuota(const std::string& guest_identifier, const std::string& today_date);

    // global settings
    std::string getConfig(std::string key);
    std::string getConfig(mysqlx::Session& currentSession, std::string key);
    
    bool setLinkFavorite(const int&userId, const std::string&code, const bool&isFav);
    
    bool deleteLink(const int&id, const std::string&code);
    
    std::unique_ptr<mysqlx::RowResult> executeStatement(
        mysqlx::Session& currentSession, 
        const std::string& sql, 
        const std::vector<mysqlx::Value>& params
    );
    
    void returnConnection(std::unique_ptr<mysqlx::Session> session);

};
