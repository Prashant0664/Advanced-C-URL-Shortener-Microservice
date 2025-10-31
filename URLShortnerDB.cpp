#include "URLShortnerDB.h"
#include "Config.h"
#include "Modals/UserDTO.h"
#include "Modals/SessionDTO.h"
#include "Modals/ShortenedLink.h"
#include "Modals/QuotaDTO.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <fstream>
#include <iomanip>
#include <chrono> // For chrono usage

// Include necessary standard namespace functions explicitly to resolve ambiguities
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::unique_ptr;
using std::runtime_error;
using std::lock_guard; // Use lock_guard for pool access
using std::unique_lock;

// Use namespace aliases for MySQL types to improve readability inside the functions
using mysqlx::abi2::Value;
using mysqlx::abi2::RowResult;

// --- CONNECTION POOL HELPERS (NEW) ---

std::unique_ptr<mysqlx::Session> UrlShortenerDB::getConnection() {
    unique_lock<std::mutex> lock(poolMutex);
    
    // Wait until the pool has a connection available (with a timeout)
    if (connectionPool.empty()) {
        if (poolCv.wait_for(lock, std::chrono::seconds(5)) == std::cv_status::timeout) {
            throw std::runtime_error("Database pool timeout: No connections available.");
        }
    }
    
    if (connectionPool.empty()) {
        throw std::runtime_error("Database pool is empty after waiting.");
    }

    // De-queue the connection and return it
    std::unique_ptr<mysqlx::Session> session = std::move(connectionPool.front());
    connectionPool.pop();
    
    return session;
}
std::string UrlShortenerDB::getConfig(mysqlx::Session& currentSession, std::string key){
    // Removed connection acquisition/return logic
    std::string value = "";

    try
    {
        string sql = "SELECT setting_value FROM global_settings WHERE setting_key = ?";
        // Use the passed-in currentSession
        auto result = executeStatement(currentSession, sql, {Value(key)}); 
        
        if (auto row = result->fetchOne()) {
            value = row[0].get<string>();
        }
    }
    catch(const std::exception& e)
    {
        // Don't log DB_ERROR here since the caller handles the main connection
        value = "invalid request"; 
    }
    // Removed returnConnection(std::move(currentSession));
    return value;
}
void UrlShortenerDB::returnConnection(std::unique_ptr<mysqlx::Session> session) {
    if (session) {
        lock_guard<std::mutex> lock(poolMutex);
        connectionPool.push(std::move(session));
        poolCv.notify_one(); // Notify one waiting thread that a connection is free
    }
}


// --- Helper Functions (MODIFIED) ---

string UrlShortenerDB::getTodayDate() {
    time_t now = time(nullptr);
    struct tm *ltm = localtime(&now); 
    char buffer[11]; // YYYY-MM-DD\0
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", ltm);
    return buffer;
}

// NEW HELPER: Generates a future timestamp for session/link expiration
string UrlShortenerDB::getFutureTimestamp(int days) {
    auto now = std::chrono::system_clock::now();
    auto future = now + std::chrono::minutes(24 * days);
    time_t future_time = std::chrono::system_clock::to_time_t(future);
    
    struct tm *ltm = localtime(&future_time); 
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);
    return buffer;
}

// Helper function to get the current time (YYYY-MM-DD HH:MM:SS)
string UrlShortenerDB::getCurrentTimestamp() {
    time_t now = time(nullptr);
    struct tm *ltm = localtime(&now); 
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);
    return buffer;
}

// Helper method implementation (DECOUPLED FROM SHARED MEMBER)
unique_ptr<RowResult> UrlShortenerDB::executeStatement(
    mysqlx::Session& currentSession, // <-- Session parameter added
    const string& sql, 
    const std::vector<Value>& params
) {
    // Create the statement object using the provided session
    mysqlx::SqlStatement stmt = currentSession.sql(sql);

    // Bind parameters
    for (const auto& param : params) {
        stmt.bind(param);
    }

    // Execute the statement and return the result
    return unique_ptr<RowResult>(new RowResult(stmt.execute()));
}


// --- UrlShortenerDB Implementation ---

UrlShortenerDB::UrlShortenerDB() {
    // Default pool size
}

UrlShortenerDB::~UrlShortenerDB() {
    // All sessions in the pool are closed automatically when unique_ptr is destroyed
}

