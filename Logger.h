// Logger.h file
// --- Logging Middleware Function ---

#include <httplib.h>

using namespace std;

class SaveLogs {
public:
    bool log_request(const httplib::Request &req, Response &res);
    
};