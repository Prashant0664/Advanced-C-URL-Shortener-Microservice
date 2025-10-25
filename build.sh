#!/bin/bash
# Exit immediately if a command exits with a non-zero status.
set -e

echo "Starting C++ compilation..."

# Note: /usr/include/mysqlx is where the header files are installed by the connector package.
# We link against -lmysqlx -lssl -lcrypto -lpthread for MySQL/SSL support.
g++ -std=c++17 -Wall -Wextra \
    main.cpp Server.cpp URLShortnerDB.cpp Config.cpp \
    -o url_shortener \
    -lmysqlx -lssl -lcrypto -lpthread

echo "Compilation finished successfully."

# Ensure the executable has run permissions
chmod +x url_shortener
