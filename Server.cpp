
#include "Server.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <utility>
#include <stdexcept>
#include <ctime>
using namespace std;
using Clock = std::chrono::steady_clock;
using Headers = httplib::Headers;

std::unordered_map<std::string, std::chrono::steady_clock::time_point> oauthStates;

std::mutex oauthStatesMutex;
struct Bucket {
    double tokens = 10.0;
    chrono::steady_clock::time_point last = Clock::now();
};

unordered_map<string, Bucket> buckets;
mutex bucketMutex;

bool checkBucket(const string &ip) {
    const double MAX_TOKENS = 10.0;
    const double REFILL_RATE = 2.0; // tokens per second
    auto now = Clock::now();
    unique_lock<mutex> lock(bucketMutex);
    auto &b = buckets[ip];
    double elapsed = chrono::duration<double>(now - b.last).count();
    b.tokens = min(MAX_TOKENS, b.tokens + elapsed * REFILL_RATE);
    b.last = now;
    if (b.tokens >= 1.0) {
        b.tokens -= 1.0;
        return true;
    }

    return false;
}

// --- for google sign in ---
// In UrlShortenerServer.cpp
std::string UrlShortenerServer::getJsonValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t start = json.find(searchKey);
    if (start == std::string::npos) return "";

    start += searchKey.length();
    
    // Skip optional whitespace and check for opening quote
    while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) {
        start++;
    }

    if (json[start] != '"') {
        // Assume non-string value (like number or boolean) - not handled for simplicity
        return ""; 
    }
    
    start++; // Move past the opening quote
    size_t end = json.find('"', start);
    
    if (end == std::string::npos) return "";

    return json.substr(start, end - start);
}

std::string UrlShortenerServer::generateRandomState(size_t length) {
    const std::string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

    std::string random_string;
    for (size_t i = 0; i < length; ++i)
        random_string += CHARACTERS[distribution(generator)];
    return random_string;
}

// Function to generate a session token (simplistic UUID for testing)
std::string UrlShortenerServer::createSessionToken(unsigned int userId, const std::string& email) {
    // In a real application, this would be a secure JWT or a cryptographically secure random string.
    std::stringstream ss;
    ss << "sess_usr_" << userId << "_" << generateRandomState(16);
    return ss.str();
}



// --- Utility Implementation ---
// NEW: Handler for setting favorite status
void UrlShortenerServer::handleLinkFavorite(const httplib::Request &req, httplib::Response &res) {
    RequestContext ctx = get_context(res);
    
    if (!ctx.isAuthenticated || !checkUserRole(ctx, "user")) {
        res.status = 403;
        res.set_content("Forbidden: Requires a signed-in user.", "text/plain");
        return;
    }
    
    // Assume JSON body contains {"short_code": "xyz", "is_favorite": true}
    // Simple implementation relies on having the short_code in the request body
    std::string code = extractShortUrl(req.body); // Re-use parser for simplicity
    bool isFav = req.body.find("\"is_favourite\": true") != string::npos; 
    if (code.empty()) {
        res.status = 400;
        res.set_content("Missing short_code in request body.", "text/plain");
        return;
    }

    unique_lock<mutex> lock(dbMutex);
    if (db.setLinkFavorite(ctx.userId, code, isFav)) { // db.setLinkFavorite is assumed to be implemented
        res.status = 200;
        res.set_content("Favourite status updated successfully.", "text/plain");
    } else {
        res.status = 404;
        res.set_content("Link not found or does not belong to user.", "text/plain");
    }
}

// NEW: Handler for deleting a link
void UrlShortenerServer::handleLinkDelete(const httplib::Request &req, httplib::Response &res) {
    RequestContext ctx = get_context(res);
    
    if (!ctx.isAuthenticated || !checkUserRole(ctx, "user")) {
        res.status = 403;
        res.set_content("Forbidden: Requires a signed-in user.", "text/plain");
        return;
    }
    
    // Assuming short code is passed as a URL parameter like /api/link?code=xyz
    std::string code = req.has_param("code") ? req.get_param_value("code") : "";
    
    if (code.empty()) {
        res.status = 400;
        res.set_content("Missing 'code' parameter.", "text/plain");
        return;
    }

    unique_lock<mutex> lock(dbMutex);
    // db.deleteLink is assumed to be implemented and checks ownership (ctx.userId)
    if (db.deleteLink(ctx.userId, code)) { 
        res.status = 200;
        res.set_content("Link deleted successfully.", "text/plain");
    } else {
        res.status = 404;
        res.set_content("Link not found or does not belong to user.", "text/plain");
    }
}


