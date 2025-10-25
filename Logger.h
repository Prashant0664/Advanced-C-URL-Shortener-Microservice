// Logger.h file

// --- Logging Middleware Function ---

#include <httplib.h>
// using namespace httplib;
using namespace std;

class SaveLogs {
public:
    // FIX: Changed return type from 'bool' to 'httplib::Server::HandlerResponse'.
    bool log_request(const httplib::Request &req, Response &res);
    
};