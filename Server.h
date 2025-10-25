#pragma once
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <iostream>
#include <mutex>
#include <string>

#include "URLShortnerDB.h"
#include "Config.h"
// Include DTOs used in Middleware/Context
#include "Modals/SessionDTO.h"

// Structure to hold current request context after middleware runs
struct RequestContext {
    bool isAuthenticated = false;
    unsigned int userId = 0; // 0 for unauthenticated
    std::string userRole = "guest";
};

class UrlShortenerServer {
public:
    UrlShortenerServer(UrlShortenerDB& db_instance, std::mutex& db_mutex_ref);

    // Runs the server
    bool run();

private:
    httplib::Server svr;
    UrlShortenerDB& db;
    std::mutex& dbMutex;
    
    // --- Middleware ---
    void setupMiddleware();
    
    // Checks for Auth Token and Sets Context
    httplib::Server::HandlerResponse AuthMiddleware(const httplib::Request &req, httplib::Response &res);

    // --- Utility ---
    static void set_context(httplib::Response &res, const RequestContext &ctx);
    static RequestContext get_context(const httplib::Response &res);
    bool checkAndApplyRateLimitDB(const std::string &guestId);
    static std::string generateShortCode(size_t length = 8);
    static std::string extractLongUrl(const std::string &body);
    void handleLinkFavorite(const httplib::Request &req, httplib::Response &res);
    bool checkUserRole(const RequestContext &ctx, const std::string &requiredRole);
    bool checkAndApplyUserLimit(unsigned int userId);
    void handleLinkDelete(const httplib::Request &req, httplib::Response &res);
    void handleAdminTest(const httplib::Request &req, httplib::Response &res);
    std::string extractShortUrl(const std::string &body);
    httplib::Server::HandlerResponse EndpointStatMiddleware(const httplib::Request &req, httplib::Response &res);
    // --- Routes ---
    void setupRoutes();
    void handleShorten(const httplib::Request &req, httplib::Response &res);
    void handleRedirect(const httplib::Request &req, httplib::Response &res);
    void handleGoogleCallback(const httplib::Request &req, httplib::Response &res);
    
    // NEW API for Link Management Dashboard
    void handleUserLinks(const httplib::Request &req, httplib::Response &res);

    // --- google sign in ---
    std::string generateRandomState(size_t length);
    void handleGoogleRedirect(const httplib::Request &req, httplib::Response &res);
    std::string createSessionToken(unsigned int userId, const std::string& email);

     // Internal helper for JSON parsing (Simplification for no external JSON library)
    std::string getJsonValue(const std::string& json, const std::string& key);
};