// NEW: Handler for Admin-Only Test API
void UrlShortenerServer::handleAdminTest(const httplib::Request &req, httplib::Response &res) {
    RequestContext ctx = get_context(res);

    if (!ctx.isAuthenticated) {
        res.status = 401;
        res.set_content("Unauthorized. Please sign in.", "text/plain");
        return;
    }
    
    // **ADMIN ROLE CHECK**
    if (ctx.userRole != "admin") {
        res.status = 403;
        res.set_content("Forbidden: This API requires 'admin' role.", "text/plain");
        return;
    }

    res.status = 200;
    res.set_content("Welcome, Admin! This is a restricted endpoint.", "text/plain");
}
// Function to store the RequestContext in the Response object
void UrlShortenerServer::set_context(httplib::Response &res, const RequestContext &ctx) {
    // Stores context as a header string (best approach for httplib context passing)
    stringstream ss;
    ss << "Auth:" << (ctx.isAuthenticated ? "true" : "false")
       << ",Id:" << ctx.userId
       << ",Role:" << ctx.userRole;
    res.set_header("X-App-Context", ss.str());
}

// Function to retrieve the RequestContext from the Response object
RequestContext UrlShortenerServer::get_context(const httplib::Response &res) {
    RequestContext ctx;
    string contextHeader = res.get_header_value("X-App-Context");
    
    // Crude parsing for demonstration:
    if (contextHeader.find("Auth:true") != string::npos) {
        ctx.isAuthenticated = true;
        // Simple extraction of UserId (assuming "Id:XXX" format)
        size_t id_pos = contextHeader.find("Id:");
        size_t role_pos = contextHeader.find(",Role:");
        if (id_pos != string::npos && role_pos != string::npos) {
            string id_str = contextHeader.substr(id_pos + 3, role_pos - (id_pos + 3));
            try {
                ctx.userId = stoul(id_str);
            } catch (...) {
                ctx.userId = 0; // Invalid ID
            }
        }
        // Role is usually not strictly necessary for checks but is available
        if (contextHeader.find("Role:admin") != string::npos) {
            ctx.userRole = "admin";
        } else {
            ctx.userRole = "user";
        }
    }
    return ctx;
}

// Generates a random 8-character alphanumeric short code
string UrlShortenerServer::generateShortCode(size_t length) {
    const string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    random_device random_device;
    mt19937 generator(random_device());
    uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

    string random_string;
    for (size_t i = 0; i < length; ++i)
        random_string += CHARACTERS[distribution(generator)];
    return random_string;
}

string UrlShortenerServer::extractLongUrl(const string &body) {
    string target = "\"long_url\"";
    size_t start = body.find(target);
    if (start == string::npos) return "";

    // Find the first quote after the colon (ignore whitespace)
    start = body.find(':', start);
    if (start == string::npos) return "";

    // Skip spaces
    while (start < body.size() && isspace(body[start + 1])) start++;

    // Find the opening quote
    start = body.find('"', start);
    if (start == string::npos) return "";

    size_t end = body.find('"', start + 1);
    if (end == string::npos) return "";

    return body.substr(start + 1, end - start - 1);
}
string UrlShortenerServer::extractShortUrl(const string &body) {
    string target = "\"short_code\"";
    size_t start = body.find(target);
    if (start == string::npos) return "";

    // Find the first quote after the colon (ignore whitespace)
    start = body.find(':', start);
    if (start == string::npos) return "";

    // Skip spaces
    while (start < body.size() && isspace(body[start + 1])) start++;

    // Find the opening quote
    start = body.find('"', start);
    if (start == string::npos) return "";

    size_t end = body.find('"', start + 1);
    if (end == string::npos) return "";

    return body.substr(start + 1, end - start - 1);
}
// Database-backed quota check
bool UrlShortenerServer::checkAndApplyRateLimitDB(const string &guestId) {
    try {
        string today = UrlShortenerDB::getTodayDate(); 
        
        // 1. Check global limit toggle
        if (!db.isQuotaLimitEnabled()) {
             return true; 
        }
        
        // 2. Check and update the guest_daily_quotas table
        unique_lock<mutex> lock(dbMutex);
        // This method handles both the read and the conditional write
        return db.checkAndUpdateGuestQuota(guestId, today); 

    } catch (const exception& e) {
        cerr << "DB_ERROR in checkAndApplyRateLimitDB: " << e.what() << endl;
        // Fail open: if DB is down, allow the user for now, but log the error.
        return true; 
    }
}


