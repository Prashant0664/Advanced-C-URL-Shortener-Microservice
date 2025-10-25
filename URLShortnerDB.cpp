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


// Include necessary standard namespace functions explicitly to resolve ambiguities
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::unique_ptr;
using std::runtime_error;

// Use namespace aliases for MySQL types to improve readability inside the functions
// namespace mysqlx = ::mysqlx;
using mysqlx::abi2::Value;
using mysqlx::abi2::RowResult;

// --- Helper Functions ---

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

// Helper method implementation (declared in UrlShortenerDB.h)
unique_ptr<RowResult> UrlShortenerDB::executeStatement(
    const string& sql, 
    const std::vector<Value>& params
) {
    if (!session) {
        throw runtime_error("Database session is not connected.");
    }
    
    // Create the statement object
    mysqlx::SqlStatement stmt = session->sql(sql);

    // Bind parameters
    for (const auto& param : params) {
        stmt.bind(param);
    }

    // Execute the statement and return the result
    return unique_ptr<RowResult>(new RowResult(stmt.execute()));
}


// --- UrlShortenerDB Implementation ---

UrlShortenerDB::UrlShortenerDB() {
    // Constructor logic for the DB Manager is minimal
}

UrlShortenerDB::~UrlShortenerDB() {
    // Session is closed automatically when unique_ptr is destroyed
}

bool UrlShortenerDB::connect() {
    try {
        // session.reset(new mysqlx::Session(Config::DB_HOST, Config::DB_PORT, Config::DB_USER, Config::DB_PASS));
        session.reset(new mysqlx::Session(Config::DB_HOST, Config::DB_PORT, Config::DB_USER, Config::DB_PASS));

        // Create database if it doesn't exist
        session->sql("CREATE DATABASE IF NOT EXISTS " + Config::DB_NAME).execute();

        // Select the database explicitly
        session->sql("USE " + Config::DB_NAME).execute();

        schema = session->getSchema(Config::DB_NAME, true);
        cerr << "DB_INFO: Database connection successful to " << Config::DB_NAME << endl;

        isConnected = true;
        return true;

    } catch (const mysqlx::Error &e) {
        cerr << "DB_ERROR: MySQL X DevAPI connection failed: " << e.what() << endl;
        session = nullptr;
        isConnected = false;
        return false;
    }
}

bool UrlShortenerDB::incrementEndpointStat(const std::string& endpoint,
                                           const std::string& method,
                                           const std::string& createdBy) {
    try {
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
        UrlShortenerDB::executeStatement(sql, params);
        return true;
    } catch (const mysqlx::Error& e) {
        std::cerr << "DB_ERROR: Failed to update endpoint stats: " << e.what() << std::endl;
        return false;
    }
}


bool UrlShortenerDB::setupDatabase() {
    if (!isConnected || !schema.has_value()) {
        cerr << "DB_ERROR: Cannot set up database: No active connection/schema." << endl;
        return false;
    }
    
    // In a real app, you would execute the table creation SQL here
    cout << "DB_INFO: Executing table creation and initial settings SQL (SIMULATED)." << endl;
     try {
        // 1️⃣ Read the schema file
        std::ifstream file("../schema.sql");
        if (!file.is_open()) {
            cerr << "DB_ERROR: Could not open schema.sql" << endl;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        string sqlContent = buffer.str();

        // 2️⃣ Split by ';' to handle multiple statements
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
        // Trim leading and trailing whitespace
        string trimmed = stmt;
        trimmed.erase(0, trimmed.find_first_not_of(" \n\r\t"));
        trimmed.erase(trimmed.find_last_not_of(" \n\r\t") + 1);

        // Skip empty lines or comment-only lines
        if (trimmed.empty() || trimmed.rfind("--", 0) == 0)
            continue;

        // Remove any inline comments starting with `--`
        size_t comment_pos = trimmed.find("--");
        if (comment_pos != string::npos) {
            trimmed = trimmed.substr(0, comment_pos);
            trimmed.erase(trimmed.find_last_not_of(" \n\r\t") + 1);
        }

        // Because actual SQL is in one line, ensure we have one continuous statement
        // (e.g., remove newlines or carriage returns that may split it)
        std::replace(trimmed.begin(), trimmed.end(), '\n', ' ');
        std::replace(trimmed.begin(), trimmed.end(), '\r', ' ');

        if (trimmed.empty())
            continue;

        cout << "Executing SQL: " << trimmed << endl;

        // Execute the SQL directly
        session->sql(trimmed).execute();

    } catch (const mysqlx::Error &e) {
        cerr << "DB_WARN: Failed to execute SQL statement: " << e.what() << endl;
        cerr << "Statement: " << stmt << endl;
    }
}



        cerr << "DB_INFO: Schema setup completed successfully." << endl;
        return true;

    } catch (const std::exception &e) {
        cerr << "DB_ERROR: Exception while setting up DB: " << e.what() << endl;
        return false;
    }
    return true;
}

