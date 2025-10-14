# Dockerfile
FROM node:22-alpine

# Install build tools and PlatformIO dependencies
RUN apk add --no-cache \
    bash \
    g++ \
    make \
    python3 \
    py3-pip \
    git \
    udev

# Install PlatformIO
RUN pip3 install --break-system-packages platformio
RUN npm install -g @anthropic-ai/claude-code

# Create a working directory inside container
WORKDIR /workspace

# Default command - interactive shell
CMD ["bash"]
