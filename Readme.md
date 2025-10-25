# C++ URL Shortener Service (Microservice)

<hr/>

## NOTE

- You do not need to setup docker, not even vcpkg to run this code. 
- I used docker to deploy on internet
- Used vcpkg(CMakeLists.txt) so that I do not have to install everything globally(just like virtual env in python, node-modules in web dev).
- Go to [Setup](#url-shortener-service-setupc-with-vcpkg) Section to setup this locally and run easily in few steps.

### For any doubt/query/setup help create a PR/raise issue or contact me. I will try to reply ASAP.

<hr/>

## Table of Contents

- [Intro](#intro)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [SETUP](#url-shortener-service-setupc-with-vcpkg)
- [Build and Run](#2-build-and-run-the-service)
- [Complete One-Line Command](#-complete-one-line-build--run-command)
- [API Usage and Testing](#api-usage-and-testing)
- [Security Notes](#security-notes)

## INTRO:
A high-performance, multi-threaded URL shortening microservice built in modern C++, using the httplib library for networking and the MySQL X DevAPI for persistent storage. It features full Google OAuth 2.0 integration for user authentication and built-in rate limiting.

## Features 
<p align="center" style="font-size:20px;">✨😎✨</p>

🔥 High Performance: Built with multi-threaded C++ and utilizing MySQL for fast lookups.<br/>
🔥 Google OAuth 2.0: Secure user authentication via Google Sign-In, enabling personalized link management.<br/>
🔥 Token-Based Security: Uses Bearer Tokens for stateless API access (AuthMiddleware).<br/>
🔥 CSRF Protection: Implements state validation during the OAuth flow.<br/>
🔥 Rate Limiting: Protects the /shorten endpoint using a <u>Token Bucket Algorithm</u> for guests and authenticated users.<br/>
🔥 Session Management: Sessions are stored in the database and have a short 5-minute expiration time (configurable) for rapid security testing.<br/>
🔥 Link Management: Authenticated users can create custom short codes, mark urls favourites, view analytics (click counts), and manage/delete their links.<br/>
🔥 Admin Endpoint: Includes a restricted /api/admin route with role-based access control.<br/>
🔥 Endpoint Statistics: Tracks usage and traffic for all primary server routes.<br/>
🔥 There are many others features like logging code(not integrated though), etc.<br/>

<hr/>

## Prerequisites
<p align="center" style="font-size:20px;">🐱🙀</p>

Before compiling and running the service, ensure you have the following installed:

- C++ Compiler: GCC/G++ or Clang (supporting C++17 or later).
- Some SDK or tool like VS-Studio Code
Crucial: This project uses httplib::SSLClient for communication with Google's API (https). You must have the OpenSSL development libraries installed and linked to your build.
- MySQL Server: A running MySQL server. Also preferable to some workbench like DBeaver or MySQL Workbench.


## URL Shortener Service SETUP(C++ with vcpkg)

This guide provides instructions for configuring, building, and running the `url_shortner` executable after cloning the repository. The project uses CMake and relies on **vcpkg** to manage external dependencies (MySQL Connector, httplib, Sentry).

---

## 1. Prerequisites and Dependency Setup

Before compiling, ensure you have **vcpkg** set up locally.

### A. Install vcpkg

1. **Clone vcpkg**  

   If you don't already have vcpkg installed, clone it into a folder adjacent to your project (or in your home directory):

   ```bash
   git clone https://github.com/microsoft/vcpkg.git
2. **Run Bootstrap**

   Builds the necessary executables for vcpkg:

   ```bash
   ./vcpkg/bootstrap-vcpkg.sh
   ```

3. **Install Project Dependencies**

   Tell vcpkg to install the required libraries specified in your `CMakeLists.txt`. This may take some time as libraries are compiled from source:

   ```bash
   ./vcpkg/vcpkg install mysql-connector-cpp sentry-native cpp-httplib
   ```

---

### B. Configure Environment Variables (`.env` file)

The application loads sensitive information (database credentials, Google secrets) from environment variables at runtime.

1. Create a file named `.env` in the root directory of the cloned project.
2. You can also configure credentials in Config.cpp if not in .env.
3. Populate it with your local MySQL credentials and OAuth configuration:

```env
# Database Credentials
DB_HOST=127.0.0.1 <IT CAN VARY, CHECK YOUR MYSQL SETUP>
DB_USER=root <IT CAN VARY, CHECK YOUR MYSQL SETUP>
DB_PASS=<YOUR PASSWORD>
DB_NAME=test_url
DB_PORT=33060 <IT CAN VARY, CHECK YOUR MYSQL SETUP>

# Application Configuration
BASE_URL=http://localhost:9080/

# Google OAuth Credentials
GOOGLE_CLIENT_ID=your_client_id_here
GOOGLE_CLIENT_SECRET=your_client_secret_here
GOOGLE_REDIRECT_URI=http://localhost:9080/auth/google/callback <IT CAN VARY, CHECK YOUR GOOGLE CONSOLE SETUP>
```

---

## 2. Build and Run the Service

Use the standard **CMake workflow**, passing the Vcpkg toolchain file to ensure dependencies are found.
- Run `rm -rf build` build to delete any existing build file.

| Step                         | Command                                                                     | Purpose                                                      |
| ---------------------------- | --------------------------------------------------------------------------- | ------------------------------------------------------------ |
| **A. Clean Build Directory** | `mkdir build && cd build`                                                   | Creates an out-of-source build directory                     |
| **B. Configure CMake**       | `cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake` | Configures the project and links Vcpkg libraries             |
| **C. Compile Application**   | `cmake --build . -- -j`                                                     | Compiles the project, creating the `url_shortner` executable |
| **D. Load Environment**      | `set -a && source ../.env && set +a`                                        | Exports variables from `.env` to the current shell           |
| **E. Execute Service**       | `./url_shortner`                                                            | Starts the C++ server application                            |

---

### ✅ Complete One-Line Build & Run Command

From the project root directory, you can run everything in a single command(from root folder):

```bash
mkdir build && cd build && \
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake && \
cmake --build . -- -j && \
set -a && source ../.env && set +a && \
./url_shortner
```

try below if above did not work:
```bash
mkdir build && cd build && \
cmake .. && \
cmake --build . -- -j && \
set -a && source ../.env && set +a && \
./url_shortner
```

After running, the server should start and print something like below:

```
Listening on http://0.0.0.0:9080
```

---

**Note:**

* Ensure MySQL is running locally with the credentials specified in `.env`.
* Modify the `GOOGLE_CLIENT_ID` and `GOOGLE_CLIENT_SECRET` with your own OAuth credentials.


---


## API Usage and Testing

The service exposes both **public** and **authenticated** REST endpoints.  
Authenticated routes use a **Bearer Token** (retrieved via Google OAuth login).

---

### 🧭 A. Authentication Flow

All management APIs (`/api/*`) require authentication.

#### 1. Start Google Sign-In

Trigger Google OAuth login. Copy the `Location` header URL and paste it into your browser to complete sign-in.

```bash
curl -i -X GET http://localhost:9080/auth/google
````

#### 2. Retrieve Session Token

After successful login, the browser redirects to:

```
http://localhost:9080/auth/success?token=<SESSION_TOKEN>&user=<USER_NAME>
```

OR
paste this in browser<br/>

```
http://localhost:9080/auth/google
```

The `/auth/success` page displays your **Session Token**.
Copy this token and use it as a **Bearer Token** for authenticated requests.

---

### 🌐 B. Public Endpoints (No Token Required)

| Endpoint        | Method   | Description                     | Example cURL                                                                                   |
| --------------- | -------- | ------------------------------- | ---------------------------------------------------------------------------------------------- |
| `/shorten`      | **POST** | Create a new public short link. | `curl -i -X POST http://localhost:9080/shorten -d '{"long_url": "https://public.site"}' ` |
| `/<short_code>` | **GET**  | Redirect to the original URL.   | `curl -i -X GET http://localhost:9080/cfvE0n4n `                                          |

---

### 🔒 C. Authenticated Endpoints (Token Required)

Use your token from `/auth/success` in the `Authorization` header:
`Authorization: Bearer [TOKEN]`

| Endpoint                         | Method     | Description                                                   | Example cURL                                                                                                                                                                                             |
| -------------------------------- | ---------- | ------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `/api/links`                     | **GET**    | Retrieve all links owned by the authenticated user.           | `curl -i -X GET http://localhost:9080/api/links -H "Authorization: Bearer [TOKEN]" `                                                                                                                |
| `/shorten?custom_code=testlink1` | **POST**   | Create an authenticated short link with a custom code.        | `curl -i -X POST 'http://localhost:9080/shorten?custom_code=testlink1' \ -H "Authorization: Bearer [TOKEN]" \ -H "Content-Type: application/json" \ -d '{"long_url": "https://private.site/123"}' ` |
| `/api/link/favourite`            | **POST**   | Mark or unmark a link as favorite.                            | `curl -i -X POST http://localhost:9080/api/link/favourite \ -H "Authorization: Bearer [TOKEN]" \ -H "Content-Type: application/json" \ -d '{"short_code": "testlink1", "is_favourite": true}' `     |
| `/api/link`                      | **DELETE** | Delete a specific short link by code.                         | `curl -i -X DELETE 'http://localhost:9080/api/link?code=testlink1' \ -H "Authorization: Bearer [TOKEN]" `                                                                                           |
| `/api/admin`                     | **GET**    | Admin-only access endpoint (User ID 1 is hardcoded as admin). | `curl -i -X GET http://localhost:9080/api/admin -H "Authorization: Bearer [TOKEN]" `                                                                                                                |

---

### 🎯 Example OAuth Success Page

After successful sign-in, the service renders a helpful HTML page showing your token and example cURL commands:

```
✅ Login Successful!

Your Bearer Token: eyJh...xyz123

Example Usage:
  curl -i -X GET http://localhost:9080/api/links \
       -H "Authorization: Bearer eyJh...xyz123"
```

---

**Note:**

* Tokens expire if the session becomes invalid or is removed from the database.
* The `/auth/success` endpoint checks the token validity against the MySQL session table before displaying it.
* `/api/admin` is restricted to the hardcoded Admin user ID (typically `1`).


## Security Notes

Session Expiration: For testing, sessions are set to expire after 1 day. After this time, all authenticated API calls will receive a 401 Unauthorized response, forcing the user to re-authenticate via /auth/google.

CSRF Protection: The server utilizes a state parameter check in handleGoogleCallback to prevent Cross-Site Request Forgery attacks during the OAuth handshake.