// --- User & Session Methods ---

bool UrlShortenerDB::createUser(const User& user) {
    if (!isConnected) return false;
    
    try {
        // NOTE: google_id is optional here, we assume if we reach this point,
        // we have either google_id or email and name.
        string sql = "INSERT INTO users (google_id, email, name, created_at, updated_at) VALUES (?, ?, ?, NOW(), NOW())";
        
        std::vector<Value> params = {
            Value(user.google_id),
            Value(user.email),
            Value(user.name)
        };

        session->sql(sql).bind(params).execute();

        return true;
    } catch (const mysqlx::Error& e) {
        cerr << "DB_ERROR: Failed to create user: " << e.what() << endl;
        return false;
    }
}

unique_ptr<User> UrlShortenerDB::findUserByGoogleId(const string& google_id) {
    if (!isConnected) return nullptr;
    
    try {
        string sql = "SELECT id, google_id, email, name, created_at FROM users WHERE google_id = ?";
        auto result = executeStatement(sql, {Value(google_id)});
        
        if (auto row = result->fetchOne()) {
            auto user = std::make_unique<User>();
            
            user->id = row[0].get<unsigned int>(); 
            user->google_id = row[1].get<string>(); 
            user->email = row[2].get<string>();
            user->name = row[3].get<string>();
            user->created_at = row[4].get<string>(); 
            
            return user;
        }

    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to find user by Google ID: " << e.what() << endl;
    }
    return nullptr;
}

// NEW FUNCTION: Find user by email (essential for standard OAuth lookup)
unique_ptr<User> UrlShortenerDB::findUserByEmail(const string& email) {
    if (!isConnected) return nullptr;
    
    try {
        string sql = "SELECT id, google_id, email, name, created_at FROM users WHERE email = ?";
        auto result = executeStatement(sql, {Value(email)});
        
        if (auto row = result->fetchOne()) {
            auto user = std::make_unique<User>();
            
            user->id = row[0].get<unsigned int>(); 
            user->google_id = row[1].get<string>(); 
            user->email = row[2].get<string>();
            user->name = row[3].get<string>();
            user->created_at = row[4].get<string>(); 
            
            return user;
        }

    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to find user by Email: " << e.what() << endl;
    }
    return nullptr;
}
// FIX: Explicitly refers to the DTO struct and avoids ambiguity
bool UrlShortenerDB::createSession(const ::Session& sessionObj) {
    if (!isConnected) return false;
    
    try {
        string sql = "INSERT INTO sessions (user_id, session_token, expires_at, created_at, updated_at) VALUES (?, ?, ?, NOW(), NOW())";
        
        std::vector<Value> params = {
            Value(sessionObj.user_id),
            Value(sessionObj.session_token),
            Value(sessionObj.expires_at) // Assuming this is a formatted string (YYYY-MM-DD HH:MM:SS)
        };

        session->sql(sql).bind(params).execute();
        return true;
    } catch (const mysqlx::Error& e) {
        cerr << "DB_ERROR: Failed to create session: " << e.what() << endl;
        return false;
    }
}
bool UrlShortenerDB::deleteSession(const string& token) {
    if (!isConnected) return false;
    try {
        string sql = "DELETE FROM sessions WHERE session_token = ?";
        session->sql(sql).bind(Value(token)).execute();
        cout << "DB_INFO: Session deleted successfully." << endl;
        return true;
    } catch (const mysqlx::Error& e) {
        cerr << "DB_ERROR: Failed to delete session: " << e.what() << endl;
        return false;
    }
}

