#include <iostream>
#include <string>
#include <mutex>
#include "Config.h"
#include "URLShortnerDB.h"
#include "Server.h"

using namespace std;
using namespace httplib;

// --- Global Data and Database Instance ---
// Note: These must be outside main() for the Server class methods to access them by reference.
UrlShortenerDB db; 
mutex dbMutex; // Global mutex to lock database access across threads


// --- Main Entry Point ---
int main() {
    cout << "RUNNING: Starting URL Shortener Service initialization..." << endl;
    
    // 1. Initialize Database (Connect and Ensure Schema exists)
    // The db object handles connection and setup logic defined in UrlShortenerDB.cpp
    if (!db.connect() || !db.setupDatabase()) {
        cerr << "FATAL: Database initialization failed. Please check credentials and MySQL server status. Exiting." << endl;
        return 1;
    }
    
    // 2. Initialize the Server Application
    // The UrlShortenerServer class encapsulates all routes, middleware, and handlers.
    UrlShortenerServer app(db, dbMutex);
    
    // 3. Run the Server
    cout << "Listening on http://0.0.0.0:9080" << endl;
    if (!app.run()) {
        cerr << "FATAL: Server failed to start or shut down unexpectedly." << endl;
        return 1;
    }

    return 0;
}
