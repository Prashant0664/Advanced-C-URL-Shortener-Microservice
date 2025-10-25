🔗 C++ URL Shortener Service (Microservice)

A high-performance, multi-threaded URL shortening microservice built in modern C++, leveraging the httplib library for networking and the MySQL X DevAPI for persistent storage. It features full Google OAuth 2.0 integration for user authentication and built-in rate limiting.

✨ Features

High Performance: Built with multi-threaded C++ and utilizing MySQL for fast lookups.

Google OAuth 2.0: Secure user authentication via Google Sign-In, enabling personalized link management.

Token-Based Security: Uses Bearer Tokens for stateless API access (AuthMiddleware).

CSRF Protection: Implements state validation during the OAuth flow.

Rate Limiting: Protects the /shorten endpoint using a Token Bucket Algorithm for guests and authenticated users.

Session Management: Sessions are stored in the database and have a short 5-minute expiration time (configurable) for rapid security testing.

Link Management: Authenticated users can create custom short codes, view analytics (click counts), and manage/delete their links.

Admin Endpoint: Includes a restricted /api/admin route with role-based access control.

Endpoint Statistics: Tracks usage and traffic for all primary server routes.

🛠️ Prerequisites

Before compiling and running the service, ensure you have the following installed:

C++ Compiler: GCC/G++ or Clang (supporting C++17 or later).

httplib Library: The C++ header-only HTTP/HTTPS library.

Crucial: This project uses httplib::SSLClient for communication with Google's API (https). You must have the OpenSSL development libraries installed and linked to your build.

MySQL Server: A running MySQL server instance (v5.7+ or v8.0+).

MySQL X DevAPI: The C++ connector library for MySQL (required for the mysqlx includes).

🚀 Setup and Installation

1. Database Schema Setup

You need to create the database and necessary tables (users, sessions, shortened_links, etc.).

The application attempts to run SQL from a file named schema.sql during startup. Please ensure you execute your table creation script manually or place it in the correct path.

The essential tables required are:

users (stores google_id, email, name, etc.)

sessions (stores user_id, session_token, expires_at)

shortened_links (stores original_url, short_code, clicks, etc.)

endpoint_stats

guest_daily_quotas

global_settings

2. Configure Credentials (Config.h and Config.cpp)

You must define the following constants in your Config.h and provide their values in Config.cpp.

Constant

Type

Description

DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT

std::string/int

MySQL connection details.

BASE_URL

std::string

The base URL of your running service (e.g., http://localhost:9080). Used for redirects.

GOOGLE_CLIENT_ID

std::string

Your Google OAuth Client ID.

GOOGLE_CLIENT_SECRET

std::string

Your Google OAuth Client Secret.

GOOGLE_REDIRECT_URI

std::string

Must exactly match the URL registered in Google Cloud Console: http://localhost:9080/auth/google/callback

3. Build and Run

Compile your source files (main.cpp, Server.cpp, UrlShortenerDB.cpp, etc.) while linking to the httplib header and the required MySQL and OpenSSL libraries.

# Example compilation command (adjusting paths/libraries as necessary)
g++ -std=c++17 main.cpp Server.cpp UrlShortenerDB.cpp -o url_shortener \
    -lmysqlx -lssl -lcrypto -lpthread
# Run the server
./url_shortener


The server will listen on http://0.0.0.0:9080.

🧪 API Usage and Testing (cURL)

The service provides public endpoints and authenticated endpoints protected by the Bearer Token system.

A. Authentication Flow

Authentication is required for all management APIs (/api/*).

1. Start Google Sign-In:
(Copy the Location header URL and paste it into your browser to complete the Google sign-in.)

curl -i -X GET http://localhost:9080/auth/google


2. Retrieve Session Token:
After successful sign-in, your browser will be redirected to the /auth/success page, which displays your unique Session Token. Copy this token for all subsequent API calls.

B. Public Endpoints (No Token Required)

Endpoint

Method

Purpose

cURL Command

/shorten

POST

Create a new, non-authenticated link.

curl -i -X POST http://localhost:9080/shorten -d '{"long_url": "https://public.site"}'

/<code_pattern>

GET

Redirect to the original URL.

curl -i -X GET http://localhost:9080/cfvE0n4n

C. Authenticated Endpoints (Token Required)

Use the token you obtained from the /auth/success page in the Authorization: Bearer [TOKEN] header.

Endpoint

Method

Purpose

cURL Command (Requires Token)

/api/links

GET

View all links owned by the authenticated user.

curl -i -X GET http://localhost:9080/api/links -H "Authorization: Bearer [TOKEN]"

/shorten

POST

Create an authenticated link with a custom code.

curl -i -X POST 'http://localhost:9080/shorten?custom_code=testlink1' \n     -H "Authorization: Bearer [TOKEN]" \n     -H "Content-Type: application/json" \n     -d '{"long_url": "https://private.site/123"}'

/api/link/favorite

POST

Set a link's favorite status.

curl -i -X POST http://localhost:9080/api/link/favorite \n     -H "Authorization: Bearer [TOKEN]" \n     -H "Content-Type: application/json" \n     -d '{"short_code": "testlink1", "is_favorite": true}'

/api/link

DELETE

Delete a specific link.

curl -i -X DELETE 'http://localhost:9080/api/link?code=testlink1' \n     -H "Authorization: Bearer [TOKEN]" 

/api/admin

GET

Admin-only access check (User ID 1 is currently hardcoded as Admin).

curl -i -X GET http://localhost:9080/api/admin -H "Authorization: Bearer [TOKEN]"

🔒 Security Notes

Session Expiration: For testing, sessions are set to expire after 5 minutes. After this time, all authenticated API calls will receive a 401 Unauthorized response, forcing the user to re-authenticate via /auth/google.

CSRF Protection: The server utilizes a state parameter check in handleGoogleCallback to prevent Cross-Site Request Forgery attacks during the OAuth handshake.