bool UrlShortenerDB::connect() {
    // Use a temporary session object to establish connections for the pool
    std::unique_ptr<mysqlx::Session> tempSession; 
    cout<<"here1111";
    try {
        // --- 1. INITIALIZE POOL ---
        for (int i = 0; i < poolSize; ++i) {
            tempSession = std::make_unique<mysqlx::Session>(
                Config::DB_HOST, 
                Config::DB_PORT, 
                Config::DB_USER, 
                Config::DB_PASS
            );
            
            // Add the new, fully connected session to the pool
            connectionPool.push(std::move(tempSession));
        }

        // --- 2. CREATE DATABASE (Use a single pool session for DDL) ---
        // We use one connection from the pool for initial DDL (Data Definition Language)
        tempSession = getConnection(); 
        cout<<"here2222";
        tempSession->sql("CREATE DATABASE IF NOT EXISTS " + Config::DB_NAME).execute();
        tempSession->sql("USE " + Config::DB_NAME).execute();
        
        // Return the session to the pool
        returnConnection(std::move(tempSession));
        cout<<"here3333";

        cerr << "DB_INFO: Database connection pool established with " << poolSize << " sessions to " << Config::DB_NAME << endl;

        isConnected = true;
        return true;
    } catch (const mysqlx::Error &e) {
        cerr << "DB_ERROR: MySQL X DevAPI connection failed: " << e.what() << endl;
        // Clean up any partially established connections (automatically handled by destructors)
        isConnected = false;
        return false;
    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to establish pool: " << e.what() << endl;
        isConnected = false;
        return false;
    }
}

// MODIFIED: Uses pool session and correct EXECUTE HELPER
bool UrlShortenerDB::incrementEndpointStat(const std::string& endpoint,
                                           const std::string& method,
                                           const std::string& createdBy) {
    if (!isConnected) return false;
    std::unique_ptr<mysqlx::Session> currentSession;
    try {
        currentSession = getConnection(); 

        string sql = "INSERT INTO endpoint_stats (endpoint, endpoint_type, count, created_by, type) "
            "VALUES (?, ?, 1, ?, 'USER') "
            "ON DUPLICATE KEY UPDATE "
            "count = count + 1, "
            "created_by = VALUES(created_by), "
            "updated_at = CURRENT_TIMESTAMP "
            ";";
        std::vector<Value> params = {
            Value(endpoint),
            Value(method),
            Value(createdBy)
        };
        UrlShortenerDB::executeStatement(*currentSession, sql, params); // Pass session
        returnConnection(std::move(currentSession));
        return true;
    } catch (const std::exception& e) {
        std::cerr << "DB_ERROR: Failed to update endpoint stats: " << e.what() << std::endl;
        returnConnection(std::move(currentSession));
        return false;
    }
}

