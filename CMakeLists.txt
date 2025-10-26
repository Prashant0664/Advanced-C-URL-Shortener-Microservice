cmake_minimum_required(VERSION 3.15)

# vcpkg toolchain must be set BEFORE project()
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

project(URL_SHORTNER_2 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-std=c++17)

# --- Define Source Files (COMPLETE LIST) ---
add_executable(url_shortner
    main.cpp
    URLShortnerDB.cpp
    Config.cpp
    Server.cpp
    # Add other .cpp files here as needed
)

# --- Find Libraries (vcpkg managed) ---

# MySQL (Using the fixed unofficial prefix)
find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED) 

# Sentry
find_package(sentry CONFIG REQUIRED)

# FIX: Use the most likely correct vcpkg package name: 'httplib'
find_package(httplib CONFIG REQUIRED)


# --- Link Executable (Consolidated and Corrected) ---
target_link_libraries(url_shortner PRIVATE
    # MySQL target (fixed)
    unofficial::mysql-connector-cpp::connector 

    # Sentry target
    sentry::sentry
    
    # FIX: Link against the correct library target 'httplib::httplib'
    httplib::httplib
)

# --- Linux-specific Libraries for Networking and Threading ---
# These libraries are often required on Linux (Docker) for httplib's asynchronous
# and socket functionality (getaddrinfo_a, etc.).
if(NOT APPLE)
    target_link_libraries(url_shortner PRIVATE
        rt         # Real-Time library
        pthread    # POSIX threads
        resolv     # DNS resolver
    )
endif()


# --- macOS networking frameworks ---
# Required for networking functionality on Apple platforms.
if(APPLE)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(CFNETWORK_FRAMEWORK CFNetwork)
    target_link_libraries(url_shortner PRIVATE
        ${COREFOUNDATION_FRAMEWORK}
        ${CFNETWORK_FRAMEWORK}
    )
endif()