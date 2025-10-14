# Dockerfile
FROM node:22-alpine

# Install useful build tools (for native npm modules, optional)
RUN apk add --no-cache bash g++ make python3

# Create a working directory inside container
WORKDIR /workspace

# Default command - interactive shell
CMD ["bash"]