unique_ptr<::Session> UrlShortenerDB::findSessionByToken(const string& token) {
    if (!isConnected) return nullptr;

    try {
        string sql = "SELECT id, user_id, session_token, expires_at FROM sessions WHERE session_token = ? AND expires_at > NOW()";
        auto result = executeStatement(sql, {Value(token)});
        
        if (auto row = result->fetchOne()) {
            auto sessionObj = std::make_unique<::Session>();
            
            sessionObj->id = row[0].get<unsigned int>(); 
            sessionObj->user_id = row[1].get<unsigned int>(); 
            sessionObj->session_token = row[2].get<string>();
            sessionObj->expires_at = row[3].get<string>();
            
            return sessionObj;
        }

    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to find session: " << e.what() << endl;
    }
    return nullptr;
}
bool UrlShortenerDB::setLinkFavorite(const int&userId, const string&code, const bool&isFav){
    if (!isConnected) return false;
    try{
        string sql="UPDATE shortened_links SET is_favourite = ? WHERE user_id = ? AND short_code = ?;";
        std::vector<Value> params = {
            Value(isFav),
            Value(userId),
            Value(code)
        };
        UrlShortenerDB::executeStatement(sql, params);
        return 1;
    }
    catch (const mysqlx::Error &e) {
        cerr<<"ERROR IN CHANGING FAVOURITE VALUE"<<" "<<e.what()<<endl;
        return 0;
    }
}

bool UrlShortenerDB::deleteLink(const int&id, const string&code){
    if (!isConnected) return false;
    try{
        string sql="DELETE FROM shortened_links"
                    " WHERE user_id = ?"
                    " AND short_code = ?;";
            std::vector<Value> params = {
            Value(id),
            Value(code),
        };
        UrlShortenerDB::executeStatement(sql, params);
        return 1;
    }
    catch (const mysqlx::Error &e) {
        cerr<<"ERROR IN DELETING LINK";
        return 0;
    }

}
bool UrlShortenerDB::createLink(const ShortenedLink& link) {
    if (!isConnected) return false;
    
    try {
        string sql = "INSERT INTO shortened_links "
                     "(original_url, short_code, user_id, guest_identifier, expires_at, created_at, updated_at) "
                     "VALUES (?, ?, ?, ?, ?, NOW(), NOW())";
        // Handle optional user_id (Authenticated Link Creation)
        Value user_id_val = link.user_id ? Value(*link.user_id) : Value(nullptr);
        auto now = std::chrono::system_clock::now();
        now += std::chrono::hours(24 * Config::LINK_EXPIRED_IN);  // add days
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
        std::string expires_at = !link.expires_at.empty() ? link.expires_at : ss.str();

    // Handle optional expires_at (Link Expiration)
        Value expires_at_val = Value(expires_at);

        std::vector<Value> params = {
            Value(link.original_url),
            Value(link.short_code),
            user_id_val,
            Value(link.guest_identifier),
            expires_at_val
        };

        session->sql(sql).bind(params).execute();
        return true;

    } catch (const mysqlx::Error &e) {
        std::string err_msg = e.what();
        std::cerr << "DB_ERROR: " << err_msg << std::endl;

        // Detect duplicate key by checking text in the error message
        if (err_msg.find("Duplicate entry") != std::string::npos) {
            std::cerr << "DB_ERROR: Short code already exists." << std::endl;
        }

        return false;
    }


}
// NEW FUNCTION: Implements Link Analytics (Click Tracking)
bool UrlShortenerDB::incrementLinkClicks(unsigned int link_id) {
    if (!isConnected) return false;
    try {
        string sql = "UPDATE shortened_links SET clicks = clicks + 1, updated_at = NOW() WHERE id = ?";
        session->sql(sql).bind(Value(link_id)).execute();
        return true;
    } catch (const mysqlx::Error& e) {
        cerr << "DB_ERROR: Failed to increment clicks: " << e.what() << endl;
        return false;
    }
}

unique_ptr<std::vector<ShortenedLink>> UrlShortenerDB::getLinksByUserId(unsigned int user_id) {
    if (!isConnected) return nullptr;

    try {
        string sql = "SELECT id, original_url, short_code, expires_at, clicks, created_at FROM shortened_links WHERE user_id = ? ORDER BY created_at DESC";
        auto result = executeStatement(sql, {Value(user_id)});
        
        auto links = std::make_unique<std::vector<ShortenedLink>>();

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
        return links;

    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to fetch user links: " << e.what() << endl;
    }
    return nullptr;
}

