/**
 * @file main.cpp
 * @brief Main entry point for the URL Shortener Service application.
 *
 * This file contains the main() function which initializes the application
 * components: the database connection (UrlShortenerDB) and the HTTP server
 * (UrlShortenerServer). It ensures the database is ready before starting
 * the server to handle incoming short URL creation and redirection requests.
 *
 * @author Prashant
 * @date Developed in 2025
 */

#include <iostream>
#include <string>
#include <mutex>
#include "Config.h"     // Configuration constants
#include "URLShortnerDB.h" // Database handler class
#include "Server.h"     // HTTP Server handler class

using namespace std;
using namespace httplib;

// --- Global Data and Database Instance ---

/**
 * @brief Global instance of the UrlShortenerDB class.
 * This object manages the connection and operations with the underlying database.
 * It is defined globally so its reference can be passed to the UrlShortenerServer
 * instance, allowing all server handlers to interact with the database.
 */
UrlShortenerDB db;

/**
 * @brief Global mutex for database synchronization.
 * This mutex is used to protect concurrent access to the 'db' instance
 * from multiple threads that the httplib server uses to handle requests,
 * ensuring thread-safe database operations.
 */
mutex dbMutex;

// --- Main Entry Point ---

/**
 * @brief Main function where the application execution begins.
 * @return 0 on successful execution, 1 on fatal error (e.g., database connection failure).
 */
int main() {
    // Output application status to standard error for logging purposes
    cerr << "RUNNING: Starting URL Shortener Service initialization..." << endl;

    // 1. Initialize Database (Connect and Ensure Schema exists)
    // Attempt to connect to the configured MySQL server and verify/setup the necessary tables.
    if (!db.connect() || !db.setupDatabase()) {
        cerr << "FATAL: Database initialization failed. Please check credentials and MySQL server status. Exiting." << endl;
        return 1;
    }

    // 2. Initialize the Server Application
    // The UrlShortenerServer class encapsulates all routes, middleware, and handlers.
    // It is constructed with references to the database instance and its protective mutex.
    UrlShortenerServer app(db, dbMutex);

    // 3. Run the Server
    // Start listening on the configured host and port.
    cerr << "Listening on http://0.0.0.0:9080" << endl;
    if (!app.run()) {
        cerr << "FATAL: Server failed to start or shut down unexpectedly." << endl;
        return 1;
    }

    // Server successfully shut down (e.g., through an explicit stop signal or clean exit)
    return 0;
}
