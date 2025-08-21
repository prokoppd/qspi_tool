# Use the latest Ubuntu LTS image
FROM ubuntu:24.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update and install build-essential and valgrind
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        valgrind \
        git \
        cmake \
        wget \
        curl \
        vim \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]