unique_ptr<ShortenedLink> UrlShortenerDB::getLinkByShortCode(const string& code) {
    if (!isConnected) return nullptr;

    try {
        // Check for the code AND ensure it hasn't expired (Link Expiration)
        string sql = "SELECT id, original_url, short_code, user_id, expires_at, clicks FROM shortened_links "
                     "WHERE short_code = ? AND (expires_at IS NULL OR expires_at > NOW())";
        auto result = executeStatement(sql, {Value(code)});
        
        if (auto row = result->fetchOne()) {
            auto link = std::make_unique<ShortenedLink>();
            
            link->id = row[0].get<unsigned int>(); 
            link->original_url = row[1].get<string>();
            link->short_code = row[2].get<string>();

            // Handle nullable user_id (Authenticated Link Creation)
            if (!row[3].isNull()) {
                link->user_id = std::make_unique<unsigned int>(row[3].get<unsigned int>());
            }
            
            link->expires_at = row[4].get<string>();
            link->clicks = row[5].get<unsigned int>(); // Link Analytics
            
            return link;
        }

    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to get link: " << e.what() << endl;
    }
    return nullptr;
}

// --- Quota Management ---

bool UrlShortenerDB::isQuotaLimitEnabled() {
    if (!isConnected) return true; // Default to true if DB fails (fail-safe)
    
    try {
        string sql = "SELECT setting_value FROM global_settings WHERE setting_key = 'MAX_LINK_LIMIT_ENABLED'";
        auto result = executeStatement(sql, {});
        
        if (auto row = result->fetchOne()) {
            string value = row[0].get<string>();
            return value == "true";
        }
    } catch (const std::exception& e) {
        cerr << "DB_ERROR: Failed to read global setting: " << e.what() << endl;
    }
    return true; // Default to limited if DB fails
}

std::string UrlShortenerDB::getConfig(std::string key){
    if (!isConnected) throw -1;
    try
    {
        string sql = "SELECT setting_value FROM global_settings WHERE setting_key = ?";
        auto result = executeStatement(sql, {Value(key)});
        if (auto row = result->fetchOne()) {
            string value = row[0].get<string>();
            return value;
        }
        return nullptr;
    }
    catch(const std::exception& e)
    {
        cerr<<"DB_ERROR: FAILED TO GET THE CONFOG FOR KEY: "<<key<<" with error"<<e.what()<<endl;
        return "invalid request";
    }
    
} 

bool UrlShortenerDB::checkAndUpdateGuestQuota(const string& guest_identifier, const string& today_date) {
    if (!isConnected) return false;

    try {
        // 1. SELECT current quota
        string select_sql = "SELECT links_created FROM guest_daily_quotas WHERE guest_identifier = ? AND quota_date = ?";
        auto result = executeStatement(select_sql, {Value(guest_identifier), Value(today_date)});
        
        unsigned int links_created_today = 0;
        if (auto row = result->fetchOne()) {
            links_created_today = row[0].get<unsigned int>();
        }
        int MAX_GUEST_LINKS_PER_DAY = 5;
        std::string config=getConfig("MAX_GUEST_LINKS_PER_DAY");
        if(!config.empty()){
            MAX_GUEST_LINKS_PER_DAY = std::stoi(config);
        }
        else{
            cerr<<"DB_CONFIG_ERROR: Unable to find config for key MAX_GUEST_LINKS_PER_DAY"<<endl;
        }
        if (links_created_today >=MAX_GUEST_LINKS_PER_DAY ) {
            cout << "DB_CHECK: Quota limit reached (" << links_created_today << "/" << MAX_GUEST_LINKS_PER_DAY << ") for " << guest_identifier << endl;
            return false; // Quota exceeded
        }

        // 2. INSERT or UPDATE quota (ON DUPLICATE KEY UPDATE)
        string upsert_sql = "INSERT INTO guest_daily_quotas (guest_identifier, quota_date, links_created, created_at, updated_at) "
                            "VALUES (?, ?, 1, NOW(), NOW()) "
                            "ON DUPLICATE KEY UPDATE links_created = links_created + 1, updated_at = NOW()";
        
        session->sql(upsert_sql).bind(Value(guest_identifier)).bind(Value(today_date)).execute();

        return true; // Quota check passed and updated
    } catch (const mysqlx::Error& e) {
        cerr << "DB_ERROR: Quota check failed: " << e.what() << endl;
        return false;
    }
}