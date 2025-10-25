# ==========================================================
# Stage 1: Build environment with vcpkg + CMake
# ==========================================================
FROM mcr.microsoft.com/devcontainers/cpp:debian-11 AS builder

# Switch to root for necessary system installs
USER root

# Install tools needed for Vcpkg cloning and installation (Minimized apt-get)
# The base image already has git, cmake, and build-essential.
RUN apt-get update && apt-get install -y curl unzip pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Copy project files into the container
WORKDIR /app
COPY . .

# Set up vcpkg (inside container)
# FIX 1: Clone vcpkg locally into /app/vcpkg and run as root to avoid permission errors
RUN git clone https://github.com/microsoft/vcpkg.git /app/vcpkg && \
    /app/vcpkg/bootstrap-vcpkg.sh

# Change Vcpkg ownership to the default non-root user (vscode) for security
RUN chown -R vscode:vscode /app/vcpkg

# Point CMake to vcpkg toolchain
ENV CMAKE_TOOLCHAIN_FILE=/app/vcpkg/scripts/buildsystems/vcpkg.cmake

# Switch to the non-root user for compilation (SECURITY BEST PRACTICE)
USER vscode

# Install dependencies using vcpkg (will create the build cache)
# This step requires CMakeLists.txt to be present, which it is in /app
RUN /app/vcpkg/vcpkg install mysql-connector-cpp sentry-native cpp-httplib

# Configure and build project (as non-root user)
RUN cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} \
    -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --config Release

# ==========================================================
# Stage 2: Minimal runtime image
# ==========================================================
FROM debian:bookworm-slim AS runtime

# Runtime dependencies compatible with Bookworm
# FIX: Replaced versioned package name with generic library links
RUN apt-get update && apt-get install -y \
    libssl3 \
    libmysqlclient-dev \
    libstdc++6 \
    libgcc-s1 \
    libgomp1 \
    && rm -rf /var/lib/apt/lists/*

# Set working directory to the final destination
WORKDIR /usr/local/bin

# Copy executable from builder
COPY --from=builder /app/build/url_shortner /usr/local/bin/

# Expose app port
EXPOSE 9080

# Entrypoint
ENTRYPOINT ["/usr/local/bin/url_shortner"]