// --- Class Implementation ---

UrlShortenerServer::UrlShortenerServer(UrlShortenerDB& db_instance, std::mutex& db_mutex_ref)
    : db(db_instance), dbMutex(db_mutex_ref) {
    setupMiddleware();
    setupRoutes();
}

bool UrlShortenerServer::run() {
    cerr << "Starting URL Shortener Service on port 9080..." << endl;
    return svr.listen("0.0.0.0", 9080);
}
// NEW: Endpoint Stat Tracking Middleware Implementation
httplib::Server::HandlerResponse UrlShortenerServer::EndpointStatMiddleware(const httplib::Request &req, httplib::Response &res) {
    
    std::string endpointPath = req.path;
    std::string httpMethod = req.method;
    std::string clientIp = req.remote_addr;
    
    // Simple way to handle the dynamic redirect route /<short_code>
    // Note: This relies on the fact that only '/<short_code>' is a short dynamic path.
    if (httpMethod == "GET") {
        db.incrementEndpointStat(endpointPath, httpMethod, clientIp);
        if (endpointPath != "/api/links" && endpointPath != "/api/admin" && endpointPath != "/auth/google/callback") {
            endpointPath = R"(/(\w+))";
            db.incrementEndpointStat(endpointPath, httpMethod, clientIp);
        }
    }
    else {
        db.incrementEndpointStat(endpointPath, httpMethod, clientIp);
    }
    return httplib::Server::HandlerResponse::Unhandled;
}
// --- Middleware Setup ---
void UrlShortenerServer::setupMiddleware() {
    cerr << "ckonekw" << endl; // This runs during initialization

    // Register ONE SINGLE pre-routing handler function
    svr.set_pre_routing_handler([this](const httplib::Request &req, httplib::Response &res) {
        
        // 1. Run AuthMiddleware FIRST
        // This includes Rate Limiting and setting the RequestContext
        httplib::Server::HandlerResponse auth_result = this->AuthMiddleware(req, res);

        // Check if AuthMiddleware decided to handle (terminate) the request (e.g., 429 or 401)
        if (auth_result == httplib::Server::HandlerResponse::Handled) {
            return auth_result; // Stop the request immediately
        }
        
        // 2. Run EndpointStatMiddleware SECOND
        // This is primarily for logging/stats and should typically return Unhandled
        httplib::Server::HandlerResponse stat_result = this->EndpointStatMiddleware(req, res);

        // If EndpointStatMiddleware somehow returns Handled, use that.
        // Otherwise, it should always return Unhandled to proceed to the route.
        return stat_result; 
    });

    // NOTE: Logger Middleware would be integrated here as well.
}
bool UrlShortenerServer::checkUserRole(const RequestContext &ctx, const std::string &requiredRole) {
    if (ctx.userRole == "admin") {
        return true;
    }
    return ctx.userRole == requiredRole;
}

/// @deprecated Already handled in pre request...
bool UrlShortenerServer::checkAndApplyUserLimit(unsigned int userId) {
    return true; 
}
// Implements Token Expiration Check
httplib::Server::HandlerResponse UrlShortenerServer::AuthMiddleware(const httplib::Request &req, httplib::Response &res) {
    RequestContext ctx;
    string token;
    std::string clientIp = req.remote_addr;
    if (!checkBucket(clientIp)) {
        res.status = 429; // Too Many Requests
        res.set_content("Rate limit exceeded. Please slow down.", "text/plain");
        return httplib::Server::HandlerResponse::Handled; // Stop request here
    }

    if (req.has_header("Authorization")) {
        string authHeader = req.get_header_value("Authorization");
        if (authHeader.size() > 7 && authHeader.substr(0, 7) == "Bearer ") {
            token = authHeader.substr(7);
        }
    }

    if (!token.empty()) {
        try {
            unique_lock<mutex> lock(dbMutex);
            unique_ptr<::Session> sessionObj = db.findSessionByToken(token);
            
            if (sessionObj) {
                // If findSessionByToken was successful, it means the token was valid AND NOT expired (Token Expiration Check)
                ctx.isAuthenticated = true;
                ctx.userId = sessionObj->user_id;
                ctx.userRole = (sessionObj->user_id == 1) ? "admin" : "user";
            } else {
                // Token exists but is invalid or expired, delete it from DB (Token Expiration Cleanup)
                db.deleteSession(token);
            }
        } catch (const exception& e) {
            cerr << "DB_ERROR in AuthMiddleware: " << e.what() << endl;
            // Fail safely: treat as unauthenticated if DB call fails
        }
    }

    set_context(res, ctx);
    return httplib::Server::HandlerResponse::Unhandled;
}


