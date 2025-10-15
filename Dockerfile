# Dockerfile
# Use Debian-based Node image instead of Alpine for ESP32 toolchain compatibility
# Alpine uses musl libc while ESP32 toolchains are compiled for glibc
FROM node:22-slim

# Install build tools and PlatformIO dependencies
RUN apt-get update && apt-get install -y \
    bash \
    build-essential \
    git \
    python3 \
    python3-pip \
    python3-venv \
    udev \
    && rm -rf /var/lib/apt/lists/*

# Install PlatformIO
RUN pip3 install --break-system-packages platformio
RUN npm install -g @anthropic-ai/claude-code

# Create a working directory inside container
WORKDIR /workspace

# Default command - interactive shell
CMD ["bash"]
