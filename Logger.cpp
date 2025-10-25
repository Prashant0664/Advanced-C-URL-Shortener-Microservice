#include "Logger.h"
#include <sentry.h>
#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <iomanip>

using namespace httplib;
using namespace std;

// --- CORS Configuration (Repeated here for clarity/dependency) ---
const string CORS_HEADER_KEY = "Access-Control-Allow-Origin";
const string CORS_HEADER_VALUE = "*";

bool SaveLogs::log_request(const Request &req, Response &res) {
    // 1. Prepare Request Details
    string payload_summary;
    string full_payload = (req.method == "POST" || req.method == "PUT") ? req.body : "[N/A]";
    
    if (full_payload != "[N/A]") {
        const size_t max_len = 100;
        payload_summary = (full_payload.size() < max_len) ? full_payload : full_payload.substr(0, max_len) + "...";
    } else {
        payload_summary = full_payload;
    }

    // 2. Add CORS headers to response (required for React frontend)
    res.set_header(CORS_HEADER_KEY, CORS_HEADER_VALUE);
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");

    // 3. Log to Sentry (Structured Event)
    {
        // Create a new Sentry Event
        sentry_value_t event = sentry_value_new_message_event(
            SENTRY_LEVEL_INFO, 
            "http.request", 
            ("Incoming Request: " + req.method + " " + req.path).c_str()
        );

        // --- Add Required Contextual Data as Tags (Searchable in Sentry UI) ---
        // Sentry automatically handles date/time.
        sentry_set_tag("app.file", "Logger.cpp");
        sentry_set_tag("app.class", "SaveLogs");
        sentry_set_tag("app.function", "log_request");
        sentry_set_tag("http.ip", req.remote_addr.c_str());
        sentry_set_tag("http.method", req.method.c_str());
        sentry_set_tag("http.path", req.path.c_str());

        // --- Add Payload as Extra Context ---
        sentry_set_extra("request.payload_summary", sentry_value_new_string(payload_summary.c_str()));
        sentry_set_extra("request.full_payload", sentry_value_new_string(full_payload.c_str()));


        // Capture the event
        sentry_capture_event(event);
    }
    
    // 4. Log to console (Local output)
    time_t now = time(nullptr);
    tm *ltm = localtime(&now); 
    stringstream time_ss;
    if (ltm) {
        time_ss << put_time(ltm, "%Y-%m-%d %H:%M:%S");
    } else {
        time_ss << "TIME_ERROR";
    }

    // Log format requested: <date and time> <FileName> <class Name> <function name> <...other details> <Log message/stacktrace/etc.>
    cerr << time_ss.str() << " | Logger.cpp | SaveLogs | log_request | " 
         << "IP: " << req.remote_addr << ", Method: " << req.method << ", Path: " << req.path << " | "
         << "Payload: " << payload_summary << endl;

    return true;
}
