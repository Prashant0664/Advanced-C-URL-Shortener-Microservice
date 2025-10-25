# ==========================================================
# Stage 1: Build environment with vcpkg + CMake
# ==========================================================
FROM mcr.microsoft.com/devcontainers/cpp:debian-11 AS builder

# Switch to root for installing deps
USER root

# Install Git and build tools
RUN apt-get update && apt-get install -y git curl unzip build-essential cmake pkg-config && rm -rf /var/lib/apt/lists/*

# Copy project files into the container
WORKDIR /app
COPY . .

# Set up vcpkg (inside container)
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg && /opt/vcpkg/bootstrap-vcpkg.sh

# Point CMake to vcpkg toolchain
ENV CMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake

# Install dependencies using vcpkg
RUN /opt/vcpkg/vcpkg install mysql-connector-cpp sentry-native cpp-httplib

# Configure and build project
RUN cmake -B build -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --config Release

# ==========================================================
# Stage 2: Minimal runtime image
# ==========================================================
FROM debian:bookworm-slim AS runtime

# Runtime dependencies compatible with Bookworm
RUN apt-get update && apt-get install -y \
    libssl3 libmysqlclient21 libstdc++6 libgcc-s1 libgomp1 \
    && rm -rf /var/lib/apt/lists/*

# Working directory
WORKDIR /usr/local/bin

# Copy executable from builder
COPY --from=builder /app/build/url_shortner /usr/local/bin/url_shortner

# Expose app port
EXPOSE 9080

# Entrypoint
ENTRYPOINT ["./url_shortner"]