// MODIFIED: Uses pool session for DDL execution
bool UrlShortenerDB::setupDatabase() {
    if (!isConnected) {
        cerr << "DB_ERROR: Cannot set up database: No active pool." << endl;
        return false;
    }
    
    std::unique_ptr<mysqlx::Session> currentSession;
    
    try {
        currentSession = getConnection();
        cerr << "DB_INFO: Executing table creation and initial settings SQL." << endl;
        
        // 1️⃣ Read the schema file
        std::ifstream file("../schema.sql");
        if (!file.is_open()) {
            cerr << "DB_ERROR: Could not open schema.sql" << endl;
            returnConnection(std::move(currentSession));
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        string sqlContent = buffer.str();

        // 2️⃣ Split by ';' and execute
        std::vector<string> statements;
        string current;
        for (char c : sqlContent) {
            current += c;
            if (c == ';') {
                if (!current.empty()) {
                    statements.push_back(current);
                    current.clear();
                }
            }
        }

        // 3️⃣ Execute each statement
        for (auto& stmt : statements) {
            try {
                // ... (Trimming and cleaning logic remains the same) ...
                
                string trimmed = stmt;
                trimmed.erase(0, trimmed.find_first_not_of(" \n\r\t"));
                trimmed.erase(trimmed.find_last_not_of(" \n\r\t") + 1);

                if (trimmed.empty() || trimmed.rfind("--", 0) == 0) continue;

                size_t comment_pos = trimmed.find("--");
                if (comment_pos != string::npos) {
                    trimmed = trimmed.substr(0, comment_pos);
                    trimmed.erase(trimmed.find_last_not_of(" \n\r\t") + 1);
                }

                std::replace(trimmed.begin(), trimmed.end(), '\n', ' ');
                std::replace(trimmed.begin(), trimmed.end(), '\r', ' ');

                if (trimmed.empty()) continue;
                
                // Execute the SQL directly on the pool session
                currentSession->sql(trimmed).execute(); 

            } catch (const mysqlx::Error &e) {
                cerr << "DB_WARN: Failed to execute SQL statement: " << e.what() << endl;
                cerr << "Statement: " << stmt << endl;
            }
        }

        returnConnection(std::move(currentSession));
        cerr << "DB_INFO: Schema setup completed successfully." << endl;
        return true;

    } catch (const std::exception &e) {
        cerr << "DB_ERROR: Exception while setting up DB: " << e.what() << endl;
        returnConnection(std::move(currentSession));
        return false;
    }
}

// --- User & Session Methods (MODIFIED) ---

bool UrlShortenerDB::createUser(const User& user) {
    if (!isConnected) return false;
    std::unique_ptr<mysqlx::Session> currentSession;
    try {
        currentSession = getConnection();

        string sql = "INSERT INTO users (google_id, email, name, created_at, updated_at) VALUES (?, ?, ?, NOW(), NOW())";
        
        std::vector<Value> params = {
            Value(user.google_id),
            Value(user.email),
            Value(user.name)
        };

        currentSession->sql(sql).bind(params).execute();
        returnConnection(std::move(currentSession));
        return true;
    } catch (const mysqlx::Error& e) {
        cerr << "DB_ERROR: Failed to create user: " << e.what() << endl;
        returnConnection(std::move(currentSession));
        return false;
    }
}

unique_ptr<User> UrlShortenerDB::findUserByGoogleId(const string& google_id) {
    if (!isConnected) return nullptr;
    std::unique_ptr<mysqlx::Session> currentSession;
    unique_ptr<User> user = nullptr;
    
    try {
        currentSession = getConnection();
        string sql = "SELECT id, google_id, email, name, created_at FROM users WHERE google_id = ?";
        auto result = executeStatement(*currentSession, sql, {Value(google_id)}); // Pass session
        
        if (auto row = result->fetchOne()) {
            user = std::make_unique<User>();
            
            user->id = row[0].get<unsigned int>(); 
            user->google_id = row[1].get<string>(); 
            user->email = row[2].get<string>();
            user->name = row[3].get<string>();
            user->created_at = row[4].get<string>(); 
        }

    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to find user by Google ID: " << e.what() << endl;
    }
    returnConnection(std::move(currentSession));
    return user;
}

// NEW FUNCTION: Find user by email (essential for standard OAuth lookup)
unique_ptr<User> UrlShortenerDB::findUserByEmail(const string& email) {
    if (!isConnected) return nullptr;
    std::unique_ptr<mysqlx::Session> currentSession;
    unique_ptr<User> user = nullptr;

    try {
        currentSession = getConnection();
        string sql = "SELECT id, google_id, email, name, created_at FROM users WHERE email = ?";
        auto result = executeStatement(*currentSession, sql, {Value(email)}); // Pass session
        
        if (auto row = result->fetchOne()) {
            user = std::make_unique<User>();
            
            user->id = row[0].get<unsigned int>(); 
            user->google_id = row[1].get<string>(); 
            user->email = row[2].get<string>();
            user->name = row[3].get<string>();
            user->created_at = row[4].get<string>(); 
        }

    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to find user by Email: " << e.what() << endl;
    }
    returnConnection(std::move(currentSession));
    return user;
}

// FIX: Explicitly refers to the DTO struct and avoids ambiguity
bool UrlShortenerDB::createSession(const ::Session& sessionObj) {
    if (!isConnected) return false;
    std::unique_ptr<mysqlx::Session> currentSession;

    try {
        currentSession = getConnection();
        string sql = "INSERT INTO sessions (user_id, session_token, expires_at, created_at, updated_at) VALUES (?, ?, ?, NOW(), NOW())";
        
        std::vector<Value> params = {
            Value(sessionObj.user_id),
            Value(sessionObj.session_token),
            Value(sessionObj.expires_at) // Assuming this is a formatted string (YYYY-MM-DD HH:MM:SS)
        };

        currentSession->sql(sql).bind(params).execute();
        returnConnection(std::move(currentSession));
        return true;
    } catch (const mysqlx::Error& e) {
        cerr << "DB_ERROR: Failed to create session: " << e.what() << endl;
        returnConnection(std::move(currentSession));
        return false;
    }
}
bool UrlShortenerDB::deleteSession(const string& token) {
    if (!isConnected) return false;
    std::unique_ptr<mysqlx::Session> currentSession;
    try {
        currentSession = getConnection();
        string sql = "DELETE FROM sessions WHERE session_token = ?";
        currentSession->sql(sql).bind(Value(token)).execute();
        cerr << "DB_INFO: Session deleted successfully." << endl;
        returnConnection(std::move(currentSession));
        return true;
    } catch (const mysqlx::Error& e) {
        cerr << "DB_ERROR: Failed to delete session: " << e.what() << endl;
        returnConnection(std::move(currentSession));
        return false;
    }
}

unique_ptr<::Session> UrlShortenerDB::findSessionByToken(const string& token) {
    if (!isConnected) return nullptr;
    std::unique_ptr<mysqlx::Session> currentSession;
    unique_ptr<::Session> sessionObj = nullptr;

    try {
        currentSession = getConnection();
        string sql = "SELECT id, user_id, session_token, expires_at FROM sessions WHERE session_token = ? AND expires_at > NOW()";
        auto result = executeStatement(*currentSession, sql, {Value(token)}); // Pass session
        
        if (auto row = result->fetchOne()) {
            sessionObj = std::make_unique<::Session>();
            
            sessionObj->id = row[0].get<unsigned int>(); 
            sessionObj->user_id = row[1].get<unsigned int>(); 
            sessionObj->session_token = row[2].get<string>();
            sessionObj->expires_at = row[3].get<string>();
        }

    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to find session: " << e.what() << endl;
    }
    returnConnection(std::move(currentSession));
    return sessionObj;
}
bool UrlShortenerDB::setLinkFavorite(const int&userId, const string&code, const bool&isFav){
    if (!isConnected) return false;
    std::unique_ptr<mysqlx::Session> currentSession;
    try{
        currentSession = getConnection();
        string sql="UPDATE shortened_links SET is_favourite = ? WHERE user_id = ? AND short_code = ?;";
        std::vector<Value> params = {
            Value(isFav),
            Value(userId),
            Value(code)
        };
        UrlShortenerDB::executeStatement(*currentSession, sql, params); // Pass session
        returnConnection(std::move(currentSession));
        return true;
    }
    catch (const mysqlx::Error &e) {
        cerr<<"ERROR IN CHANGING FAVOURITE VALUE"<<" "<<e.what()<<endl;
        returnConnection(std::move(currentSession));
        return false;
    }
}

bool UrlShortenerDB::deleteLink(const int&id, const string&code){
    if (!isConnected) return false;
    std::unique_ptr<mysqlx::Session> currentSession;
    try{
        currentSession = getConnection();
        string sql="DELETE FROM shortened_links"
                    " WHERE user_id = ?"
                    " AND short_code = ?;";
            std::vector<Value> params = {
            Value(id),
            Value(code),
        };
        UrlShortenerDB::executeStatement(*currentSession, sql, params); // Pass session
        returnConnection(std::move(currentSession));
        return true;
    }
    catch (const mysqlx::Error &e) {
        cerr<<"ERROR IN DELETING LINK";
        returnConnection(std::move(currentSession));
        return false;
    }

}
bool UrlShortenerDB::createLink(const ShortenedLink& link) {
    if (!isConnected) return false;
    std::unique_ptr<mysqlx::Session> currentSession;

    try {
        currentSession = getConnection();

        string sql = "INSERT INTO shortened_links "
                     "(original_url, short_code, user_id, guest_identifier, expires_at, created_at, updated_at) "
                     "VALUES (?, ?, ?, ?, ?, NOW(), NOW())";
        
        // Handle optional user_id (Authenticated Link Creation)
        Value user_id_val = link.user_id ? Value(*link.user_id) : Value(nullptr);
        
        // Calculate expiration date
        auto now = std::chrono::system_clock::now();
        now += std::chrono::hours(24 * Config::LINK_EXPIRED_IN);  
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        
        std::string expires_at = !link.expires_at.empty() ? link.expires_at : std::string(buffer);
        
        // Handle optional expires_at (Link Expiration)
        Value expires_at_val = Value(expires_at);

        std::vector<Value> params = {
            Value(link.original_url),
            Value(link.short_code),
            user_id_val,
            Value(link.guest_identifier),
            expires_at_val
        };

        currentSession->sql(sql).bind(params).execute();
        returnConnection(std::move(currentSession));
        return true;

    } catch (const mysqlx::Error &e) {
        std::string err_msg = e.what();
        std::cerr << "DB_ERROR: " << err_msg << std::endl;

        // Detect duplicate key by checking text in the error message
        if (err_msg.find("Duplicate entry") != std::string::npos) {
            std::cerr << "DB_ERROR: Short code already exists." << std::endl;
        }
        returnConnection(std::move(currentSession));
        return false;
    }
}
// NEW FUNCTION: Implements Link Analytics (Click Tracking)
bool UrlShortenerDB::incrementLinkClicks(unsigned int link_id) {
    if (!isConnected) return false;
    std::unique_ptr<mysqlx::Session> currentSession;
    try {
        currentSession = getConnection();
        string sql = "UPDATE shortened_links SET clicks = clicks + 1, updated_at = NOW() WHERE id = ?";
        currentSession->sql(sql).bind(Value(link_id)).execute();
        returnConnection(std::move(currentSession));
        return true;
    } catch (const mysqlx::Error& e) {
        cerr << "DB_ERROR: Failed to increment clicks: " << e.what() << endl;
        returnConnection(std::move(currentSession));
        return false;
    }
}

unique_ptr<std::vector<ShortenedLink>> UrlShortenerDB::getLinksByUserId(unsigned int user_id) {
    if (!isConnected) return nullptr;
    std::unique_ptr<mysqlx::Session> currentSession;
    unique_ptr<std::vector<ShortenedLink>> links = nullptr;

    try {
        currentSession = getConnection();
        string sql = "SELECT id, original_url, short_code, expires_at, clicks, created_at FROM shortened_links WHERE user_id = ? ORDER BY created_at DESC";
        auto result = executeStatement(*currentSession, sql, {Value(user_id)}); // Pass session
        
        links = std::make_unique<std::vector<ShortenedLink>>();

        for (auto row : *result) {
            ShortenedLink link;
            link.id = row[0].get<unsigned int>();
            link.original_url = row[1].get<string>();
            link.short_code = row[2].get<string>();
            link.expires_at = row[3].get<string>();
            link.clicks = row[4].get<unsigned int>();
            link.created_at = row[5].get<string>();
            links->push_back(std::move(link));
        }

    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to fetch user links: " << e.what() << endl;
    }
    returnConnection(std::move(currentSession));
    return links;
}

unique_ptr<ShortenedLink> UrlShortenerDB::getLinkByShortCode(const string& code) {
    if (!isConnected) return nullptr;
    std::unique_ptr<mysqlx::Session> currentSession;
    unique_ptr<ShortenedLink> link = nullptr;

    try {
        currentSession = getConnection();
        // Check for the code AND ensure it hasn't expired (Link Expiration)
        string sql = "SELECT id, original_url, short_code, user_id, expires_at, clicks FROM shortened_links "
                     "WHERE short_code = ? AND (expires_at IS NULL OR expires_at > NOW())";
        auto result = executeStatement(*currentSession, sql, {Value(code)}); // Pass session
        
        if (auto row = result->fetchOne()) {
            link = std::make_unique<ShortenedLink>();
            
            link->id = row[0].get<unsigned int>(); 
            link->original_url = row[1].get<string>();
            link->short_code = row[2].get<string>();

            // Handle nullable user_id (Authenticated Link Creation)
            if (!row[3].isNull()) {
                link->user_id = std::make_unique<unsigned int>(row[3].get<unsigned int>());
            }
            
            link->expires_at = row[4].get<string>();
            link->clicks = row[5].get<unsigned int>(); // Link Analytics
        }

    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to get link: " << e.what() << endl;
    }
    returnConnection(std::move(currentSession));
    return link;
}

// --- Quota Management ---

bool UrlShortenerDB::isQuotaLimitEnabled() {
    if (!isConnected) return true; // Default to true if DB fails (fail-safe)
    std::unique_ptr<mysqlx::Session> currentSession;
    bool enabled = true;
    
    try {
        currentSession = getConnection();
        string sql = "SELECT setting_value FROM global_settings WHERE setting_key = 'MAX_LINK_LIMIT_ENABLED'";
        auto result = executeStatement(*currentSession, sql, {}); // Pass session
        
        if (auto row = result->fetchOne()) {
            string value = row[0].get<string>();
            enabled = (value == "true");
        }
    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to read global setting: " << e.what() << endl;
    }
    returnConnection(std::move(currentSession));
    return enabled; 
}

std::string UrlShortenerDB::getConfig(std::string key){
    if (!isConnected) throw runtime_error("Database not connected");
    std::unique_ptr<mysqlx::Session> currentSession;
    std::string value = "";

    try
    {
        currentSession = getConnection();
        string sql = "use database ";
        sql+=Config::DB_NAME;
        auto result = executeStatement(*currentSession, sql, {}); // Pass session
        
        sql = "SELECT setting_value FROM global_settings WHERE setting_key = ?";
        result = executeStatement(*currentSession, sql, {Value(key)}); // Pass session
        
        if (auto row = result->fetchOne()) {
            value = row[0].get<string>();
        }
    }
    catch(const std::exception& e)
    {
        cerr<<"DB_ERROR: FAILED TO GET THE CONFIG FOR KEY: "<<key<<" with error"<<e.what()<<endl;
        value = "invalid request"; // Use a specific error value
    }
    returnConnection(std::move(currentSession));
    return value;
} 

bool UrlShortenerDB::checkAndUpdateGuestQuota(const string& guest_identifier, const string& today_date) {
    if (!isConnected) return false;
    std::unique_ptr<mysqlx::Session> currentSession;
    bool success = false;

    try {
        currentSession = getConnection();

        // 1. SELECT current quota
        string select_sql = "SELECT links_created FROM guest_daily_quotas WHERE guest_identifier = ? AND quota_date = ?";
        auto result = executeStatement(*currentSession, select_sql, {Value(guest_identifier), Value(today_date)});
        
        unsigned int links_created_today = 0;
        if (auto row = result->fetchOne()) {
            links_created_today = row[0].get<unsigned int>();
        }
        
        int MAX_GUEST_LINKS_PER_DAY = 5;
        std::string config_val = getConfig(*currentSession, "MAX_GUEST_LINKS_PER_DAY");
        
        if (config_val != "invalid request" && !config_val.empty()) {
            try {
                MAX_GUEST_LINKS_PER_DAY = std::stoi(config_val);
            } catch (const std::exception&) {
                cerr << "DB_CONFIG_ERROR: Failed to convert MAX_GUEST_LINKS_PER_DAY to int." << endl;
            }
        } else {
            cerr<<"DB_CONFIG_ERROR: Unable to find config for key MAX_GUEST_LINKS_PER_DAY, using default 5."<<endl;
        }

        if (links_created_today >= MAX_GUEST_LINKS_PER_DAY ) {
            cerr << "DB_CHECK: Quota limit reached (" << links_created_today << "/" << MAX_GUEST_LINKS_PER_DAY << ") for " << guest_identifier << endl;
            success = false; // Quota exceeded
        } else {
            // 2. INSERT or UPDATE quota (ON DUPLICATE KEY UPDATE)
            string upsert_sql = "INSERT INTO guest_daily_quotas (guest_identifier, quota_date, links_created, created_at, updated_at) "
                                "VALUES (?, ?, 1, NOW(), NOW()) "
                                "ON DUPLICATE KEY UPDATE links_created = links_created + 1, updated_at = NOW()";
            
            currentSession->sql(upsert_sql).bind(Value(guest_identifier)).bind(Value(today_date)).execute();
            success = true; // Quota check passed and updated
        }

    } catch (const mysqlx::Error& e) {
        cerr << "DB_ERROR: Quota check failed: " << e.what() << endl;
        success = false;
    }
    returnConnection(std::move(currentSession));
    return success;
}