// --- Route Setup ---
void UrlShortenerServer::setupRoutes() {
    // POST /shorten - Link Creation Endpoint
    svr.Post("/shorten", [this](const httplib::Request &req, httplib::Response &res) {
        this->handleShorten(req, res);
    });

    // GET /<short_code> - Redirection Endpoint (No Auth Needed)
    svr.Get(R"(/(\w+))", [this](const httplib::Request &req, httplib::Response &res) {
        this->handleRedirect(req, res);
    });
    
    // GET /api/links - Link Management Dashboard (USER/ADMIN)
    svr.Get("/api/links", [this](const httplib::Request &req, httplib::Response &res) {
        this->handleUserLinks(req, res);
    });
    
    // POST /api/link/favorite - Set favorite status (USER/ADMIN)
    svr.Post("/api/link/favourite", [this](const httplib::Request &req, httplib::Response &res) {
        this->handleLinkFavorite(req, res);
    });
    
    // DELETE /api/link - Delete a link (USER/ADMIN)
    svr.Delete("/api/link", [this](const httplib::Request &req, httplib::Response &res) {
        this->handleLinkDelete(req, res);
    });
    
    // GET /api/admin - Admin-Only Endpoint
    svr.Get("/api/admin", [this](const httplib::Request &req, httplib::Response &res) {
        this->handleAdminTest(req, res);
    });
    
    // for signin stuff
        svr.Get("/auth/google", [this](const httplib::Request &req, httplib::Response &res) {
        this->handleGoogleRedirect(req, res);
    });

    // OAuth Callback endpoint - Now fully implemented
    svr.Get("/auth/google/callback", [this](const httplib::Request &req, httplib::Response &res) {
        this->handleGoogleCallback(req, res);
    });

    // NEW: Mock success page to display the token after sign-in
    // NEW: Mock success page to display the token after sign-in (NOW SECURED WITH DB CHECK)
    svr.Get("/auth/success", [this](const httplib::Request &req, httplib::Response &res) {
        std::string token = req.get_param_value("token");
        std::string user = req.get_param_value("user");

        if (token.empty()) {
            res.status = 400;
            res.set_content("Authentication failed or token was not provided.", "text/html");
            return;
        }

        // === NEW: CHECK SESSION VALIDITY AGAINST DB ===
        unique_lock<mutex> lock(dbMutex);
        // db.findSessionByToken returns nullptr if token is not found OR has expired.
        std::unique_ptr<Session> sessionObj = db.findSessionByToken(token);
        lock.unlock();

        if (!sessionObj) {
            res.status = 401;
            res.set_content("Session has expired or is invalid. Please restart the sign-in process via /auth/google.", "text/plain");
            return;
        }
        // === END DB CHECK ===

        std::stringstream html;
        html << R"html(
            <!DOCTYPE html>
            <html lang="en">
            <head><meta charset="UTF-8"><title>Login Success</title>
            <style>
                body { font-family: sans-serif; background-color: #f0f4f8; padding: 40px; }
                .container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); max-width: 800px; margin: auto; }
                h2 { color: #10b981; border-bottom: 2px solid #10b981; padding-bottom: 10px; }
                p { margin-top: 15px; }
                code.token { display: block; background-color: #e2e8f0; padding: 15px; border-radius: 4px; font-size: 1.1em; overflow-wrap: break-word; user-select: all; margin-bottom: 20px;}
                code.command { display: block; background-color: #334155; color: #f1f5f9; padding: 10px; border-radius: 4px; font-size: 0.9em; overflow-x: auto; white-space: pre-wrap; word-break: break-all; margin-bottom: 15px; }
                .api-section { margin-top: 25px; border-top: 1px dashed #cbd5e1; padding-top: 15px; }
                .api-section h3 { color: #1e3a8a; margin-bottom: 5px; }
                .instruction { background-color: #f0f9ff; border-left: 4px solid #3b82f6; padding: 15px; border-radius: 4px; color: #1e40af; font-size: 0.9em; margin-bottom: 20px;}
            </style>
            </head>
            <body>
            <div class="container">
                <h2>✅ Login Successful, )html" << user << R"html(!</h2>
                
                <div class="instruction">
                    <p>Your session is active. All authenticated API requests require the <strong>Bearer Token</strong> below in the <code>Authorization</code> header.</p>
                </div>

                <h3>1. Your Session Token (Click to Copy):</h3>
                <code class="token" id="tokenValue">)html" << token << R"html(</code>
                
                <div class="api-section">
                    <h3>2. API Testing Commands:</h3>
                    
                    <p><strong>A. View Your Links</strong> (GET /api/links)</p>
                    <code class="command">curl -i -X GET http://localhost:9080/api/links \
     -H "Authorization: Bearer )html" << token << R"html("</code>
                    
                    <p><strong>B. Create New Link (requires Custom Code)</strong> (POST /shorten)</p>
                    <code class="command">curl -i -X POST 'http://localhost:9080/shorten?custom_code=testlink1' \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer )html" << token << R"html(" \
     -d '{"long_url": "https://www.auth-test-site.com/new"}'</code>

                    <p><strong>C. Set Link as Favourite</strong> (POST /api/link/favourite)</p>
                    <code class="command">curl -i -X POST http://localhost:9080/api/link/favourite \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer )html" << token << R"html(" \
     -d '{"short_code": "testlink1", "is_favourite": true}'</code>

                    <p><strong>D. Delete a Link</strong> (DELETE /api/link)</p>
                    <code class="command">curl -i -X DELETE 'http://localhost:9080/api/link?code=testlink1' \
     -H "Authorization: Bearer )html" << token << R"html("'</code>
                </div>
            </div>
            <script>
                document.getElementById('tokenValue').onclick = function() {
                    const token = this.innerText;
                    // Note: window.alert() is used here for a simple success notification
                    // In a production app, use a custom modal.
                    alert('Token copied to clipboard!');
                    
                };
            </script>
            </body>
            </html>
        )html";
        res.set_content(html.str(), "text/html");
    });
}
// --- Route Handlers ---

// --- Route Handlers ---

void UrlShortenerServer::handleShorten(const httplib::Request &req, httplib::Response &res) {
    RequestContext ctx = get_context(res);
    string longUrl = extractLongUrl(req.body);
    string customCode = req.has_param("custom_code") ? req.get_param_value("custom_code") : "";
    string expiryDate = req.has_param("expires_at") ? req.get_param_value("expires_at") : "";
    string clientIp = req.remote_addr;
    
    if (longUrl.empty()) {
        res.status = 400;
        res.set_content("Missing 'long_url' in request body.", "text/plain");
        return;
    }
    if (longUrl.size() > Config::MAX_URL_LENGTH) {
        res.status = 400;
        res.set_content("URL too long. Maximum allowed length is 2048 characters.", "text/plain");
        return;
    }
    if (longUrl.compare(0, 7, "http://") != 0 && longUrl.compare(0, 8, "https://") != 0) {
        res.status = 400;
        res.set_content("Invalid URL protocol.", "text/plain");
        return;
    }



    // --- 1. Custom Code Restriction ---
    if (!customCode.empty() && !ctx.isAuthenticated) {
        res.status = 403;
        res.set_content("Custom short codes require a signed-in account.", "text/plain");
        return;
    }

    // --- 2. Rate Limiting ---
    if (ctx.isAuthenticated) {
        if (!checkAndApplyUserLimit(ctx.userId)) { // Check authenticated user limit
            res.status = 429; // Too Many Requests
            res.set_content("Link creation limit (200/hr) reached. Please wait.", "text/plain");
            return;
        }
    } else {
        if (!checkAndApplyRateLimitDB(clientIp)) { // Check guest limit
            res.status = 403;
            res.set_content("Limit reached. Max "
                            + db.getConfig("MAX_GUEST_LINKS_PER_DAY")
                            + " per day. Please log in.",
                            "text/plain");
            return;
        }
    }
    
    // --- 3. Determine Short Code & Conflict Handling ---
    string shortCode = customCode;
    if (shortCode.empty()) {
        // Auto-generated code loop, handles conflict by retrying
        do {
            shortCode = generateShortCode();
        } while (db.getLinkByShortCode(shortCode)); 
    } else {
        // Custom code conflict check (handled gracefully by DB in step 4)
        // We rely on the DB's unique constraint check (step 4) to handle conflict for custom codes.
    }

        // Link Expiration
    // 3. Prepare Link DTO (Authenticated Link Creation/Expiration)
        ShortenedLink linkToSave;
        linkToSave.original_url = longUrl;
        linkToSave.short_code = shortCode;
    
        if (ctx.isAuthenticated) {
                 // Authenticated Link Creation
                 linkToSave.user_id = make_unique<unsigned int>(ctx.userId); 
                 linkToSave.guest_identifier = "";
            } else {
                     linkToSave.user_id = nullptr;
                     linkToSave.guest_identifier = clientIp;
                }
            
                // Link Expiration
    linkToSave.expires_at = expiryDate!=""?expiryDate:"";
            // 4. Save Link
    unique_lock<mutex> lock(dbMutex);
    if (!db.createLink(linkToSave)) {
        // If a custom short code was used and failed the unique constraint check (DB error 409)
        res.status = 409; 
        res.set_content("Short code '" + shortCode + "' is already taken or DB error occurred.", "text/plain");
        return;
    }

    // 5. Response
    string fullShortUrl =  Config::BASE_URL + shortCode;
    stringstream ss;
    ss << "{ \"short_url\": \"" << fullShortUrl
       << "\", \"long_url\": \"" << longUrl 
       << "\", \"authenticated\": " << (ctx.isAuthenticated ? "true" : "false")
       << ", \"user_id\": " << ctx.userId
       << " }";

    res.status = 201;
    res.set_content(ss.str(), "application/json");
}

void UrlShortenerServer::handleRedirect(const httplib::Request &req, httplib::Response &res) {
    string code = req.matches[1];
    
    unique_lock<mutex> lock(dbMutex);
    unique_ptr<ShortenedLink> link = db.getLinkByShortCode(code);
    
    if (link) {
        // Link Expiration Check is done inside getLinkByShortCode (WHERE expires_at > NOW())
        
        // Link Analytics (Click Tracking)
        db.incrementLinkClicks(link->id); 
        
        // Redirect
        res.set_redirect(link->original_url);
    } else {
        res.status = 404;
        res.set_content("Short code not found or has expired.", "text/plain");
    }
}

// Implements Link Management Dashboard (Read All Links by User)
void UrlShortenerServer::handleUserLinks(const httplib::Request &req, httplib::Response &res) {
    RequestContext ctx = get_context(res);

    if (!ctx.isAuthenticated) {
        res.status = 401;
        res.set_content("Unauthorized. Please provide a valid Authorization token.", "text/plain");
        return;
    }
    
    // **NEW ROLE CHECK** (User or Admin required)
    if (!checkUserRole(ctx, "user")) {
        res.status = 403;
        res.set_content("Forbidden: Insufficient privileges.", "text/plain");
        return;
    }

    unique_lock<mutex> lock(dbMutex);
    unique_ptr<vector<ShortenedLink>> links = db.getLinksByUserId(ctx.userId);
    
    // Convert links vector to a JSON array string for response
    stringstream ss;
    ss << "[";
    bool first = true;
    for (const auto& link : *links) {
        if (!first) ss << ",";
        ss << "{\"code\":\"" << link.short_code << "\",";
        ss << "\"url\":\"" << link.original_url << "\",";
        ss << "\"clicks\":" << link.clicks << ","; // Link Analytics
        ss << "\"expires_at\":\"" << link.expires_at << "\"}";
        first = false;
    }
    ss << "]";

    res.status = 200;
    res.set_content(ss.str(), "application/json");
}






// NEW: Initiates the OAuth flow by redirecting to Google
void UrlShortenerServer::handleGoogleRedirect(const httplib::Request &req, httplib::Response &res) {
    std::string state = generateRandomState(16);

    // CRITICAL FIX: Store the state token server-side for validation in the callback
    {
        std::lock_guard<std::mutex> lock(oauthStatesMutex);
        // State expires after 5 minutes
        oauthStates[state] = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    }

    std::stringstream authUrl;
    authUrl << "https://accounts.google.com/o/oauth2/v2/auth?"
            << "client_id=" << Config::GOOGLE_CLIENT_ID
            << "&response_type=code"
            << "&scope=openid%20email%20profile"
            << "&redirect_uri=" << Config::GOOGLE_REDIRECT_URI
            << "&state=" << state;

    res.status = 302;
    res.set_header("Location", authUrl.str());
    res.set_content("Redirecting to Google for authentication...", "text/plain");

    std::cerr << "SERVER_INFO: Redirecting client for Google OAuth. State saved." << std::endl;
}


// FULL IMPLEMENTATION: Handles the callback, token exchange, user creation, and session management
void UrlShortenerServer::handleGoogleCallback(const httplib::Request &req, httplib::Response &res) {
    std::string code = req.get_param_value("code");
    std::string state = req.get_param_value("state");

    if (code.empty()) {
        res.status = 400;
        res.set_content("Missing authorization code.", "text/plain");
        return;
    }
    
    // Begin comprehensive exception handling for external calls
    try {
        // CRITICAL: 0. Validate and Consume State Token (CSRF Check)
        {
            std::lock_guard<std::mutex> lock(oauthStatesMutex);
            auto it = oauthStates.find(state);

            if (it == oauthStates.end()) {
                std::cerr << "SERVER_SECURITY_ERROR: Invalid CSRF state received (not found)." << std::endl;
                res.status = 403;
                res.set_content("CSRF validation failed: Invalid or missing state token.", "text/plain");
                return;
            }
            if (it->second < std::chrono::steady_clock::now()) {
                std::cerr << "SERVER_SECURITY_ERROR: Expired CSRF state received." << std::endl;
                res.status = 403;
                res.set_content("CSRF validation failed: Expired state token.", "text/plain");
                oauthStates.erase(it); // Clean up expired token
                return;
            }
            
            // State is valid and not expired, consume it immediately to prevent replay/reuse
            oauthStates.erase(it);
        }
        std::cerr << "SERVER_DEBUG: State validated. Code received. Proceeding to token exchange." << std::endl;

        // 1. Prepare POST body
        std::stringstream postBody; 
        postBody << "code=" << code
                 << "&client_id=" << Config::GOOGLE_CLIENT_ID
                 << "&client_secret=" << Config::GOOGLE_CLIENT_SECRET
                 << "&redirect_uri=" << Config::GOOGLE_REDIRECT_URI
                 << "&grant_type=authorization_code";
        
        std::string finalPostBody = postBody.str();
        std::cerr << "SERVER_DEBUG: Token exchange POST body: " << finalPostBody << std::endl; // LOG POST BODY
        
        // Prepare headers
        Headers headers;
        headers.insert(std::make_pair("Content-Type", "application/x-www-form-urlencoded"));
        
        // FIX: Construct SSLClient immediately before use to minimize memory window
        httplib::SSLClient cli("oauth2.googleapis.com");
        
        // Note: For SSLClient, the path is '/token' since the host is set in the constructor
        auto token_res = cli.Post("/token", headers, finalPostBody, "application/x-www-form-urlencoded");

        if (!token_res) {
            std::cerr << "SERVER_FATAL: Token exchange request failed (connection error or unhandled exception)." << std::endl;
            res.status = 500;
            res.set_content("Authentication failed: Unable to connect to Google token endpoint.", "text/plain");
            return;
        }

        if (token_res->status != 200) {
            std::cerr << "SERVER_ERROR: Token exchange failed. Status: " << token_res->status << std::endl;
            std::cerr << "SERVER_ERROR: Google Response Body: " << token_res->body << std::endl; // LOG ERROR BODY
            res.status = 500;
            res.set_content("Failed to exchange code for token. Check server logs.", "text/plain");
            return;
        }

        // 2. Parse the JSON response for tokens
        std::string id_token = getJsonValue(token_res->body, "id_token");
        std::string access_token = getJsonValue(token_res->body, "access_token");
        std::cerr << "SERVER_DEBUG: Tokens extracted. ID Token start: " << id_token.substr(0, 10) << "..." << std::endl; // LOG SUCCESS

        if (id_token.empty() || access_token.empty()) {
            std::cerr << "SERVER_ERROR: Failed to extract tokens from response." << std::endl;
            res.status = 500;
            res.set_content("Authentication failed: tokens missing.", "text/plain");
            return;
        }

        // 3. Get User Info (or decode ID Token) - Using the User Info endpoint is simpler for testing
        // FIX: Construct SSLClient immediately before use to minimize memory window
        httplib::SSLClient userCli("www.googleapis.com"); 
        Headers userHeaders; // Use explicit declaration
        userHeaders.insert(std::make_pair("Authorization", "Bearer " + access_token)); // Use explicit insert

        auto user_info_res = userCli.Get("/oauth2/v3/userinfo", userHeaders);

        if (!user_info_res || user_info_res->status != 200) {
            std::cerr << "SERVER_ERROR: Failed to fetch user info. Status: " << (user_info_res ? std::to_string(user_info_res->status) : "Unknown") << std::endl;
            std::cerr << "SERVER_ERROR: User Info Response Body: " << (user_info_res ? user_info_res->body : "No response body.") << std::endl; // LOG USER INFO ERROR
            res.status = 500;
            res.set_content("Failed to retrieve user details.", "text/plain");
            return;
        }

        // 4. Extract user data (simplified JSON parsing)
        std::string email = getJsonValue(user_info_res->body, "email");
        std::string name = getJsonValue(user_info_res->body, "name");
        std::string google_id = getJsonValue(user_info_res->body, "sub"); // 'sub' is the unique Google ID
        std::cerr << "SERVER_DEBUG: User Info received for: " << email << std::endl; // LOG USER DATA

        if (email.empty() || name.empty() || google_id.empty()) {
            std::cerr << "SERVER_ERROR: Missing essential user info (email/name/sub)." << std::endl;
            res.status = 500;
            res.set_content("Incomplete user profile received.", "text/plain");
            return;
        }

        // 5. Create/Find User and Create Session
        unique_lock<mutex> lock(dbMutex);
        
        unique_ptr<User> existingUser = db.findUserByEmail(email);
        unsigned int userId;

        if (!existingUser) {
            // User does not exist, create new account
            User newUser;
            newUser.google_id = google_id;
            newUser.email = email;
            newUser.name = name;
            if (db.createUser(newUser)) {
                // Re-fetch the user to get the auto-generated ID
                existingUser = db.findUserByEmail(email); 
            }
        }
        
        if (!existingUser) {
            std::cerr << "DB_ERROR: Failed to create or retrieve user after login." << std::endl;
            res.status = 500;
            res.set_content("User management failed.", "text/plain");
            return;
        }

        userId = existingUser->id;
        std::string token = createSessionToken(userId, email);

        Session sessionObj;
        sessionObj.user_id = userId;
        sessionObj.session_token = token;
        sessionObj.expires_at = db.getFutureTimestamp(30); // 30 days expiration

        if (!db.createSession(sessionObj)) {
            std::cerr << "DB_ERROR: Failed to create session for user " << userId << std::endl;
            res.status = 500;
            res.set_content("Session creation failed.", "text/plain");
            return;
        }

        // 6. Respond to Client: Redirect to a success page with the token
        std::stringstream successRedirectUrl;
        
        // FIX: Ensure there is a slash after the BASE_URL if it doesn't end with one.
        // The safest approach is to check if BASE_URL ends with a slash, and if not, add one.
        std::string baseUrl = Config::BASE_URL;
        if (!baseUrl.empty() && baseUrl.back() != '/') {
            baseUrl += "/";
        }
        
        successRedirectUrl << baseUrl << "auth/success?token=" << token << "&user=" << name;

        res.status = 302;
        res.set_header("Location", successRedirectUrl.str());
        res.set_content("Authentication successful. Redirecting...", "text/plain");
        std::cerr << "SERVER_INFO: User " << userId << " authenticated successfully. Redirecting with token." << std::endl;
        
    } catch (const std::exception& e) {
        // Catch any standard exceptions thrown by networking or memory issues
        std::cerr << "SERVER_FATAL_CATCH: An unexpected C++ exception occurred: " << e.what() << std::endl;
        res.status = 500;
        res.set_content("Critical server error during authentication.", "text/plain");
    } catch (...) {
        // Catch all other unknown exceptions (e.g., signals, non-standard exceptions)
        std::cerr << "SERVER_FATAL_CATCH: An unknown critical error occurred during authentication." << std::endl;
        res.status = 500;
        res.set_content("Unknown critical server error.", "text/plain");
    }
}
